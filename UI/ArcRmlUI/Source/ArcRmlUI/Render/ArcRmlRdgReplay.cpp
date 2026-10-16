// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcRmlRdgReplay.h"

#include "ArcRmlShaders.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "PixelShaderUtils.h"
#include "RHIStaticStates.h"
#include "Render/ArcRmlRenderResources.h"
#include "RenderGraphBuilder.h"
#include "RenderResource.h"
#include "RHITextureReference.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

BEGIN_SHADER_PARAMETER_STRUCT(FArcRmlRasterPassParameters, )
	RDG_TEXTURE_ACCESS_ARRAY(Textures)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

namespace ArcRmlRdgReplayPrivate
{
	constexpr EPixelFormat LayerFormat = PF_FloatRGBA;
	constexpr int32 MaxBlurPassLevel = 10;
	constexpr float MaxSinglePassSigma = 3.0f;

	FRHIBlendState* GetBlendState(EArcRmlBlendMode BlendMode)
	{
		switch (BlendMode)
		{
		case EArcRmlBlendMode::Blend:
			return TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
		case EArcRmlBlendMode::SourceIn:
			return TStaticBlendState<CW_RGBA, BO_Add, BF_DestAlpha, BF_Zero, BO_Add, BF_DestAlpha, BF_Zero>::GetRHI();
		case EArcRmlBlendMode::SourceOut:
			return TStaticBlendState<CW_RGBA, BO_Add, BF_InverseDestAlpha, BF_Zero, BO_Add, BF_InverseDestAlpha, BF_Zero>::GetRHI();
		case EArcRmlBlendMode::Xor:
			return TStaticBlendState<CW_RGBA, BO_Add, BF_InverseDestAlpha, BF_InverseSourceAlpha, BO_Add, BF_InverseDestAlpha, BF_InverseSourceAlpha>::GetRHI();
		case EArcRmlBlendMode::Replace:
			break;
		}
		return TStaticBlendState<>::GetRHI();
	}

	FRHIDepthStencilState* GetDepthStencilState(ArcRmlRdg::EStencilMode Mode)
	{
		switch (Mode)
		{
		case ArcRmlRdg::EStencilMode::Test:
			return TStaticDepthStencilState<false, CF_Always,
				true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				0xFF, 0x00>::GetRHI();
		case ArcRmlRdg::EStencilMode::Replace:
			return TStaticDepthStencilState<false, CF_Always,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				0xFF, 0xFF>::GetRHI();
		case ArcRmlRdg::EStencilMode::Increment:
			return TStaticDepthStencilState<false, CF_Always,
				true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
				true, CF_Always, SO_Keep, SO_Keep, SO_SaturatedIncrement,
				0xFF, 0xFF>::GetRHI();
		default:
			return TStaticDepthStencilState<false, CF_Always>::GetRHI();
		}
	}

	void SetSource(FArcRmlSourceParameters& Out, FRDGTextureRef Texture, FVector2f Scale, FVector2f Bias, const FIntRect& Region)
	{
		const FIntPoint Extent = Texture->Desc.Extent;
		const FVector2f InvSize(1.0f / static_cast<float>(Extent.X), 1.0f / static_cast<float>(Extent.Y));
		Out.SourceTexture = Texture;
		Out.SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Out.SourceScaleBias = FVector4f(Scale.X, Scale.Y, Bias.X, Bias.Y);
		Out.SourceInvSize = InvSize;
		Out.SourceRegion = FVector4f(
			(static_cast<float>(Region.Min.X) + 0.5f) * InvSize.X,
			(static_cast<float>(Region.Min.Y) + 0.5f) * InvSize.Y,
			(static_cast<float>(Region.Max.X) - 0.5f) * InvSize.X,
			(static_cast<float>(Region.Max.Y) - 0.5f) * InvSize.Y);
	}

	template <typename TShaderClass>
	void AddFullscreenPass(FRDGBuilder& GraphBuilder, FRDGEventName&& PassName, const TShaderRef<TShaderClass>& PixelShader,
		typename TShaderClass::FParameters* Parameters, const FIntRect& Viewport, const FIntRect& Scissor, FRHIBlendState* BlendState,
		FRHIDepthStencilState* DepthStencilState, uint32 StencilRef)
	{
		ClearUnusedGraphResources(PixelShader, Parameters);
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIShaderPlatform);
		GraphBuilder.AddPass(MoveTemp(PassName), Parameters, ERDGPassFlags::Raster,
			[Parameters, PixelShader, GlobalShaderMap, Viewport, Scissor, BlendState, DepthStencilState, StencilRef](FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(static_cast<float>(Viewport.Min.X), static_cast<float>(Viewport.Min.Y), 0.0f,
					static_cast<float>(Viewport.Max.X), static_cast<float>(Viewport.Max.Y), 1.0f);

				FGraphicsPipelineStateInitializer PipelineState;
				FPixelShaderUtils::InitFullscreenPipelineState(RHICmdList, GlobalShaderMap, PixelShader, PipelineState);
				PipelineState.BlendState = BlendState;
				PipelineState.DepthStencilState = DepthStencilState;
				SetGraphicsPipelineState(RHICmdList, PipelineState, StencilRef);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *Parameters);

				RHICmdList.SetScissorRect(true, Scissor.Min.X, Scissor.Min.Y, Scissor.Max.X, Scissor.Max.Y);
				FPixelShaderUtils::DrawFullscreenTriangle(RHICmdList);
				RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
			});
	}

	void SigmaToParameters(float DesiredSigma, int32& OutPassLevel, float& OutSigma)
	{
		const int32 Scaled = static_cast<int32>(DesiredSigma * (2.0f / MaxSinglePassSigma));
		OutPassLevel = Scaled > 0 ? FMath::Clamp(static_cast<int32>(FMath::FloorLog2(static_cast<uint32>(Scaled))), 0, MaxBlurPassLevel) : 0;
		OutSigma = FMath::Clamp(DesiredSigma / static_cast<float>(1 << OutPassLevel), 0.0f, MaxSinglePassSigma);
	}

	FVector4f ComputeBlurWeights(float Sigma)
	{
		float Weights[4];
		float Normalization = 0.0f;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (FMath::Abs(Sigma) < 0.1f)
			{
				Weights[Index] = Index == 0 ? 1.0f : 0.0f;
			}
			else
			{
				Weights[Index] = FMath::Exp(-static_cast<float>(Index * Index) / (2.0f * Sigma * Sigma)) / (FMath::Sqrt(2.0f * PI) * Sigma);
			}
			Normalization += (Index == 0 ? 1.0f : 2.0f) * Weights[Index];
		}
		return FVector4f(Weights[0], Weights[1], Weights[2], Weights[3]) / Normalization;
	}
}

FRDGTextureRef ArcRmlRdg::CreateLayerTexture(FRDGBuilder& GraphBuilder, FIntPoint Size, const TCHAR* Name)
{
	const FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(Size, ArcRmlRdgReplayPrivate::LayerFormat, FClearValueBinding::Transparent,
		ETextureCreateFlags::ShaderResource | ETextureCreateFlags::RenderTargetable);
	return GraphBuilder.CreateTexture(Desc, Name);
}

#if !UE_BUILD_SHIPPING
void ArcRmlRdg::FReplayStats::Append(const FReplayStats& Other)
{
	Passes += Other.Passes;
	Textures += Other.Textures;
	Blurs += Other.Blurs;
	BlurPasses += Other.BlurPasses;
}
#endif

bool ArcRmlRdg::AddCopyPass(FRDGBuilder& GraphBuilder, const FCopyPassDesc& Desc)
{
	if (Desc.Scissor.Area() <= 0)
	{
		return false;
	}

	const TShaderMapRef<FArcRmlCopyPS> PixelShader(GetGlobalShaderMap(GMaxRHIShaderPlatform));
	FArcRmlCopyPS::FParameters* Parameters = GraphBuilder.AllocParameters<FArcRmlCopyPS::FParameters>();
	ArcRmlRdgReplayPrivate::SetSource(Parameters->Source, Desc.Source, Desc.Scale, Desc.Bias, Desc.SourceRegion);
	Parameters->CopyOpacity = Desc.Opacity;
	Parameters->RenderTargets[0] = FRenderTargetBinding(Desc.Destination,
		Desc.bLoadDestination ? ERenderTargetLoadAction::ELoad : ERenderTargetLoadAction::ENoAction);

	FRHIDepthStencilState* DepthStencilState = ArcRmlRdgReplayPrivate::GetDepthStencilState(EStencilMode::None);
	if (Desc.StencilTexture)
	{
		Parameters->RenderTargets.DepthStencil = FDepthStencilBinding(Desc.StencilTexture, ERenderTargetLoadAction::ENoAction,
			ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthNop_StencilRead);
		DepthStencilState = ArcRmlRdgReplayPrivate::GetDepthStencilState(EStencilMode::Test);
	}

	ArcRmlRdgReplayPrivate::AddFullscreenPass(GraphBuilder, RDG_EVENT_NAME("ArcRmlCopy"), PixelShader, Parameters, Desc.Viewport, Desc.Scissor,
		ArcRmlRdgReplayPrivate::GetBlendState(Desc.BlendMode), DepthStencilState, Desc.StencilRef);
	return true;
}

FArcRmlRdgReplayer::FArcRmlRdgReplayer(FRDGBuilder& InGraphBuilder, FArcRmlRenderResources& InResources, const FArcRmlRenderFrame& InFrame)
	: GraphBuilder(InGraphBuilder)
	, Resources(InResources)
	, Frame(InFrame)
	, ViewSize(InFrame.ViewSize)
	, ShaderMap(GetGlobalShaderMap(GMaxRHIShaderPlatform))
{
}

void FArcRmlRdgReplayer::Replay(FRDGTextureRef BaseLayer)
{
	Layers.Reset();
	Layers.Add(BaseLayer);
	LayerCount = 1;

	const FRDGTextureDesc StencilDesc = FRDGTextureDesc::Create2D(ViewSize, PF_DepthStencil, FClearValueBinding::DepthZero,
		ETextureCreateFlags::DepthStencilTargetable);
	Stencil = GraphBuilder.CreateTexture(StencilDesc, TEXT("ArcRmlStencil"));
	AddClearDepthStencilPass(GraphBuilder, Stencil, ERenderTargetLoadAction::EClear, ERenderTargetLoadAction::EClear);
#if !UE_BUILD_SHIPPING
	++Stats.Textures;
	++Stats.Passes;
#endif

	for (const FArcRmlDrawCommand& Command : Frame.Commands)
	{
		switch (Command.Op)
		{
		case EArcRmlDrawOp::RenderGeometry:
			RenderGeometry(Command);
			break;
		case EArcRmlDrawOp::RenderShader:
			RenderShader(Command);
			break;
		case EArcRmlDrawOp::SetScissor:
			bScissorEnabled = Command.bEnable;
			Scissor = Command.Rect;
			break;
		case EArcRmlDrawOp::EnableClipMask:
			bClipMaskEnabled = Command.bEnable;
			break;
		case EArcRmlDrawOp::RenderToClipMask:
			RenderToClipMask(Command);
			break;
		case EArcRmlDrawOp::SetTransform:
			TransformIndex = Frame.Transforms.IsValidIndex(Command.Index) ? Command.Index : 0;
			break;
		case EArcRmlDrawOp::PushLayer:
			PushLayer();
			break;
		case EArcRmlDrawOp::PopLayer:
			PopLayer();
			break;
		case EArcRmlDrawOp::CompositeLayers:
			CompositeLayers(Command);
			break;
		case EArcRmlDrawOp::SaveLayerAsTexture:
			SaveLayerAsTexture(Command);
			break;
		case EArcRmlDrawOp::SaveLayerAsMaskImage:
			SaveLayerAsMaskImage(Command);
			break;
		}
	}

	FlushRaster();
}

#if !UE_BUILD_SHIPPING
const ArcRmlRdg::FReplayStats& FArcRmlRdgReplayer::GetStats() const
{
	return Stats;
}
#endif

FRDGTextureRef FArcRmlRdgReplayer::CreateTarget(const TCHAR* Name)
{
	ARCRMLUI_PERF(++Stats.Textures);
	return ArcRmlRdg::CreateLayerTexture(GraphBuilder, ViewSize, Name);
}

bool FArcRmlRdgReplayer::AddCopy(const ArcRmlRdg::FCopyPassDesc& Desc)
{
	const bool bAdded = ArcRmlRdg::AddCopyPass(GraphBuilder, Desc);
	ARCRMLUI_PERF(Stats.Passes += bAdded ? 1 : 0);
	return bAdded;
}

FRDGTextureRef FArcRmlRdgReplayer::GetTopLayer() const
{
	return Layers[LayerCount - 1];
}

FIntRect FArcRmlRdgReplayer::GetViewRect() const
{
	return FIntRect(FIntPoint::ZeroValue, ViewSize);
}

FIntRect FArcRmlRdgReplayer::GetScissorRect() const
{
	FIntRect Result = GetViewRect();
	if (bScissorEnabled)
	{
		Result.Clip(Scissor);
	}
	return Result;
}

bool FArcRmlRdgReplayer::ResolveTexture(uint64 Handle, ArcRmlRdg::FRasterItem& Item)
{
	const FArcRmlTextureResource* Resource = Resources.Textures.Find(Handle);
	if (!Resource)
	{
		return false;
	}

	if (Resource->AssetReference)
	{
		const FTextureReferenceRHIRef& Reference = Resource->AssetReference->TextureReferenceRHI;
		FRHITexture* Texture = Reference.IsValid() ? Reference->GetReferencedTexture() : nullptr;
		if (!Texture)
		{
			return false;
		}
		Item.AssetTexture = Texture;
		Item.TextureEncoding = EnumHasAnyFlags(Texture->GetDesc().Flags, ETextureCreateFlags::SRGB)
			? EArcRmlTextureEncoding::StraightLinear
			: EArcRmlTextureEncoding::Straight;
		return true;
	}

	if (!Resource->Pooled.IsValid())
	{
		return false;
	}
	Item.Texture = GraphBuilder.RegisterExternalTexture(Resource->Pooled);
	return true;
}

ArcRmlRdg::FRasterItem FArcRmlRdgReplayer::MakeItem(uint64 GeometryHandle, FVector2f Translation, bool& bOutValid)
{
	ArcRmlRdg::FRasterItem Item;
	const FArcRmlGeometryResource* Geometry = Resources.Geometries.Find(GeometryHandle);
	bOutValid = Geometry != nullptr && Geometry->NumIndices > 0;
	if (!bOutValid)
	{
		return Item;
	}

	Item.VertexBuffer = Geometry->VertexBuffer;
	Item.IndexBuffer = Geometry->IndexBuffer;
	Item.NumVertices = Geometry->NumVertices;
	Item.NumIndices = Geometry->NumIndices;
	Item.Transform = Frame.Transforms.IsValidIndex(TransformIndex) ? Frame.Transforms[TransformIndex] : FMatrix44f::Identity;
	Item.Translation = Translation;
	Item.Scissor = GetScissorRect();
	Item.StencilMode = bClipMaskEnabled ? ArcRmlRdg::EStencilMode::Test : ArcRmlRdg::EStencilMode::None;
	Item.StencilRef = StencilTestValue;
	return Item;
}

void FArcRmlRdgReplayer::AddItem(ArcRmlRdg::FRasterItem&& Item)
{
	if (Item.Scissor.Area() <= 0)
	{
		return;
	}

	const FRDGTextureRef Target = GetTopLayer();
	if (PendingTarget != Target)
	{
		FlushRaster();
		PendingTarget = Target;
	}
	PendingItems.Add(MoveTemp(Item));
}

void FArcRmlRdgReplayer::FlushRaster()
{
	if (PendingItems.IsEmpty())
	{
		PendingTarget = nullptr;
		return;
	}

	FArcRmlRasterPassParameters* Parameters = GraphBuilder.AllocParameters<FArcRmlRasterPassParameters>();
	Parameters->RenderTargets[0] = FRenderTargetBinding(PendingTarget, ERenderTargetLoadAction::ELoad);
	Parameters->RenderTargets.DepthStencil = FDepthStencilBinding(Stencil, ERenderTargetLoadAction::ENoAction, ERenderTargetLoadAction::ELoad,
		FExclusiveDepthStencil::DepthNop_StencilWrite);
	for (const ArcRmlRdg::FRasterItem& Item : PendingItems)
	{
		if (Item.Texture)
		{
			Parameters->Textures.AddUnique(FRDGTextureAccess(Item.Texture, ERHIAccess::SRVGraphics));
		}
	}

	const TShaderMapRef<FArcRmlGeometryVS> VertexShader(ShaderMap);
	const TShaderMapRef<FArcRmlColorPS> ColorShader(ShaderMap);
	const TShaderMapRef<FArcRmlTexturePS> TextureShader(ShaderMap);
	const TShaderMapRef<FArcRmlGradientPS> GradientShader(ShaderMap);
	FRHIVertexDeclaration* VertexDeclaration = Resources.GetVertexDeclaration();
	const FIntPoint PassViewSize = ViewSize;

	ARCRMLUI_PERF(++Stats.Passes);
	GraphBuilder.AddPass(RDG_EVENT_NAME("ArcRmlGeometry"), Parameters, ERDGPassFlags::Raster,
		[Items = MoveTemp(PendingItems), Gradients = MoveTemp(PendingGradients), PassViewSize, VertexShader, ColorShader, TextureShader,
			GradientShader, VertexDeclaration](FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, static_cast<float>(PassViewSize.X), static_cast<float>(PassViewSize.Y), 1.0f);

			for (const ArcRmlRdg::FRasterItem& Item : Items)
			{
				FGraphicsPipelineStateInitializer PipelineState;
				RHICmdList.ApplyCachedRenderTargets(PipelineState);
				PipelineState.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				PipelineState.BlendState = Item.bColorWrite
					? ArcRmlRdgReplayPrivate::GetBlendState(EArcRmlBlendMode::Blend)
					: TStaticBlendState<CW_NONE>::GetRHI();
				PipelineState.DepthStencilState = ArcRmlRdgReplayPrivate::GetDepthStencilState(Item.StencilMode);
				PipelineState.PrimitiveType = PT_TriangleList;
				PipelineState.BoundShaderState.VertexDeclarationRHI = VertexDeclaration;
				PipelineState.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				switch (Item.PixelKind)
				{
				case ArcRmlRdg::EPixelKind::Textured:
					PipelineState.BoundShaderState.PixelShaderRHI = TextureShader.GetPixelShader();
					break;
				case ArcRmlRdg::EPixelKind::Gradient:
					PipelineState.BoundShaderState.PixelShaderRHI = GradientShader.GetPixelShader();
					break;
				default:
					PipelineState.BoundShaderState.PixelShaderRHI = ColorShader.GetPixelShader();
					break;
				}
				SetGraphicsPipelineState(RHICmdList, PipelineState, Item.StencilRef);

				FArcRmlGeometryVS::FParameters VertexParameters;
				VertexParameters.GeometryTransform = Item.Transform;
				VertexParameters.GeometryTranslation = Item.Translation;
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VertexParameters);

				if (Item.PixelKind == ArcRmlRdg::EPixelKind::Textured)
				{
					FArcRmlTexturePS::FParameters PixelParameters;
					PixelParameters.GeometryTexture = Item.AssetTexture.IsValid() ? Item.AssetTexture.GetReference() : Item.Texture->GetRHI();
					PixelParameters.GeometrySampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
					PixelParameters.GeometryTextureEncoding = static_cast<uint32>(Item.TextureEncoding);
					SetShaderParameters(RHICmdList, TextureShader, TextureShader.GetPixelShader(), PixelParameters);
				}
				else if (Item.PixelKind == ArcRmlRdg::EPixelKind::Gradient && Gradients.IsValidIndex(Item.GradientIndex))
				{
					const FArcRmlShaderDesc& Gradient = Gradients[Item.GradientIndex];
					FArcRmlGradientPS::FParameters PixelParameters;
					PixelParameters.GradientFunction = static_cast<int32>(Gradient.Function);
					PixelParameters.GradientNumStops = Gradient.NumStops;
					PixelParameters.GradientP = Gradient.P;
					PixelParameters.GradientV = Gradient.V;
					for (int32 StopIndex = 0; StopIndex < ArcRmlShaders::MaxGradientStops; ++StopIndex)
					{
						const bool bValidStop = StopIndex < Gradient.NumStops;
						PixelParameters.GradientStopColors[StopIndex] = bValidStop ? Gradient.StopColors[StopIndex] : FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
						PixelParameters.GradientStopPositions[StopIndex / 4][StopIndex % 4] = bValidStop ? Gradient.StopPositions[StopIndex] : 0.0f;
					}
					SetShaderParameters(RHICmdList, GradientShader, GradientShader.GetPixelShader(), PixelParameters);
				}

				RHICmdList.SetScissorRect(true, Item.Scissor.Min.X, Item.Scissor.Min.Y, Item.Scissor.Max.X, Item.Scissor.Max.Y);
				RHICmdList.SetStreamSource(0, Item.VertexBuffer, 0);
				RHICmdList.DrawIndexedPrimitive(Item.IndexBuffer, 0, 0, Item.NumVertices, 0, Item.NumIndices / 3, 1);
			}

			RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
		});

	PendingItems.Reset();
	PendingGradients.Reset();
	PendingTarget = nullptr;
}

void FArcRmlRdgReplayer::RenderGeometry(const FArcRmlDrawCommand& Command)
{
	bool bValid = false;
	ArcRmlRdg::FRasterItem Item = MakeItem(Command.Geometry, Command.Translation, bValid);
	if (!bValid)
	{
		return;
	}

	if (Command.Texture != 0)
	{
		if (!ResolveTexture(Command.Texture, Item))
		{
			return;
		}
		Item.PixelKind = ArcRmlRdg::EPixelKind::Textured;
	}

	AddItem(MoveTemp(Item));
}

void FArcRmlRdgReplayer::RenderShader(const FArcRmlDrawCommand& Command)
{
	const FArcRmlShaderDesc* Shader = Resources.Shaders.Find(Command.Resource);
	if (!Shader)
	{
		return;
	}

	bool bValid = false;
	ArcRmlRdg::FRasterItem Item = MakeItem(Command.Geometry, Command.Translation, bValid);
	if (!bValid)
	{
		return;
	}

	if (PendingTarget != GetTopLayer())
	{
		FlushRaster();
	}
	Item.PixelKind = ArcRmlRdg::EPixelKind::Gradient;
	Item.GradientIndex = PendingGradients.Add(*Shader);
	AddItem(MoveTemp(Item));
}

void FArcRmlRdgReplayer::AddStencilClear(uint32 Value)
{
	const FArcRmlGeometryResource& Quad = Resources.GetFullscreenQuad(GraphBuilder.RHICmdList);
	ArcRmlRdg::FRasterItem Item;
	Item.VertexBuffer = Quad.VertexBuffer;
	Item.IndexBuffer = Quad.IndexBuffer;
	Item.NumVertices = Quad.NumVertices;
	Item.NumIndices = Quad.NumIndices;
	Item.Transform = FMatrix44f::Identity;
	Item.Scissor = GetScissorRect();
	Item.bColorWrite = false;
	Item.StencilMode = ArcRmlRdg::EStencilMode::Replace;
	Item.StencilRef = Value;
	AddItem(MoveTemp(Item));
}

void FArcRmlRdgReplayer::RenderToClipMask(const FArcRmlDrawCommand& Command)
{
	bool bValid = false;
	ArcRmlRdg::FRasterItem Item = MakeItem(Command.Geometry, Command.Translation, bValid);
	if (!bValid)
	{
		return;
	}

	Item.bColorWrite = false;
	switch (Command.ClipOperation)
	{
	case EArcRmlClipOperation::Set:
		AddStencilClear(0);
		Item.StencilMode = ArcRmlRdg::EStencilMode::Replace;
		Item.StencilRef = 1;
		StencilTestValue = 1;
		break;
	case EArcRmlClipOperation::SetInverse:
		AddStencilClear(1);
		Item.StencilMode = ArcRmlRdg::EStencilMode::Replace;
		Item.StencilRef = 0;
		StencilTestValue = 1;
		break;
	case EArcRmlClipOperation::Intersect:
		Item.StencilMode = ArcRmlRdg::EStencilMode::Increment;
		Item.StencilRef = 0;
		StencilTestValue += 1;
		break;
	}
	AddItem(MoveTemp(Item));
}

void FArcRmlRdgReplayer::PushLayer()
{
	FlushRaster();
	++LayerCount;
	const FRDGTextureRef Layer = CreateTarget(TEXT("ArcRmlLayer"));
	if (Layers.Num() < LayerCount)
	{
		Layers.Add(Layer);
	}
	else
	{
		Layers[LayerCount - 1] = Layer;
	}
	AddClearRenderTargetPass(GraphBuilder, Layer, FLinearColor::Transparent);
	ARCRMLUI_PERF(++Stats.Passes);
}

void FArcRmlRdgReplayer::PopLayer()
{
	FlushRaster();
	LayerCount = FMath::Max(1, LayerCount - 1);
}

void FArcRmlRdgReplayer::CompositeLayers(const FArcRmlDrawCommand& Command)
{
	FlushRaster();
	if (Command.SourceLayer < 0 || Command.SourceLayer >= LayerCount || Command.DestinationLayer < 0 || Command.DestinationLayer >= LayerCount)
	{
		return;
	}

	const FIntRect Region = GetScissorRect();
	if (Region.Area() <= 0)
	{
		return;
	}

	const FRDGTextureRef Destination = Layers[Command.DestinationLayer];
	FRDGTextureRef Current = Layers[Command.SourceLayer];
	for (int32 FilterIndex = 0; FilterIndex < Command.Count; ++FilterIndex)
	{
		const int32 HandleIndex = Command.Index + FilterIndex;
		if (!Frame.FilterHandles.IsValidIndex(HandleIndex))
		{
			break;
		}

		const uint64 Handle = Frame.FilterHandles[HandleIndex];
		if (const FRDGTextureRef* Mask = MaskImages.Find(Handle))
		{
			Current = ApplyMaskImage(Current, *Mask, Region);
		}
		else if (const FArcRmlFilterDesc* Filter = Resources.Filters.Find(Handle))
		{
			Current = ApplyFilter(Current, *Filter, Region);
		}
	}

	if (Current == Destination)
	{
		const FRDGTextureRef Copy = CreateTarget(TEXT("ArcRmlCompositeSource"));
		ArcRmlRdg::FCopyPassDesc CopyDesc;
		CopyDesc.Source = Current;
		CopyDesc.Destination = Copy;
		CopyDesc.SourceRegion = GetViewRect();
		CopyDesc.Viewport = GetViewRect();
		CopyDesc.Scissor = Region;
		CopyDesc.bLoadDestination = false;
		AddCopy(CopyDesc);
		Current = Copy;
	}

	ArcRmlRdg::FCopyPassDesc CompositeDesc;
	CompositeDesc.Source = Current;
	CompositeDesc.Destination = Destination;
	CompositeDesc.SourceRegion = GetViewRect();
	CompositeDesc.Viewport = GetViewRect();
	CompositeDesc.Scissor = Region;
	CompositeDesc.BlendMode = Command.BlendMode;
	CompositeDesc.StencilTexture = bClipMaskEnabled ? Stencil : nullptr;
	CompositeDesc.StencilRef = StencilTestValue;
	AddCopy(CompositeDesc);
}

void FArcRmlRdgReplayer::SaveLayerAsTexture(const FArcRmlDrawCommand& Command)
{
	FlushRaster();
	if (Resources.Textures.Contains(Command.Resource))
	{
		return;
	}

	const FIntRect Bounds = Command.Rect;
	if (Bounds.Area() <= 0)
	{
		return;
	}

	const FRDGTextureRef Saved = ArcRmlRdg::CreateLayerTexture(GraphBuilder, Bounds.Size(), TEXT("ArcRmlSavedLayer"));
	AddClearRenderTargetPass(GraphBuilder, Saved, FLinearColor::Transparent);
#if !UE_BUILD_SHIPPING
	++Stats.Textures;
	++Stats.Passes;
#endif

	FIntRect CopyRegion = Bounds;
	CopyRegion.Clip(GetViewRect());
	if (CopyRegion.Area() > 0)
	{
		AddCopyTexturePass(GraphBuilder, GetTopLayer(), Saved, CopyRegion.Min, CopyRegion.Min - Bounds.Min, CopyRegion.Size());
		ARCRMLUI_PERF(++Stats.Passes);
	}

	Resources.Textures.Add(Command.Resource, FArcRmlTextureResource{GraphBuilder.ConvertToExternalTexture(Saved), nullptr});
}

void FArcRmlRdgReplayer::SaveLayerAsMaskImage(const FArcRmlDrawCommand& Command)
{
	FlushRaster();
	MaskImages.Add(Command.Resource, GetTopLayer());
}

FRDGTextureRef FArcRmlRdgReplayer::ApplyFilter(FRDGTextureRef Input, const FArcRmlFilterDesc& Filter, const FIntRect& Region)
{
	switch (Filter.Type)
	{
	case EArcRmlFilterType::Opacity:
	{
		const FRDGTextureRef Output = CreateTarget(TEXT("ArcRmlOpacity"));
		ArcRmlRdg::FCopyPassDesc Desc;
		Desc.Source = Input;
		Desc.Destination = Output;
		Desc.SourceRegion = GetViewRect();
		Desc.Viewport = GetViewRect();
		Desc.Scissor = Region;
		Desc.Opacity = Filter.Opacity;
		Desc.bLoadDestination = false;
		AddCopy(Desc);
		return Output;
	}
	case EArcRmlFilterType::Blur:
		return ApplyBlur(Input, Filter.Sigma, Region);
	case EArcRmlFilterType::DropShadow:
		return ApplyDropShadow(Input, Filter, Region);
	case EArcRmlFilterType::ColorMatrix:
		return ApplyColorMatrix(Input, Filter.ColorMatrix, Region);
	}
	return Input;
}

FRDGTextureRef FArcRmlRdgReplayer::ApplyMaskImage(FRDGTextureRef Input, FRDGTextureRef Mask, const FIntRect& Region)
{
	const FRDGTextureRef Output = CreateTarget(TEXT("ArcRmlMasked"));
	const TShaderMapRef<FArcRmlBlendMaskPS> PixelShader(ShaderMap);
	FArcRmlBlendMaskPS::FParameters* Parameters = GraphBuilder.AllocParameters<FArcRmlBlendMaskPS::FParameters>();
	ArcRmlRdgReplayPrivate::SetSource(Parameters->Source, Input, FVector2f(1.0f, 1.0f), FVector2f::ZeroVector, GetViewRect());
	Parameters->MaskTexture = Mask;
	Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ENoAction);
	ARCRMLUI_PERF(++Stats.Passes);
	ArcRmlRdgReplayPrivate::AddFullscreenPass(GraphBuilder, RDG_EVENT_NAME("ArcRmlMaskImage"), PixelShader, Parameters, GetViewRect(), Region,
		ArcRmlRdgReplayPrivate::GetBlendState(EArcRmlBlendMode::Replace), ArcRmlRdgReplayPrivate::GetDepthStencilState(ArcRmlRdg::EStencilMode::None), 0);
	return Output;
}

FRDGTextureRef FArcRmlRdgReplayer::ApplyColorMatrix(FRDGTextureRef Input, const FMatrix44f& Matrix, const FIntRect& Region)
{
	const FRDGTextureRef Output = CreateTarget(TEXT("ArcRmlColorMatrix"));
	const TShaderMapRef<FArcRmlColorMatrixPS> PixelShader(ShaderMap);
	FArcRmlColorMatrixPS::FParameters* Parameters = GraphBuilder.AllocParameters<FArcRmlColorMatrixPS::FParameters>();
	ArcRmlRdgReplayPrivate::SetSource(Parameters->Source, Input, FVector2f(1.0f, 1.0f), FVector2f::ZeroVector, GetViewRect());
	Parameters->ColorMatrix = Matrix;
	Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ENoAction);
	ARCRMLUI_PERF(++Stats.Passes);
	ArcRmlRdgReplayPrivate::AddFullscreenPass(GraphBuilder, RDG_EVENT_NAME("ArcRmlColorMatrix"), PixelShader, Parameters, GetViewRect(), Region,
		ArcRmlRdgReplayPrivate::GetBlendState(EArcRmlBlendMode::Replace), ArcRmlRdgReplayPrivate::GetDepthStencilState(ArcRmlRdg::EStencilMode::None), 0);
	return Output;
}

void FArcRmlRdgReplayer::AddBlurPass(FRDGTextureRef Source, FRDGTextureRef Destination, const FIntRect& Region, FVector2f TexelOffset,
	const FVector4f& Weights)
{
	const TShaderMapRef<FArcRmlBlurPS> PixelShader(ShaderMap);
	FArcRmlBlurPS::FParameters* Parameters = GraphBuilder.AllocParameters<FArcRmlBlurPS::FParameters>();
	ArcRmlRdgReplayPrivate::SetSource(Parameters->Source, Source, FVector2f(1.0f, 1.0f), FVector2f::ZeroVector, Region);
	Parameters->BlurTexelOffset = TexelOffset;
	Parameters->BlurWeights = Weights;
	Parameters->RenderTargets[0] = FRenderTargetBinding(Destination, ERenderTargetLoadAction::ENoAction);
	ARCRMLUI_PERF(++Stats.Passes);
	ArcRmlRdgReplayPrivate::AddFullscreenPass(GraphBuilder, RDG_EVENT_NAME("ArcRmlBlur"), PixelShader, Parameters, GetViewRect(), Region,
		ArcRmlRdgReplayPrivate::GetBlendState(EArcRmlBlendMode::Replace), ArcRmlRdgReplayPrivate::GetDepthStencilState(ArcRmlRdg::EStencilMode::None), 0);
}

FRDGTextureRef FArcRmlRdgReplayer::ApplyBlur(FRDGTextureRef Input, float Sigma, const FIntRect& Region)
{
#if !UE_BUILD_SHIPPING
	++Stats.Blurs;
	const int32 PassesBeforeBlur = Stats.Passes;
#endif
	int32 PassLevel = 0;
	float PassSigma = 0.0f;
	ArcRmlRdgReplayPrivate::SigmaToParameters(Sigma, PassLevel, PassSigma);

	FRDGTextureRef Current = Input;
	FIntRect CurrentRegion = Region;
	for (int32 Level = 0; Level < PassLevel; ++Level)
	{
		FIntRect NextRegion;
		NextRegion.Min = (CurrentRegion.Min + FIntPoint(1, 1)) / 2;
		NextRegion.Max = (CurrentRegion.Max / 2).ComponentMax(NextRegion.Min);
		if (NextRegion.Area() <= 0)
		{
			PassLevel = Level;
			break;
		}

		const FRDGTextureRef Downsampled = CreateTarget(TEXT("ArcRmlBlurDownsample"));
		ArcRmlRdg::FCopyPassDesc Desc;
		Desc.Source = Current;
		Desc.Destination = Downsampled;
		Desc.SourceRegion = CurrentRegion;
		Desc.Viewport = GetViewRect();
		Desc.Scissor = NextRegion;
		Desc.Scale = FVector2f(2.0f, 2.0f);
		Desc.bLoadDestination = false;
		AddCopy(Desc);

		Current = Downsampled;
		CurrentRegion = NextRegion;
	}

	const FVector4f Weights = ArcRmlRdgReplayPrivate::ComputeBlurWeights(PassSigma);
	const FVector2f InvSize(1.0f / static_cast<float>(ViewSize.X), 1.0f / static_cast<float>(ViewSize.Y));

	const FRDGTextureRef Vertical = CreateTarget(TEXT("ArcRmlBlurVertical"));
	AddBlurPass(Current, Vertical, CurrentRegion, FVector2f(0.0f, InvSize.Y), Weights);

	const FRDGTextureRef Horizontal = CreateTarget(TEXT("ArcRmlBlurHorizontal"));
	AddBlurPass(Vertical, Horizontal, CurrentRegion, FVector2f(InvSize.X, 0.0f), Weights);

	if (PassLevel == 0)
	{
		ARCRMLUI_PERF(Stats.BlurPasses += Stats.Passes - PassesBeforeBlur);
		return Horizontal;
	}

	const FRDGTextureRef Upsampled = CreateTarget(TEXT("ArcRmlBlurUpsample"));
	const float UpScale = 1.0f / static_cast<float>(1 << PassLevel);
	ArcRmlRdg::FCopyPassDesc Desc;
	Desc.Source = Horizontal;
	Desc.Destination = Upsampled;
	Desc.SourceRegion = CurrentRegion;
	Desc.Viewport = GetViewRect();
	Desc.Scissor = Region;
	Desc.Scale = FVector2f(UpScale, UpScale);
	Desc.bLoadDestination = false;
	AddCopy(Desc);
	ARCRMLUI_PERF(Stats.BlurPasses += Stats.Passes - PassesBeforeBlur);
	return Upsampled;
}

FRDGTextureRef FArcRmlRdgReplayer::ApplyDropShadow(FRDGTextureRef Input, const FArcRmlFilterDesc& Filter, const FIntRect& Region)
{
	FRDGTextureRef Shadow = CreateTarget(TEXT("ArcRmlDropShadow"));
	ARCRMLUI_PERF(++Stats.Passes);
	const TShaderMapRef<FArcRmlDropShadowPS> PixelShader(ShaderMap);
	FArcRmlDropShadowPS::FParameters* Parameters = GraphBuilder.AllocParameters<FArcRmlDropShadowPS::FParameters>();
	ArcRmlRdgReplayPrivate::SetSource(Parameters->Source, Input, FVector2f(1.0f, 1.0f), -Filter.Offset, Region);
	Parameters->ShadowColor = Filter.Color;
	Parameters->RenderTargets[0] = FRenderTargetBinding(Shadow, ERenderTargetLoadAction::ENoAction);
	ArcRmlRdgReplayPrivate::AddFullscreenPass(GraphBuilder, RDG_EVENT_NAME("ArcRmlDropShadow"), PixelShader, Parameters, GetViewRect(), Region,
		ArcRmlRdgReplayPrivate::GetBlendState(EArcRmlBlendMode::Replace), ArcRmlRdgReplayPrivate::GetDepthStencilState(ArcRmlRdg::EStencilMode::None), 0);

	if (Filter.Sigma >= 0.5f)
	{
		Shadow = ApplyBlur(Shadow, Filter.Sigma, Region);
	}

	ArcRmlRdg::FCopyPassDesc Desc;
	Desc.Source = Input;
	Desc.Destination = Shadow;
	Desc.SourceRegion = GetViewRect();
	Desc.Viewport = GetViewRect();
	Desc.Scissor = Region;
	Desc.BlendMode = EArcRmlBlendMode::Blend;
	AddCopy(Desc);
	return Shadow;
}
