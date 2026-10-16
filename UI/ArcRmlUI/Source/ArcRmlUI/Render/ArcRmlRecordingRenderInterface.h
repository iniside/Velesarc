// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/ArcRmlRenderTypes.h"

#include "RmlUi/Core/RenderInterface.h"
#include "RmlUi/Core/Types.h"

class FArcRmlFileInterface;
class IImageWrapperModule;

namespace ArcRmlTextureDecode
{
	bool DecodeFile(IImageWrapperModule& ImageWrapperModule, const FArcRmlFileInterface& FileInterface, const Rml::String& Source,
		Rml::Vector2i& OutDimensions, Rml::Vector<Rml::byte>& OutPixels);
}

class FArcRmlRecordingRenderInterface final : public Rml::RenderInterface
{
public:
	FArcRmlRecordingRenderInterface();

	void BeginFrame(FIntPoint InViewSize);
	TUniquePtr<FArcRmlRenderFrame> EndFrame();

	virtual Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> Vertices, Rml::Span<const int> Indices) override;
	virtual void RenderGeometry(Rml::CompiledGeometryHandle Geometry, Rml::Vector2f Translation, Rml::TextureHandle Texture) override;
	virtual void ReleaseGeometry(Rml::CompiledGeometryHandle Geometry) override;

	virtual Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> Source, Rml::Vector2i SourceDimensions) override;
	virtual Rml::TextureHandle GenerateAssetTexture(Rml::AssetTextureToken Token, Rml::Vector2i Dimensions) override;
	virtual void ReleaseTexture(Rml::TextureHandle Texture) override;

	virtual void EnableScissorRegion(bool bEnable) override;
	virtual void SetScissorRegion(Rml::Rectanglei Region) override;

	virtual void EnableClipMask(bool bEnable) override;
	virtual void RenderToClipMask(Rml::ClipMaskOperation Operation, Rml::CompiledGeometryHandle Geometry, Rml::Vector2f Translation) override;

	virtual void SetTransform(const Rml::Matrix4f* Transform) override;

	virtual Rml::LayerHandle PushLayer() override;
	virtual void CompositeLayers(Rml::LayerHandle Source, Rml::LayerHandle Destination, Rml::BlendMode BlendMode,
		Rml::Span<const Rml::CompiledFilterHandle> Filters) override;
	virtual void PopLayer() override;

	virtual Rml::TextureHandle SaveLayerAsTexture() override;
	virtual Rml::CompiledFilterHandle SaveLayerAsMaskImage() override;

	virtual Rml::CompiledFilterHandle CompileFilter(const Rml::String& Name, const Rml::Dictionary& Parameters) override;
	virtual void ReleaseFilter(Rml::CompiledFilterHandle Filter) override;

	virtual Rml::CompiledShaderHandle CompileShader(const Rml::String& Name, const Rml::Dictionary& Parameters) override;
	virtual void RenderShader(Rml::CompiledShaderHandle Shader, Rml::CompiledGeometryHandle Geometry, Rml::Vector2f Translation,
		Rml::TextureHandle Texture) override;
	virtual void ReleaseShader(Rml::CompiledShaderHandle Shader) override;

private:
	uint64 AllocateHandle();
	void AddResourceOp(EArcRmlResourceOpType Type, uint64 Handle, int32 PayloadIndex);
	FArcRmlDrawCommand& AddCommand(EArcRmlDrawOp Op);
	uint64 AddTexturePayload(FIntPoint Size, TArray<uint8>&& Pixels);

	TUniquePtr<FArcRmlRenderFrame> Frame;
	FIntPoint ViewSize = FIntPoint::ZeroValue;
	Rml::Matrix4f Projection;
	FIntRect Scissor;
	bool bScissorEnabled = false;
	bool bRecording = false;
	int32 LayerCount = 0;
	uint64 NextHandle = 1;
};
