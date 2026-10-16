// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Assets/ArcTerrainPatchAsset.h" // EArcTerrainSourceChannel, UArcTerrainPatchAsset
#include "ArcTerrainPatchStamp.generated.h"

class UTexture2D;

/** Source of the height + weight rasters stamped by UArcTerrainPatchStamp. */
UENUM()
enum class EArcTerrainPatchSourceMode : uint8
{
	/** Height + weight textures configured directly on the stamp. */
	DirectTextures,
	/** Height + named weight layers pulled from a UArcTerrainPatchAsset. */
	DataAsset
};

/**
 * One weight texture stamped directly on the stamp (Direct mode). weight = (texel - ZeroInEncoding) * EncodingScale.
 * Runtime parallel of FArcTexturePatchDirectWeight (ArcEditorTools) — TargetChannel is a plain terrain
 * channel FName, populated from GetTerrainChannels(); the carve fields are consumed.
 */
USTRUCT()
struct FArcTerrainPatchDirectWeight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Source|Direct")
	TObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, Category = "Source|Direct")
	EArcTerrainSourceChannel SourceChannel = EArcTerrainSourceChannel::R;

	/** weight = (texel - ZeroInEncoding) * EncodingScale. */
	UPROPERTY(EditAnywhere, Category = "Source|Direct")
	float EncodingScale = 1.0f;

	/** weight = (texel - ZeroInEncoding) * EncodingScale. */
	UPROPERTY(EditAnywhere, Category = "Source|Direct")
	float ZeroInEncoding = 0.0f;

	/** Terrain channel this texture paints into (GetOptions resolves on the owning stamp). */
	UPROPERTY(EditAnywhere, Category = "Source|Direct", meta = (GetOptions = "GetTerrainChannels"))
	FName TargetChannel;

	/** Fold this weightmap into real height: +raises, -carves. Consumed (carve). */
	UPROPERTY(EditAnywhere, Category = "Weight|Geometry")
	bool bContributeToHeight = false;

	/** Signed height (cm) of a full-strength feature. Negative carves. Consumed (carve). */
	UPROPERTY(EditAnywhere, Category = "Weight|Geometry",
		meta = (ForceUnits = "cm", EditCondition = "bContributeToHeight"))
	float HeightAmplitude = 0.0f;
};

/**
 * Terrain stamp that composites a premade heightmap texture plus weightmap textures into mesh terrain,
 * with two source modes (EArcTerrainPatchSourceMode). Targets UArcTerrainPatchAsset. Height + weight
 * rasters upload through the subsystem's editor-only FTextureSource path, so GetSourceDescriptor
 * produces sources only in editor builds.
 */
UCLASS(ClassGroup = (ArcTerrain), meta = (BlueprintSpawnableComponent, DisplayName = "Terrain Patch Stamp"))
class ARCTERRAIN_API UArcTerrainPatchStamp : public UArcTerrainStampComponent
{
	GENERATED_BODY()

public:
	// --- Source ---

	/** Whether the textures come from properties on this stamp (Direct) or a UArcTerrainPatchAsset (Data Asset). */
	UPROPERTY(EditAnywhere, Category = "Source")
	EArcTerrainPatchSourceMode SourceMode = EArcTerrainPatchSourceMode::DataAsset;

	// --- Mode: Direct Textures ---

	/** Heightmap texture (Direct mode). If null, no height displacement is applied. */
	UPROPERTY(EditAnywhere, Category = "Source|Direct", meta = (EditCondition = "SourceMode == EArcTerrainPatchSourceMode::DirectTextures", EditConditionHides))
	TObjectPtr<UTexture2D> HeightTexture;

	UPROPERTY(EditAnywhere, Category = "Source|Direct", meta = (EditCondition = "SourceMode == EArcTerrainPatchSourceMode::DirectTextures", EditConditionHides))
	EArcTerrainSourceChannel HeightSourceChannel = EArcTerrainSourceChannel::R;

	/** height (cm) = (texel - HeightZeroInEncoding) * HeightEncodingScale (Direct mode). */
	UPROPERTY(EditAnywhere, Category = "Source|Direct", meta = (EditCondition = "SourceMode == EArcTerrainPatchSourceMode::DirectTextures", EditConditionHides, Units = "cm"))
	float HeightEncodingScale = 1000.0f;

	/** height (cm) = (texel - HeightZeroInEncoding) * HeightEncodingScale (Direct mode). */
	UPROPERTY(EditAnywhere, Category = "Source|Direct", meta = (EditCondition = "SourceMode == EArcTerrainPatchSourceMode::DirectTextures", EditConditionHides))
	float HeightZeroInEncoding = 0.5f;

	/** Weight textures routed to terrain channels (Direct mode). */
	UPROPERTY(EditAnywhere, Category = "Source|Direct", meta = (EditCondition = "SourceMode == EArcTerrainPatchSourceMode::DirectTextures", EditConditionHides))
	TArray<FArcTerrainPatchDirectWeight> DirectWeights;

	// --- Mode: Data Asset ---

	/** Data asset holding the height texture + named weight layers (Data Asset mode). */
	UPROPERTY(EditAnywhere, Category = "Source|Asset", meta = (EditCondition = "SourceMode == EArcTerrainPatchSourceMode::DataAsset", EditConditionHides))
	TObjectPtr<UArcTerrainPatchAsset> PatchAsset;

	/** Override the asset's height encoding (Data Asset mode) instead of using its authored scale/zero. */
	UPROPERTY(EditAnywhere, Category = "Source|Asset", meta = (EditCondition = "SourceMode == EArcTerrainPatchSourceMode::DataAsset", EditConditionHides))
	bool bOverrideHeightEncoding = false;

	/** Height encoding scale override (Data Asset mode; used only when bOverrideHeightEncoding). */
	UPROPERTY(EditAnywhere, Category = "Source|Asset", meta = (EditCondition = "SourceMode == EArcTerrainPatchSourceMode::DataAsset && bOverrideHeightEncoding", EditConditionHides, Units = "cm"))
	float OverrideHeightEncodingScale = 1000.0f;

	/** Height encoding zero override (Data Asset mode; used only when bOverrideHeightEncoding). */
	UPROPERTY(EditAnywhere, Category = "Source|Asset", meta = (EditCondition = "SourceMode == EArcTerrainPatchSourceMode::DataAsset && bOverrideHeightEncoding", EditConditionHides))
	float OverrideHeightZeroInEncoding = 0.5f;

	/** Maps a PatchAsset weight-layer name -> the terrain channel it paints into (Data Asset mode).
	 *  Auto-populated on assign in PostEditChangeProperty (missing keys added, existing values never clobbered). */
	UPROPERTY(EditAnywhere, Category = "Channels", meta = (EditCondition = "SourceMode == EArcTerrainPatchSourceMode::DataAsset", EditConditionHides, GetOptions = "GetTerrainChannels"))
	TMap<FName, FName> LayerChannelMap;

	// --- Falloff (semantics: FArcStampSourceDesc EdgeMask / CircleMask) ---

	/** Width (cm) of the inward ramp from the footprint edge. 0 = hard vertical clip. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Falloff", meta = (ClampMin = "0.0", UIMax = "5000.0", ForceUnits = "cm"))
	float EdgeFalloffBand = 1000.0f;

	/** Sharpness exponent of the edge mask: pow(saturate(dist/band), sharpness). Higher = sharper. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Falloff", meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "16.0"))
	float EdgeSharpness = 4.0f;

	/** Multiply a circular blend mask (smoothstep edge) into the stamp — height AND weights. */
	UPROPERTY(EditAnywhere, Category = "Falloff")
	bool bCircleMask = false;

	/** Patch-local XY offset (cm) of the circle center from the footprint center. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (EditCondition = "bCircleMask", ForceUnits = "cm"))
	FVector2D CircleCenterOffset = FVector2D::ZeroVector;

	/** Circle radius (cm). 0 = auto: the ellipse inscribed in the footprint. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm", EditCondition = "bCircleMask"))
	float CircleRadius = 0.0f;

	/** Width (cm) of the smoothstep ramp inward from the circle edge. 0 = hard clip at the circle. */
	UPROPERTY(EditAnywhere, Category = "Falloff", meta = (ClampMin = "0.0", UIMax = "5000.0", ForceUnits = "cm", EditCondition = "bCircleMask"))
	float CircleFalloffBand = 1000.0f;

	//~ Begin UArcTerrainStampComponent interface
	virtual bool GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const override;
	//~ End UArcTerrainStampComponent interface

#if WITH_EDITOR
	/** Invalidates this loaded, registered stamp when its exact Data Asset source changes. */
	void NotifyPatchAssetChanged(UArcTerrainPatchAsset& ChangedAsset);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
