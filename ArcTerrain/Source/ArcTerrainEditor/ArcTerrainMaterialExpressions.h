// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MaterialValueType.h"
#include "Materials/MaterialExpression.h"
#include "ArcTerrainMaterialExpressions.generated.h"

class UArcTerrainChannelSet;
class UTexture;

/**
 * How one layer of UMaterialExpressionArcTerrainLayerBlend contributes to the blend. Mirrors the
 * engine ELandscapeLayerBlendType semantics (LB_WeightBlend / LB_AlphaBlend / LB_HeightBlend):
 *  - WeightBlend: the layer's raw channel weight joins the normalized weight-sum group.
 *  - AlphaBlend:  the layer is excluded from the weight sum and lerped over the accumulated output.
 *  - HeightBlend: like WeightBlend, but its weight is remapped by a per-layer height offset first.
 */
UENUM()
enum class EArcTerrainLayerBlendType : uint8
{
	WeightBlend,
	AlphaBlend,
	HeightBlend,
};

/**
 * One layer of a UMaterialExpressionArcTerrainLayerBlend. Field semantics mirror the engine
 * FLayerBlendInput: the layer's weight comes from a named terrain channel (sampled from the
 * tile atlas), LayerInput is the value blended in (falls back to ConstLayerInput when
 * unconnected), and HeightInput/ConstHeightInput drive the height offset for HeightBlend layers.
 */
USTRUCT()
struct FArcTerrainLayerBlendInput
{
	GENERATED_BODY()

	/** Terrain weight channel that drives this layer's contribution (resolved against ChannelSet). */
	UPROPERTY(EditAnywhere, Category = "ArcTerrain")
	FName Channel;

	/** How this layer contributes: weight / alpha / height blend (see EArcTerrainLayerBlendType). */
	UPROPERTY(EditAnywhere, Category = "ArcTerrain")
	EArcTerrainLayerBlendType BlendType = EArcTerrainLayerBlendType::WeightBlend;

	/** Value blended in for this layer; defaults to ConstLayerInput when the pin is not connected. */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstLayerInput' if not specified"))
	FExpressionInput LayerInput;

	/** Height offset (HeightBlend only); defaults to ConstHeightInput when the pin is not connected. */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstHeightInput' if not specified"))
	FExpressionInput HeightInput;

	/** Constant weight used for material-graph preview before any terrain MID binds real weights. */
	UPROPERTY(EditAnywhere, Category = "ArcTerrain")
	float PreviewWeight = 1.0f;

	/** Used only when LayerInput is not connected. */
	UPROPERTY(EditAnywhere, Category = "ArcTerrain")
	FVector ConstLayerInput = FVector(0.0f, 0.0f, 0.0f);

	/** Used only when HeightInput is not connected (HeightBlend only). */
	UPROPERTY(EditAnywhere, Category = "ArcTerrain")
	float ConstHeightInput = 0.0f;
};

/**
 * Material expression sampling the terrain's HEIGHT atlas (float1, terrain-local cm).
 *
 * NOT for displacement/WPO — the ArcTerrain vertex factory already displaces every vertex and derives
 * the normal per pixel from the same atlas, so terrain materials never wire Normal or World Position
 * Offset. This node is a plain graph read of that height for shading logic (height-based masks, tint).
 *
 * Emission: a TextureParameter "ArcTerrainHeightAtlas" (a Texture2DArray, bound by the terrain's
 * material instance), sampled at (face-local UV1 remapped to inner texel centers, height slice). The
 * SLICE comes from UV2.x, which the vertex factory resolved per leaf from the bisector's face id —
 * one draw covers many sectors, so no material parameter could carry it.
 *
 * Editor-only data: the whole ArcTerrainEditor module is editor; a cook strips these expressions.
 *
 */
UCLASS(MinimalAPI, CollapseCategories, HideCategories = Object)
class UMaterialExpressionArcTerrainHeight : public UMaterialExpression
{
	GENERATED_BODY()

public:
	UMaterialExpressionArcTerrainHeight(const FObjectInitializer& ObjectInitializer);

	//~ Begin UMaterialExpression interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void Build(MIR::FEmitter& Emitter) override;
	virtual EMaterialValueType GetOutputValueType(int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual bool CanReferenceTexture() const override { return true; }
	virtual UObject* GetReferencedTexture() const override;
	/** Declares ArcTerrainNodeVersion.ush so a C++ translator math change busts the material shader-map
	 *  DDC via ExpressionIncludesHash (the node emits no #include; this is a pure version-hash hook). */
	virtual void GetIncludeFilePaths(TSet<FString>& OutIncludeFilePaths) const override;
	//~ End UMaterialExpression interface

protected:
	/** Placeholder Texture2DArray the material compiles against until the terrain's material instance
	 *  binds the real height atlas array. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> PlaceholderTexture;
};

/**
 * Material expression sampling one named weight CHANNEL of the terrain (float1).
 *
 * The SLICE comes from UV2.y (the vertex factory's per-leaf weight slice) and the UV from UV0
 * (weight-group local), remapped to inner texel centers with "ArcTerrainWeightInvRes".
 *
 */
UCLASS(MinimalAPI, CollapseCategories, HideCategories = Object)
class UMaterialExpressionArcTerrainLayerWeight : public UMaterialExpression
{
	GENERATED_BODY()

public:
	UMaterialExpressionArcTerrainLayerWeight(const FObjectInitializer& ObjectInitializer);

	/** Weight channel this node samples; picker options come from ChannelSet (GetChannelOptions). */
	UPROPERTY(EditAnywhere, Category = "ArcTerrain", meta = (GetOptions = "GetChannelOptions", NoResetToDefault))
	FName Channel;

	/** Channel set defining the ordered weight channels — the same asset the terrain composites with. */
	UPROPERTY(EditAnywhere, Category = "ArcTerrain")
	TObjectPtr<UArcTerrainChannelSet> ChannelSet;

	/** GetOptions source for the Channel picker: the ChannelSet's channel names, or empty. */
	UFUNCTION(CallInEditor)
	TArray<FName> GetChannelOptions() const;

	//~ Begin UMaterialExpression interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void Build(MIR::FEmitter& Emitter) override;
	virtual EMaterialValueType GetOutputValueType(int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual bool CanReferenceTexture() const override { return true; }
	virtual UObject* GetReferencedTexture() const override;
	/** Declares ArcTerrainNodeVersion.ush so a C++ translator math change busts the material shader-map
	 *  DDC via ExpressionIncludesHash (the node emits no #include; this is a pure version-hash hook). */
	virtual void GetIncludeFilePaths(TSet<FString>& OutIncludeFilePaths) const override;
	//~ End UMaterialExpression interface

protected:
	/** Placeholder Texture2DArray the material compiles against until the terrain's material instance
	 *  binds the real weight atlas arrays. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> PlaceholderTexture;
};

/**
 * N-layer terrain material blend node — the ArcTerrain analogue of engine
 * UMaterialExpressionLandscapeLayerBlend, ported faithfully but for ArcTerrain's dual weight sources.
 * Each layer names a weight Channel whose value comes from the tile atlas
 * (UMaterialExpressionArcTerrainLayerWeight math).
 *
 * Blend math mirrors LandscapeLayerBlend's MCT_Float path: WeightBlend layers join a normalized
 * weight-sum group (InvWeightSum = 1/max(WeightSum, 1)); HeightBlend uses a height-modified weight
 * (ArcTerrainLayerBlendMath.h: ArcTerrainHeightBlendWeight); AlphaBlend layers are excluded from the
 * sum and lerped over the accumulated output last (ArcTerrainAlphaBlend). The pure-scalar numeric
 * definitions live in the RUNTIME header ArcTerrain/ArcTerrainLayerBlendMath.h (single source of truth,
 * mirrored here in compiler-calls); InvWeightSum is not factored because it operates on compiler node
 * indices, not floats.
 *
 * MaterialAttributes (Step 3): LayerInput accepts MCT_Float | MCT_MaterialAttributes. When the blended
 * output is (or contains) MaterialAttributes, Compile is re-invoked once per EMaterialProperty; the
 * non-ShadingModel properties accumulate the delta from the compiled material default
 * (Default + Sum((LayerCode − DefaultCode) * InvWeightSum * Weight)) so unconnected/zero-weight layers
 * fall back to the material default instead of black, and MP_ShadingModel does a discrete dominant-layer
 * pick (weight >= 0.5 AND greater than every other layer's weight) rather than a weighted sum. The modern
 * (non-legacy-CVar) rule is used throughout, matching the float path's max(sum,1) normalization.
 *
 * The inert per-layer PreviewWeight field is still not wired into the compiled weight.
 */
UCLASS(MinimalAPI, CollapseCategories, HideCategories = Object)
class UMaterialExpressionArcTerrainLayerBlend : public UMaterialExpression
{
	GENERATED_BODY()

public:
	UMaterialExpressionArcTerrainLayerBlend(const FObjectInitializer& ObjectInitializer);

	/** Channel set defining the ordered weight channels — the same asset the terrain composites with. */
	UPROPERTY(EditAnywhere, Category = "ArcTerrain")
	TObjectPtr<UArcTerrainChannelSet> ChannelSet;

	/** Ordered blend layers; each names a Channel and how it contributes (weight/alpha/height). */
	UPROPERTY(EditAnywhere, Category = "ArcTerrain")
	TArray<FArcTerrainLayerBlendInput> Layers;

	//~ Begin UObject interface
#if WITH_EDITOR
	/** Regenerates the graph node's input pins when the Layers array changes (mirrors the engine
	 *  LandscapeLayerBlend) — without this the details-panel array edit adds no visible LayerInput pin. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

	//~ Begin UMaterialExpression interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void Build(MIR::FEmitter& Emitter) override;
	virtual EMaterialValueType GetOutputValueType(int32 OutputIndex) override { return MCT_Unknown; }
	virtual EMaterialValueType GetInputValueType(int32 InputIndex) override;
	/** True when any connected LayerInput resolves to MaterialAttributes — makes the translator re-invoke
	 *  Compile once per EMaterialProperty (delta-from-default + ShadingModel pick paths). */
	virtual bool IsResultMaterialAttributes(int32 OutputIndex, TMaterialRecursionGuard<UMaterialExpression> RecursionGuard) override;
	UE_DEPRECATED_OVERRIDE(5.5, "Use FExpressionInputIterator instead or GetInput() directly.")
	virtual TArrayView<FExpressionInput*> GetInputsView() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FName GetInputName(int32 InputIndex) const override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual bool CanReferenceTexture() const override { return true; }
	virtual UObject* GetReferencedTexture() const override;
	virtual ReferencedTextureArray GetReferencedTextures() const override;
	/** Declares ArcTerrainNodeVersion.ush so a C++ translator math change busts the material shader-map
	 *  DDC via ExpressionIncludesHash (the node emits no #include; this is a pure version-hash hook). */
	virtual void GetIncludeFilePaths(TSet<FString>& OutIncludeFilePaths) const override;
	//~ End UMaterialExpression interface

protected:
	/** Placeholder Texture2DArray the node compiles against until the terrain's material instance binds
	 *  the real weight atlas arrays. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> PlaceholderTexture;
};
