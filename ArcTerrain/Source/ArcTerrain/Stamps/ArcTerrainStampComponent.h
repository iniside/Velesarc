// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "ArcTerrainTypes.h"
#include "Assets/ArcTerrainPatchAsset.h" // EArcTerrainSourceChannel
#include "ArcTerrainStampComponent.generated.h"

class AArcTerrain;
class UArcTerrainSubsystem;
class UTexture2D;
class FSceneView;
class FPrimitiveDrawInterface;

/** Where a stamp source's raster data comes from. */
enum class EArcStampSourceKind : uint8
{
	/** No source — e.g. a stamp with weight layers but no height contribution. */
	None,
	/** UTexture2D — uploaded from FTextureSource (editor-only data; lossless, pre-BCn). */
	Texture,
	/** Baked float field — provider fills a TArray<float> + dims on demand (game thread). */
	BakedField
};

/**
 * One named weight layer a stamp composites into a terrain channel. The pairing FName (in
 * FArcStampSourceDesc::Layers) is the TERRAIN CHANNEL name the layer writes to; this struct is the
 * layer's raster source + decode + optional carve (signed height) contribution.
 */
struct FArcStampLayerDesc
{
	EArcStampSourceKind Kind = EArcStampSourceKind::None;

	/** Content identity of the raster — keys the GPU source cache. Texture: FTextureSource::GetId();
	 *  baked: FArcTerrainBakedField::ContentId (regenerated per bake). */
	FGuid ContentId;

	/** Set when Kind == Texture. */
	UTexture2D* Texture = nullptr;

	/** Actual raster dimensions. Available without invoking BakedProvider and used by the native
	 *  density resolver for both texture and baked-field sources. */
	FIntPoint SourceDims = FIntPoint::ZeroValue;

	/** Set when Kind == BakedField: fills the row-major float buffer + dims; returns false on failure.
	 *  Called on the game thread only, at most once per ContentId (source cache). */
	TFunction<bool(TArray<float>&, FIntPoint&)> BakedProvider;

	/** Texel channel to decode from (Texture kind; baked fields are single-channel). */
	EArcTerrainSourceChannel SourceChannel = EArcTerrainSourceChannel::R;

	/** weight = (texel - ZeroInEncoding) * EncodingScale, decoded in the composite shader. */
	float EncodingScale = 1.0f;
	float ZeroInEncoding = 0.0f;

	/** Fold this weight layer into height as a signed additive delta (carve). */
	bool bContributeToHeight = false;

	/** Signed height (cm, stamp-local) of a full-strength texel; negative carves. */
	float HeightAmplitude = 0.0f;
};

/**
 * Everything the subsystem needs to composite one stamp: the height raster source + decode, the
 * weight layers (terrain channel name -> layer source), and the falloff mask parameters. Filled by
 * the concrete stamp's GetSourceDescriptor (Step 5); consumed on the game thread when a dirty cell
 * composites.
 */
struct FArcStampSourceDesc
{
	// --- Height source (Kind == None -> no height pass; weight layers may still composite) ---

	EArcStampSourceKind Kind = EArcStampSourceKind::None;
	FGuid ContentId;
	UTexture2D* Texture = nullptr;
	FIntPoint SourceDims = FIntPoint::ZeroValue;
	TFunction<bool(TArray<float>&, FIntPoint&)> BakedProvider;
	EArcTerrainSourceChannel SourceChannel = EArcTerrainSourceChannel::R;

	/** height (cm, stamp-local) = (texel - ZeroInEncoding) * EncodingScale, decoded in-shader. */
	float EncodingScale = 1.0f;
	float ZeroInEncoding = 0.0f;

	/** Height only: ZeroInEncoding comes from the resolved source's border-ring mean, not the field above. */
	bool bDeriveZeroFromBorder = false;

	/** Sampled BILINEAR on both the CPU and the GPU rail — never bicubic, whose negative Catmull-Rom
	 *  weights leave [0,1] — at each consuming value raster's patch UV, so its dims MUST equal the
	 *  dims of every layer that consumes it (the height source and/or each weight layer); a dims
	 *  mismatch against a given layer drops the mask for that layer only. */
	EArcStampSourceKind MaskKind = EArcStampSourceKind::None;
	FGuid MaskContentId;
	UTexture2D* MaskTexture = nullptr;
	FIntPoint MaskSourceDims = FIntPoint::ZeroValue;
	TFunction<bool(TArray<float>&, FIntPoint&)> MaskBakedProvider;
	EArcTerrainSourceChannel MaskSourceChannel = EArcTerrainSourceChannel::R;

	// --- Weight layers: terrain channel name -> layer source ---

	TArray<TPair<FName, FArcStampLayerDesc>> Layers;

	// --- Falloff masks ---

	/** Rect edge falloff band width (cm, patch-local); 0 = hard cutoff at the footprint edge. */
	float EdgeFalloffBand = 0.0f;

	/** Pow exponent of the edge falloff ramp. */
	float EdgeSharpness = 1.0f;

	/** Corner rounding radius of the rect edge mask (cm, patch-local); 0 = square corners. */
	float CornerRadius = 0.0f;

	/** Additionally multiply by a circle / inscribed-ellipse smoothstep mask. */
	bool bCircleMask = false;

	/** Circle center offset in patch-local cm. */
	FVector2D CircleCenterOffset = FVector2D::ZeroVector;

	/** Explicit circle radius (cm); 0 = ellipse inscribed in the footprint. */
	float CircleRadius = 0.0f;

	/** Circle falloff band width (cm); 0 = hard circle edge. */
	float CircleFalloffBand = 0.0f;
};

/**
 * Base class for terrain stamp components. Concrete stamps (Step 5) provide the raster sources via
 * GetSourceDescriptor; this base owns terrain binding, blend settings, the terrain-local footprint,
 * subsystem registration and the SINGLE invalidation routing point (NotifyStampChanged ->
 * UArcTerrainSubsystem::OnStampChanged(OldBounds, NewBounds)).
 */
UCLASS(Abstract, ClassGroup = (ArcTerrain), HideCategories = (Physics, Collision, Lighting, Navigation, Mobility, LOD, TextureStreaming))
class ARCTERRAIN_API UArcTerrainStampComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UArcTerrainStampComponent();

	/** Terrain this stamp composites into. Soft so a stamp can live in a sublevel. */
	UPROPERTY(EditAnywhere, Category = "Stamp")
	TSoftObjectPtr<AArcTerrain> AffectedTerrain;

	/** Composite order: lower priority composites first. Ties break on the order anchor path, then the stamp path. */
	UPROPERTY(EditAnywhere, Category = "Stamp")
	double Priority = 100.0;

	/** How the stamp's height blends against the already-composited surface. */
	UPROPERTY(EditAnywhere, Category = "Stamp")
	EArcTerrainBlendMode BlendMode = EArcTerrainBlendMode::Additive;

	/** Blend extent (cm) of the SmoothMin/SmoothMax used by the Min/Max modes; 0 = hard min/max. */
	UPROPERTY(EditAnywhere, Category = "Stamp", meta = (ClampMin = "0.0", ForceUnits = "cm", EditConditionHides,
		EditCondition = "BlendMode == EArcTerrainBlendMode::Min || BlendMode == EArcTerrainBlendMode::Max"))
	float MinMaxSoftness = 0.0f;

	/** Excluded from compositing (its cells recomposite without it). */
	UPROPERTY(EditAnywhere, Category = "Stamp")
	bool bIsDisabled = false;

	/** Stamp footprint extent (cm) in the component's local XY, centered on the component. */
	UPROPERTY(EditAnywhere, Category = "Stamp", meta = (ClampMin = "1.0", Units = "cm"))
	FVector2D CoverageXY = FVector2D(10000.0, 10000.0);

	/** TERRAIN-LOCAL footprint AABB: the CoverageXY rectangle (z = 0 plane, component-local)
	 *  transformed by the component transform relative to the affected terrain's frame. Invalid box
	 *  when AffectedTerrain doesn't resolve. Cell partitioning ignores Z, so a z = 0 slab is exact. */
	virtual FBox ComputeBounds() const;

	/** Fill the raster sources + falloff for compositing. Return false to skip this stamp. */
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const PURE_VIRTUAL(UArcTerrainStampComponent::GetSourceDescriptor, return false;);
	virtual const UArcTerrainStampComponent* GetCompositeOrderAnchor() const { return this; }

	virtual void OnAffectedTerrainConfigChanged() {}

#if WITH_EDITOR
	/**
	 * Editor-only footprint visualization: a wire box of ComputeBounds() (terrain-local) transformed
	 * to world via AffectedTerrain->GetTerrainTransform(), plus a circle in the stamp plane. Concrete
	 * stamps (Step 5) may extend this to draw falloff-mask-specific detail. Driven by
	 * FArcTerrainStampVisualizer (ArcTerrainEditor, Step 8).
	 */
	virtual void DrawStampVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) const;
#endif

	/**
	 * SINGLE invalidation routing point: every mutation funnels here. Routes
	 * (LastNotifiedBounds, ComputeBounds()) to UArcTerrainSubsystem::OnStampChanged so both the
	 * old and the new footprint's cells recomposite, then updates LastNotifiedBounds.
	 */
	void NotifyStampChanged();

	void NotifyAffectedTerrainChanged();

	/** Channel names of the affected terrain — GetOptions source for channel pickers (Step 5). */
	UFUNCTION(CallInEditor)
	TArray<FName> GetTerrainChannels() const;

	/** The terrain this stamp registered against (resolved AffectedTerrain), if any. */
	AArcTerrain* GetResolvedTerrain() const { return ResolvedTerrain.Get(); }

	//~ Begin UActorComponent / USceneComponent interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditComponentMove(bool bFinished) override;
	virtual void PostEditUndo() override;
#endif
	//~ End UActorComponent / USceneComponent interface

private:
	UArcTerrainSubsystem* GetTerrainSubsystem() const;

	void RegisterWithSubsystem();
	void UnregisterFromSubsystem();

	/** Terrain the subsystem registration is held against (may differ from AffectedTerrain mid-edit). */
	TWeakObjectPtr<AArcTerrain> ResolvedTerrain;

	/** Footprint reported by the last registration/notify — the "old bounds" of the next change. */
	FBox LastNotifiedBounds;

	bool bRegisteredWithSubsystem = false;
};
