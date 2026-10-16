// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcRmlUIPerf.h"
#include "RenderGraphDefinitions.h"
#include "RHIResources.h"
#include "Render/ArcRmlRenderTypes.h"

class FArcRmlRenderResources;
class FGlobalShaderMap;
class FRDGBuilder;

namespace ArcRmlRdg
{
	enum class EStencilMode : uint8
	{
		None,
		Test,
		Replace,
		Increment
	};

	enum class EPixelKind : uint8
	{
		Color,
		Textured,
		Gradient
	};

	struct FRasterItem
	{
		EPixelKind PixelKind = EPixelKind::Color;
		EStencilMode StencilMode = EStencilMode::None;
		bool bColorWrite = true;
		uint32 StencilRef = 0;
		FBufferRHIRef VertexBuffer;
		FBufferRHIRef IndexBuffer;
		uint32 NumVertices = 0;
		uint32 NumIndices = 0;
		FRDGTexture* Texture = nullptr;
		FTextureRHIRef AssetTexture;
		EArcRmlTextureEncoding TextureEncoding = EArcRmlTextureEncoding::Premultiplied;
		FMatrix44f Transform = FMatrix44f::Identity;
		FVector2f Translation = FVector2f::ZeroVector;
		FIntRect Scissor;
		int32 GradientIndex = INDEX_NONE;
	};

	struct FCopyPassDesc
	{
		FRDGTextureRef Source = nullptr;
		FRDGTextureRef Destination = nullptr;
		FIntRect SourceRegion;
		FIntRect Viewport;
		FIntRect Scissor;
		FVector2f Scale = FVector2f(1.0f, 1.0f);
		FVector2f Bias = FVector2f::ZeroVector;
		float Opacity = 1.0f;
		EArcRmlBlendMode BlendMode = EArcRmlBlendMode::Replace;
		bool bLoadDestination = true;
		FRDGTextureRef StencilTexture = nullptr;
		uint32 StencilRef = 0;
	};

#if !UE_BUILD_SHIPPING
	struct FReplayStats
	{
		int32 Passes = 0;
		int32 Textures = 0;
		int32 Blurs = 0;
		int32 BlurPasses = 0;

		void Append(const FReplayStats& Other);
	};
#endif

	FRDGTextureRef CreateLayerTexture(FRDGBuilder& GraphBuilder, FIntPoint Size, const TCHAR* Name);
	bool AddCopyPass(FRDGBuilder& GraphBuilder, const FCopyPassDesc& Desc);
}

class FArcRmlRdgReplayer
{
public:
	FArcRmlRdgReplayer(FRDGBuilder& InGraphBuilder, FArcRmlRenderResources& InResources, const FArcRmlRenderFrame& InFrame);

	void Replay(FRDGTextureRef BaseLayer);
#if !UE_BUILD_SHIPPING
	const ArcRmlRdg::FReplayStats& GetStats() const;
#endif

private:
	FRDGTextureRef CreateTarget(const TCHAR* Name);
	bool AddCopy(const ArcRmlRdg::FCopyPassDesc& Desc);
	FRDGTextureRef GetTopLayer() const;
	FIntRect GetScissorRect() const;
	FIntRect GetViewRect() const;
	bool ResolveTexture(uint64 Handle, ArcRmlRdg::FRasterItem& Item);

	ArcRmlRdg::FRasterItem MakeItem(uint64 GeometryHandle, FVector2f Translation, bool& bOutValid);
	void AddItem(ArcRmlRdg::FRasterItem&& Item);
	void FlushRaster();

	void RenderGeometry(const FArcRmlDrawCommand& Command);
	void RenderShader(const FArcRmlDrawCommand& Command);
	void RenderToClipMask(const FArcRmlDrawCommand& Command);
	void AddStencilClear(uint32 Value);
	void PushLayer();
	void PopLayer();
	void CompositeLayers(const FArcRmlDrawCommand& Command);
	void SaveLayerAsTexture(const FArcRmlDrawCommand& Command);
	void SaveLayerAsMaskImage(const FArcRmlDrawCommand& Command);

	FRDGTextureRef ApplyFilter(FRDGTextureRef Input, const FArcRmlFilterDesc& Filter, const FIntRect& Region);
	FRDGTextureRef ApplyMaskImage(FRDGTextureRef Input, FRDGTextureRef Mask, const FIntRect& Region);
	FRDGTextureRef ApplyBlur(FRDGTextureRef Input, float Sigma, const FIntRect& Region);
	FRDGTextureRef ApplyDropShadow(FRDGTextureRef Input, const FArcRmlFilterDesc& Filter, const FIntRect& Region);
	FRDGTextureRef ApplyColorMatrix(FRDGTextureRef Input, const FMatrix44f& Matrix, const FIntRect& Region);
	void AddBlurPass(FRDGTextureRef Source, FRDGTextureRef Destination, const FIntRect& Region, FVector2f TexelOffset, const FVector4f& Weights);

	FRDGBuilder& GraphBuilder;
	FArcRmlRenderResources& Resources;
	const FArcRmlRenderFrame& Frame;
	FIntPoint ViewSize;
	FGlobalShaderMap* ShaderMap = nullptr;

	TArray<FRDGTextureRef> Layers;
	int32 LayerCount = 0;
	FRDGTextureRef Stencil = nullptr;
	FIntRect Scissor;
	bool bScissorEnabled = false;
	int32 TransformIndex = 0;
	bool bClipMaskEnabled = false;
	uint32 StencilTestValue = 1;
	TMap<uint64, FRDGTextureRef> MaskImages;

	TArray<ArcRmlRdg::FRasterItem> PendingItems;
	TArray<FArcRmlShaderDesc> PendingGradients;
	FRDGTextureRef PendingTarget = nullptr;
#if !UE_BUILD_SHIPPING
	ArcRmlRdg::FReplayStats Stats;
#endif
};
