// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcRmlRecordingRenderInterface.h"

#include "ArcRmlUILog.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/ArcRmlFileInterface.h"
#include "Interfaces/ArcRmlStrings.h"

#include "RmlUi/Core/DecorationTypes.h"
#include "RmlUi/Core/Dictionary.h"
#include "RmlUi/Core/Math.h"
#include "RmlUi/Core/Vertex.h"

#include <type_traits>

static_assert(sizeof(FArcRmlVertex) == sizeof(Rml::Vertex), "FArcRmlVertex must mirror Rml::Vertex");
static_assert(std::is_same_v<Rml::Matrix4f, Rml::ColumnMajorMatrix4f>, "Matrix conversion assumes column-major RmlUi matrices");

namespace ArcRmlRecordingPrivate
{
	FMatrix44f ToShaderMatrix(const Rml::Matrix4f& Matrix)
	{
		FMatrix44f Result;
		FMemory::Memcpy(&Result.M[0][0], Matrix.data(), sizeof(float) * 16);
		return Result;
	}

	FIntRect ToIntRect(const Rml::Rectanglei& Rect)
	{
		return FIntRect(Rect.Left(), Rect.Top(), Rect.Right(), Rect.Bottom());
	}

	FVector4f ToColor(const Rml::ColourbPremultiplied& Colour)
	{
		return FVector4f(Colour.red / 255.0f, Colour.green / 255.0f, Colour.blue / 255.0f, Colour.alpha / 255.0f);
	}
}

bool ArcRmlTextureDecode::DecodeFile(IImageWrapperModule& ImageWrapperModule, const FArcRmlFileInterface& FileInterface, const Rml::String& Source,
	Rml::Vector2i& OutDimensions, Rml::Vector<Rml::byte>& OutPixels)
{
	Rml::String FileData;
	if (!FileInterface.ReadFileBytes(Source, FileData) || FileData.empty())
	{
		UE_LOGF(LogArcRmlUI, Warning, "RmlUi texture '%ls' could not be read", *ArcRmlStrings::ToFString(Source));
		return false;
	}

	const EImageFormat Format = ImageWrapperModule.DetectImageFormat(FileData.data(), static_cast<int64>(FileData.size()));
	if (Format == EImageFormat::Invalid)
	{
		UE_LOGF(LogArcRmlUI, Warning, "RmlUi texture '%ls' has an unsupported image format", *ArcRmlStrings::ToFString(Source));
		return false;
	}

	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);
	TArray<uint8> Pixels;
	if (!ImageWrapper.IsValid()
		|| !ImageWrapper->SetCompressed(FileData.data(), static_cast<int64>(FileData.size()))
		|| !ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, Pixels))
	{
		UE_LOGF(LogArcRmlUI, Warning, "RmlUi texture '%ls' could not be decoded", *ArcRmlStrings::ToFString(Source));
		return false;
	}

	for (int32 PixelIndex = 0; PixelIndex + 3 < Pixels.Num(); PixelIndex += 4)
	{
		const uint32 Alpha = Pixels[PixelIndex + 3];
		Pixels[PixelIndex + 0] = static_cast<uint8>((Pixels[PixelIndex + 0] * Alpha) / 255);
		Pixels[PixelIndex + 1] = static_cast<uint8>((Pixels[PixelIndex + 1] * Alpha) / 255);
		Pixels[PixelIndex + 2] = static_cast<uint8>((Pixels[PixelIndex + 2] * Alpha) / 255);
	}

	OutDimensions = Rml::Vector2i(static_cast<int>(ImageWrapper->GetWidth()), static_cast<int>(ImageWrapper->GetHeight()));
	OutPixels.assign(Pixels.GetData(), Pixels.GetData() + Pixels.Num());
	return true;
}

FArcRmlRecordingRenderInterface::FArcRmlRecordingRenderInterface()
	: Frame(MakeUnique<FArcRmlRenderFrame>())
	, Projection(Rml::Matrix4f::Identity())
{
}

void FArcRmlRecordingRenderInterface::BeginFrame(FIntPoint InViewSize)
{
	ViewSize = InViewSize;
	Projection = Rml::Matrix4f::ProjectOrtho(0.0f, static_cast<float>(ViewSize.X), static_cast<float>(ViewSize.Y), 0.0f, -10000.0f, 10000.0f);
	Frame->ViewSize = ViewSize;
	Frame->Commands.Reset();
	Frame->Transforms.Reset();
	Frame->FilterHandles.Reset();
	Frame->Transforms.Add(ArcRmlRecordingPrivate::ToShaderMatrix(Projection));
	Frame->bSavesLayerTextures = false;
	bScissorEnabled = false;
	LayerCount = 1;
	bRecording = true;
}

TUniquePtr<FArcRmlRenderFrame> FArcRmlRecordingRenderInterface::EndFrame()
{
	bRecording = false;
	TUniquePtr<FArcRmlRenderFrame> Finished = MoveTemp(Frame);
	Frame = MakeUnique<FArcRmlRenderFrame>();
	return Finished;
}

uint64 FArcRmlRecordingRenderInterface::AllocateHandle()
{
	return NextHandle++;
}

void FArcRmlRecordingRenderInterface::AddResourceOp(EArcRmlResourceOpType Type, uint64 Handle, int32 PayloadIndex)
{
	FArcRmlResourceOp& Op = Frame->ResourceOps.AddDefaulted_GetRef();
	Op.Type = Type;
	Op.Handle = Handle;
	Op.PayloadIndex = PayloadIndex;
}

FArcRmlDrawCommand& FArcRmlRecordingRenderInterface::AddCommand(EArcRmlDrawOp Op)
{
	FArcRmlDrawCommand& Command = Frame->Commands.AddDefaulted_GetRef();
	Command.Op = Op;
	return Command;
}

uint64 FArcRmlRecordingRenderInterface::AddTexturePayload(FIntPoint Size, TArray<uint8>&& Pixels)
{
	const uint64 Handle = AllocateHandle();
	const int32 PayloadIndex = Frame->Textures.Num();
	FArcRmlTexturePayload& Payload = Frame->Textures.AddDefaulted_GetRef();
	Payload.Size = Size;
	Payload.Pixels = MoveTemp(Pixels);
	AddResourceOp(EArcRmlResourceOpType::CreateTexture, Handle, PayloadIndex);
	return Handle;
}

Rml::CompiledGeometryHandle FArcRmlRecordingRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> Vertices, Rml::Span<const int> Indices)
{
	if (Vertices.empty() || Indices.empty())
	{
		return 0;
	}

	const uint64 Handle = AllocateHandle();
	const int32 PayloadIndex = Frame->Geometries.Num();
	FArcRmlGeometryPayload& Payload = Frame->Geometries.AddDefaulted_GetRef();
	Payload.Vertices.SetNumUninitialized(static_cast<int32>(Vertices.size()));
	FMemory::Memcpy(Payload.Vertices.GetData(), Vertices.data(), sizeof(Rml::Vertex) * Vertices.size());
	Payload.Indices.SetNumUninitialized(static_cast<int32>(Indices.size()));
	FMemory::Memcpy(Payload.Indices.GetData(), Indices.data(), sizeof(int) * Indices.size());
	AddResourceOp(EArcRmlResourceOpType::CreateGeometry, Handle, PayloadIndex);
	return static_cast<Rml::CompiledGeometryHandle>(Handle);
}

void FArcRmlRecordingRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle Geometry, Rml::Vector2f Translation, Rml::TextureHandle Texture)
{
	if (!bRecording)
	{
		return;
	}

	FArcRmlDrawCommand& Command = AddCommand(EArcRmlDrawOp::RenderGeometry);
	Command.Geometry = Geometry;
	Command.Texture = Texture;
	Command.Translation = FVector2f(Translation.x, Translation.y);
}

void FArcRmlRecordingRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle Geometry)
{
	AddResourceOp(EArcRmlResourceOpType::ReleaseGeometry, Geometry, INDEX_NONE);
}

Rml::TextureHandle FArcRmlRecordingRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> Source, Rml::Vector2i SourceDimensions)
{
	if (SourceDimensions.x <= 0 || SourceDimensions.y <= 0 || Source.size() != static_cast<size_t>(SourceDimensions.x * SourceDimensions.y * 4))
	{
		return 0;
	}

	TArray<uint8> Pixels;
	Pixels.SetNumUninitialized(static_cast<int32>(Source.size()));
	FMemory::Memcpy(Pixels.GetData(), Source.data(), Source.size());
	return static_cast<Rml::TextureHandle>(AddTexturePayload(FIntPoint(SourceDimensions.x, SourceDimensions.y), MoveTemp(Pixels)));
}

Rml::TextureHandle FArcRmlRecordingRenderInterface::GenerateAssetTexture(Rml::AssetTextureToken Token, Rml::Vector2i Dimensions)
{
	const uint64 Handle = AllocateHandle();
	const int32 PayloadIndex = Frame->AssetTextures.Add(reinterpret_cast<const FTextureReference*>(Token));
	AddResourceOp(EArcRmlResourceOpType::CreateAssetTexture, Handle, PayloadIndex);
	return static_cast<Rml::TextureHandle>(Handle);
}

void FArcRmlRecordingRenderInterface::ReleaseTexture(Rml::TextureHandle Texture)
{
	AddResourceOp(EArcRmlResourceOpType::ReleaseTexture, Texture, INDEX_NONE);
}

void FArcRmlRecordingRenderInterface::EnableScissorRegion(bool bEnable)
{
	if (bEnable || !bRecording)
	{
		return;
	}

	bScissorEnabled = false;
	FArcRmlDrawCommand& Command = AddCommand(EArcRmlDrawOp::SetScissor);
	Command.bEnable = false;
}

void FArcRmlRecordingRenderInterface::SetScissorRegion(Rml::Rectanglei Region)
{
	if (!bRecording)
	{
		return;
	}

	bScissorEnabled = Region.Valid();
	Scissor = ArcRmlRecordingPrivate::ToIntRect(Region);
	FArcRmlDrawCommand& Command = AddCommand(EArcRmlDrawOp::SetScissor);
	Command.bEnable = bScissorEnabled;
	Command.Rect = Scissor;
}

void FArcRmlRecordingRenderInterface::EnableClipMask(bool bEnable)
{
	if (!bRecording)
	{
		return;
	}

	FArcRmlDrawCommand& Command = AddCommand(EArcRmlDrawOp::EnableClipMask);
	Command.bEnable = bEnable;
}

void FArcRmlRecordingRenderInterface::RenderToClipMask(Rml::ClipMaskOperation Operation, Rml::CompiledGeometryHandle Geometry, Rml::Vector2f Translation)
{
	if (!bRecording)
	{
		return;
	}

	FArcRmlDrawCommand& Command = AddCommand(EArcRmlDrawOp::RenderToClipMask);
	Command.Geometry = Geometry;
	Command.Translation = FVector2f(Translation.x, Translation.y);
	switch (Operation)
	{
	case Rml::ClipMaskOperation::Set:
		Command.ClipOperation = EArcRmlClipOperation::Set;
		break;
	case Rml::ClipMaskOperation::SetInverse:
		Command.ClipOperation = EArcRmlClipOperation::SetInverse;
		break;
	case Rml::ClipMaskOperation::Intersect:
		Command.ClipOperation = EArcRmlClipOperation::Intersect;
		break;
	}
}

void FArcRmlRecordingRenderInterface::SetTransform(const Rml::Matrix4f* Transform)
{
	if (!bRecording)
	{
		return;
	}

	FArcRmlDrawCommand& Command = AddCommand(EArcRmlDrawOp::SetTransform);
	if (Transform)
	{
		Command.Index = Frame->Transforms.Add(ArcRmlRecordingPrivate::ToShaderMatrix(Projection * (*Transform)));
	}
	else
	{
		Command.Index = 0;
	}
}

Rml::LayerHandle FArcRmlRecordingRenderInterface::PushLayer()
{
	if (!bRecording)
	{
		return 0;
	}

	AddCommand(EArcRmlDrawOp::PushLayer);
	++LayerCount;
	return static_cast<Rml::LayerHandle>(LayerCount - 1);
}

void FArcRmlRecordingRenderInterface::CompositeLayers(Rml::LayerHandle Source, Rml::LayerHandle Destination, Rml::BlendMode BlendMode,
	Rml::Span<const Rml::CompiledFilterHandle> Filters)
{
	if (!bRecording)
	{
		return;
	}

	FArcRmlDrawCommand& Command = AddCommand(EArcRmlDrawOp::CompositeLayers);
	Command.SourceLayer = static_cast<int32>(Source);
	Command.DestinationLayer = static_cast<int32>(Destination);
	switch (BlendMode)
	{
	case Rml::BlendMode::Blend:
		Command.BlendMode = EArcRmlBlendMode::Blend;
		break;
	case Rml::BlendMode::Replace:
		Command.BlendMode = EArcRmlBlendMode::Replace;
		break;
	case Rml::BlendMode::SourceIn:
		Command.BlendMode = EArcRmlBlendMode::SourceIn;
		break;
	case Rml::BlendMode::SourceOut:
		Command.BlendMode = EArcRmlBlendMode::SourceOut;
		break;
	case Rml::BlendMode::Xor:
		Command.BlendMode = EArcRmlBlendMode::Xor;
		break;
	}
	Command.Index = Frame->FilterHandles.Num();
	Command.Count = static_cast<int32>(Filters.size());
	for (const Rml::CompiledFilterHandle Filter : Filters)
	{
		Frame->FilterHandles.Add(Filter);
	}
}

void FArcRmlRecordingRenderInterface::PopLayer()
{
	if (!bRecording)
	{
		return;
	}

	AddCommand(EArcRmlDrawOp::PopLayer);
	LayerCount = FMath::Max(1, LayerCount - 1);
}

Rml::TextureHandle FArcRmlRecordingRenderInterface::SaveLayerAsTexture()
{
	if (!bRecording || !bScissorEnabled || Scissor.Area() <= 0)
	{
		return 0;
	}

	const uint64 Handle = AllocateHandle();
	FArcRmlDrawCommand& Command = AddCommand(EArcRmlDrawOp::SaveLayerAsTexture);
	Command.Resource = Handle;
	Command.Rect = Scissor;
	Frame->bSavesLayerTextures = true;
	return static_cast<Rml::TextureHandle>(Handle);
}

Rml::CompiledFilterHandle FArcRmlRecordingRenderInterface::SaveLayerAsMaskImage()
{
	if (!bRecording)
	{
		return 0;
	}

	const uint64 Handle = AllocateHandle();
	FArcRmlDrawCommand& Command = AddCommand(EArcRmlDrawOp::SaveLayerAsMaskImage);
	Command.Resource = Handle;
	return static_cast<Rml::CompiledFilterHandle>(Handle);
}

Rml::CompiledFilterHandle FArcRmlRecordingRenderInterface::CompileFilter(const Rml::String& Name, const Rml::Dictionary& Parameters)
{
	FArcRmlFilterDesc Filter;
	bool bValid = true;

	if (Name == "opacity")
	{
		Filter.Type = EArcRmlFilterType::Opacity;
		Filter.Opacity = Rml::Get(Parameters, "value", 1.0f);
	}
	else if (Name == "blur")
	{
		Filter.Type = EArcRmlFilterType::Blur;
		Filter.Sigma = Rml::Get(Parameters, "sigma", 1.0f);
	}
	else if (Name == "drop-shadow")
	{
		Filter.Type = EArcRmlFilterType::DropShadow;
		Filter.Sigma = Rml::Get(Parameters, "sigma", 0.0f);
		const Rml::Vector4f Color = Rml::Get(Parameters, "color", Rml::Vector4f(0.0f));
		Filter.Color = FVector4f(Color.x, Color.y, Color.z, Color.w);
		const Rml::Vector2f Offset = Rml::Get(Parameters, "offset", Rml::Vector2f(0.0f));
		Filter.Offset = FVector2f(Offset.x, Offset.y);
	}
	else if (Name == "brightness")
	{
		Filter.Type = EArcRmlFilterType::ColorMatrix;
		const float Value = Rml::Get(Parameters, "value", 1.0f);
		Filter.ColorMatrix = ArcRmlRecordingPrivate::ToShaderMatrix(Rml::Matrix4f::Diag(Value, Value, Value, 1.0f));
	}
	else if (Name == "contrast")
	{
		Filter.Type = EArcRmlFilterType::ColorMatrix;
		const float Value = Rml::Get(Parameters, "value", 1.0f);
		const float Grayness = 0.5f - 0.5f * Value;
		Rml::Matrix4f Matrix = Rml::Matrix4f::Diag(Value, Value, Value, 1.0f);
		Matrix.SetColumn(3, Rml::Vector4f(Grayness, Grayness, Grayness, 1.0f));
		Filter.ColorMatrix = ArcRmlRecordingPrivate::ToShaderMatrix(Matrix);
	}
	else if (Name == "invert")
	{
		Filter.Type = EArcRmlFilterType::ColorMatrix;
		const float Value = Rml::Math::Clamp(Rml::Get(Parameters, "value", 1.0f), 0.0f, 1.0f);
		const float Inverted = 1.0f - 2.0f * Value;
		Rml::Matrix4f Matrix = Rml::Matrix4f::Diag(Inverted, Inverted, Inverted, 1.0f);
		Matrix.SetColumn(3, Rml::Vector4f(Value, Value, Value, 1.0f));
		Filter.ColorMatrix = ArcRmlRecordingPrivate::ToShaderMatrix(Matrix);
	}
	else if (Name == "grayscale")
	{
		Filter.Type = EArcRmlFilterType::ColorMatrix;
		const float Value = Rml::Get(Parameters, "value", 1.0f);
		const float Rev = 1.0f - Value;
		const Rml::Vector3f Gray = Value * Rml::Vector3f(0.2126f, 0.7152f, 0.0722f);
		Filter.ColorMatrix = ArcRmlRecordingPrivate::ToShaderMatrix(Rml::Matrix4f::FromRows(
			{Gray.x + Rev, Gray.y, Gray.z, 0.0f},
			{Gray.x, Gray.y + Rev, Gray.z, 0.0f},
			{Gray.x, Gray.y, Gray.z + Rev, 0.0f},
			{0.0f, 0.0f, 0.0f, 1.0f}));
	}
	else if (Name == "sepia")
	{
		Filter.Type = EArcRmlFilterType::ColorMatrix;
		const float Value = Rml::Get(Parameters, "value", 1.0f);
		const float Rev = 1.0f - Value;
		const Rml::Vector3f RMix = Value * Rml::Vector3f(0.393f, 0.769f, 0.189f);
		const Rml::Vector3f GMix = Value * Rml::Vector3f(0.349f, 0.686f, 0.168f);
		const Rml::Vector3f BMix = Value * Rml::Vector3f(0.272f, 0.534f, 0.131f);
		Filter.ColorMatrix = ArcRmlRecordingPrivate::ToShaderMatrix(Rml::Matrix4f::FromRows(
			{RMix.x + Rev, RMix.y, RMix.z, 0.0f},
			{GMix.x, GMix.y + Rev, GMix.z, 0.0f},
			{BMix.x, BMix.y, BMix.z + Rev, 0.0f},
			{0.0f, 0.0f, 0.0f, 1.0f}));
	}
	else if (Name == "hue-rotate")
	{
		Filter.Type = EArcRmlFilterType::ColorMatrix;
		const float Value = Rml::Get(Parameters, "value", 1.0f);
		const float S = Rml::Math::Sin(Value);
		const float C = Rml::Math::Cos(Value);
		Filter.ColorMatrix = ArcRmlRecordingPrivate::ToShaderMatrix(Rml::Matrix4f::FromRows(
			{0.213f + 0.787f * C - 0.213f * S, 0.715f - 0.715f * C - 0.715f * S, 0.072f - 0.072f * C + 0.928f * S, 0.0f},
			{0.213f - 0.213f * C + 0.143f * S, 0.715f + 0.285f * C + 0.140f * S, 0.072f - 0.072f * C - 0.283f * S, 0.0f},
			{0.213f - 0.213f * C - 0.787f * S, 0.715f - 0.715f * C + 0.715f * S, 0.072f + 0.928f * C + 0.072f * S, 0.0f},
			{0.0f, 0.0f, 0.0f, 1.0f}));
	}
	else if (Name == "saturate")
	{
		Filter.Type = EArcRmlFilterType::ColorMatrix;
		const float Value = Rml::Get(Parameters, "value", 1.0f);
		Filter.ColorMatrix = ArcRmlRecordingPrivate::ToShaderMatrix(Rml::Matrix4f::FromRows(
			{0.213f + 0.787f * Value, 0.715f - 0.715f * Value, 0.072f - 0.072f * Value, 0.0f},
			{0.213f - 0.213f * Value, 0.715f + 0.285f * Value, 0.072f - 0.072f * Value, 0.0f},
			{0.213f - 0.213f * Value, 0.715f - 0.715f * Value, 0.072f + 0.928f * Value, 0.0f},
			{0.0f, 0.0f, 0.0f, 1.0f}));
	}
	else
	{
		bValid = false;
	}

	if (!bValid)
	{
		UE_LOGF(LogArcRmlUI, Warning, "Unsupported RmlUi filter '%ls'", *ArcRmlStrings::ToFString(Name));
		return 0;
	}

	const uint64 Handle = AllocateHandle();
	const int32 PayloadIndex = Frame->Filters.Add(Filter);
	AddResourceOp(EArcRmlResourceOpType::CreateFilter, Handle, PayloadIndex);
	return static_cast<Rml::CompiledFilterHandle>(Handle);
}

void FArcRmlRecordingRenderInterface::ReleaseFilter(Rml::CompiledFilterHandle Filter)
{
	AddResourceOp(EArcRmlResourceOpType::ReleaseFilter, Filter, INDEX_NONE);
}

Rml::CompiledShaderHandle FArcRmlRecordingRenderInterface::CompileShader(const Rml::String& Name, const Rml::Dictionary& Parameters)
{
	FArcRmlShaderDesc Shader;
	const bool bRepeating = Rml::Get(Parameters, "repeating", false);

	if (Name == "linear-gradient")
	{
		Shader.Function = bRepeating ? EArcRmlGradientFunction::RepeatingLinear : EArcRmlGradientFunction::Linear;
		const Rml::Vector2f P0 = Rml::Get(Parameters, "p0", Rml::Vector2f(0.0f));
		const Rml::Vector2f P1 = Rml::Get(Parameters, "p1", Rml::Vector2f(0.0f));
		Shader.P = FVector2f(P0.x, P0.y);
		Shader.V = FVector2f(P1.x - P0.x, P1.y - P0.y);
	}
	else if (Name == "radial-gradient")
	{
		Shader.Function = bRepeating ? EArcRmlGradientFunction::RepeatingRadial : EArcRmlGradientFunction::Radial;
		const Rml::Vector2f Center = Rml::Get(Parameters, "center", Rml::Vector2f(0.0f));
		const Rml::Vector2f Radius = Rml::Get(Parameters, "radius", Rml::Vector2f(1.0f));
		Shader.P = FVector2f(Center.x, Center.y);
		Shader.V = FVector2f(1.0f / Radius.x, 1.0f / Radius.y);
	}
	else if (Name == "conic-gradient")
	{
		Shader.Function = bRepeating ? EArcRmlGradientFunction::RepeatingConic : EArcRmlGradientFunction::Conic;
		const Rml::Vector2f Center = Rml::Get(Parameters, "center", Rml::Vector2f(0.0f));
		const float Angle = Rml::Get(Parameters, "angle", 0.0f);
		Shader.P = FVector2f(Center.x, Center.y);
		Shader.V = FVector2f(Rml::Math::Cos(Angle), Rml::Math::Sin(Angle));
	}
	else
	{
		UE_LOGF(LogArcRmlUI, Warning, "Unsupported RmlUi shader '%ls'", *ArcRmlStrings::ToFString(Name));
		return 0;
	}

	const Rml::Dictionary::const_iterator StopsIt = Parameters.find("color_stop_list");
	if (StopsIt == Parameters.end() || StopsIt->second.GetType() != Rml::Variant::COLORSTOPLIST)
	{
		return 0;
	}

	const Rml::ColorStopList& Stops = StopsIt->second.GetReference<Rml::ColorStopList>();
	Shader.NumStops = FMath::Min(static_cast<int32>(Stops.size()), ArcRmlShaders::MaxGradientStops);
	for (int32 StopIndex = 0; StopIndex < Shader.NumStops; ++StopIndex)
	{
		const Rml::ColorStop& Stop = Stops[StopIndex];
		Shader.StopPositions[StopIndex] = Stop.position.number;
		Shader.StopColors[StopIndex] = ArcRmlRecordingPrivate::ToColor(Stop.color);
	}

	if (Shader.NumStops == 0)
	{
		return 0;
	}

	const uint64 Handle = AllocateHandle();
	const int32 PayloadIndex = Frame->Shaders.Add(Shader);
	AddResourceOp(EArcRmlResourceOpType::CreateShader, Handle, PayloadIndex);
	return static_cast<Rml::CompiledShaderHandle>(Handle);
}

void FArcRmlRecordingRenderInterface::RenderShader(Rml::CompiledShaderHandle Shader, Rml::CompiledGeometryHandle Geometry, Rml::Vector2f Translation,
	Rml::TextureHandle Texture)
{
	if (!bRecording)
	{
		return;
	}

	FArcRmlDrawCommand& Command = AddCommand(EArcRmlDrawOp::RenderShader);
	Command.Resource = Shader;
	Command.Geometry = Geometry;
	Command.Texture = Texture;
	Command.Translation = FVector2f(Translation.x, Translation.y);
}

void FArcRmlRecordingRenderInterface::ReleaseShader(Rml::CompiledShaderHandle Shader)
{
	AddResourceOp(EArcRmlResourceOpType::ReleaseShader, Shader, INDEX_NONE);
}
