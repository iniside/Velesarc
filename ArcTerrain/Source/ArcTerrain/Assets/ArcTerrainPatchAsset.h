// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"

#include "ArcTerrainPatchAsset.generated.h"

/** Which texel channel a texture is decoded from when read into a baked field. */
UENUM()
enum class EArcTerrainSourceChannel : uint8
{
	R,
	G,
	B,
	A
};

/**
 * One named weight layer stamped from a texture channel.
 *
 * LayerName is the semantic layer id used as the map key on the consuming terrain stamp. This
 * asset intentionally stores a plain FName (not a MeshPartition FChannelName) so it carries no
 * terrain-context dependency — the consuming stamp routes LayerName to a channel via its own map.
 */
USTRUCT()
struct FArcTerrainPatchWeightLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	FName LayerName;

	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	TObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	EArcTerrainSourceChannel SourceChannel = EArcTerrainSourceChannel::R;

	/** weight = (texel - ZeroInEncoding) * EncodingScale. */
	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	float EncodingScale = 1.0f;

	/** weight = (texel - ZeroInEncoding) * EncodingScale. */
	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	float ZeroInEncoding = 0.0f;

	/** Optional auto-fill hint for the consuming stamp's layer→channel map. */
	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	FName DefaultChannelName;

	/** Fold this weightmap into real height: +raises, -carves. Channel paint stays crisp. Consumed (carve). */
	UPROPERTY(EditAnywhere, Category = "Weight|Geometry")
	bool bContributeToHeight = false;

	/** Signed height (cm) of a full-strength feature. Negative carves channels into geometry. Consumed (carve). */
	UPROPERTY(EditAnywhere, Category = "Weight|Geometry",
		meta = (ForceUnits = "cm", EditCondition = "bContributeToHeight"))
	float HeightAmplitude = 0.0f;

	/** Gaussian blur (cm) applied ONLY to the height contribution. Stored only — not consumed in the MVP. */
	UPROPERTY(EditAnywhere, Category = "Weight|Geometry",
		meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "bContributeToHeight"))
	float HeightBlurRadius = 0.0f;
};

/**
 * Data asset holding a premade heightmap texture plus named weightmap layers to stamp into
 * mesh terrain. Independent Runtime parallel of ArcEditorTools' UArcTexturePatchAsset — new type,
 * nothing is moved/redirected from the editor asset.
 */
UCLASS()
class ARCTERRAIN_API UArcTerrainPatchAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Height")
	TObjectPtr<UTexture2D> HeightTexture;

	UPROPERTY(EditAnywhere, Category = "Height")
	EArcTerrainSourceChannel HeightSourceChannel = EArcTerrainSourceChannel::R;

	/** height (cm) = (texel - HeightZeroInEncoding) * HeightEncodingScale. */
	UPROPERTY(EditAnywhere, Category = "Height", meta = (Units = "cm"))
	float HeightEncodingScale = 1000.0f;

	/** height (cm) = (texel - HeightZeroInEncoding) * HeightEncodingScale. */
	UPROPERTY(EditAnywhere, Category = "Height")
	float HeightZeroInEncoding = 0.5f;

	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	TArray<FArcTerrainPatchWeightLayer> WeightLayers;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
