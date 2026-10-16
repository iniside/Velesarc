// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"

#include "ArcTexturePatchAsset.generated.h"

/** Which texel channel a texture is decoded from when read into a baked field. */
UENUM()
enum class EArcTextureSourceChannel : uint8
{
	Red,
	Green,
	Blue,
	Alpha
};

/**
 * One named weight layer stamped from a texture channel.
 *
 * LayerName is the semantic layer id used as the map key on the consuming modifier
 * (routed to a UE::MeshPartition::FChannelName via the modifier's LayerChannelMap — this
 * asset intentionally stores a plain FName, not a FChannelName, since FChannelName's property
 * customization requires terrain context this standalone asset does not have).
 */
USTRUCT()
struct FArcTexturePatchWeightLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	FName LayerName;

	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	TObjectPtr<UTexture2D> Texture;

	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	EArcTextureSourceChannel SourceChannel = EArcTextureSourceChannel::Red;

	/** weight = (texel - ZeroInEncoding) * EncodingScale. */
	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	double EncodingScale = 1.0;

	/** weight = (texel - ZeroInEncoding) * EncodingScale. */
	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	double ZeroInEncoding = 0.0;

	/** Optional auto-fill hint for the consuming modifier's LayerChannelMap. */
	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	FName DefaultChannelName;

	/** Refine adaptive tessellation against this weightmap (Adaptive mode). */
	UPROPERTY(EditAnywhere, Category="Weight|Geometry")
	bool bDriveTessellation = true;

	/** Pretend-height (cm) of a full-strength feature for tessellation refinement. */
	UPROPERTY(EditAnywhere, Category="Weight|Geometry",
		meta=(ClampMin="0.0", ForceUnits="cm", EditCondition="bDriveTessellation"))
	double TessellationAmplitude = 100.0;

	/** Fold this weightmap into real height: +raises, -carves. Channel paint stays crisp. */
	UPROPERTY(EditAnywhere, Category="Weight|Geometry")
	bool bContributeToHeight = false;

	/** Signed height (cm) of a full-strength feature. Negative carves channels into geometry. */
	UPROPERTY(EditAnywhere, Category="Weight|Geometry",
		meta=(ForceUnits="cm", EditCondition="bContributeToHeight"))
	double HeightAmplitude = 0.0;

	/** Gaussian blur (cm) applied ONLY to the height contribution; channel paint is never blurred. */
	UPROPERTY(EditAnywhere, Category="Weight|Geometry",
		meta=(ClampMin="0.0", ForceUnits="cm", EditCondition="bContributeToHeight"))
	double HeightBlurRadius = 0.0;

	/** EWA-filtered height contribution (Uniform/Adaptive only). */
	UPROPERTY(EditAnywhere, Category="Weight|Geometry",
		meta=(EditCondition="bContributeToHeight"))
	bool bFilteredDisplacement = false;
};

/**
 * Data asset holding a premade heightmap texture plus named weightmap layers to stamp into
 * mesh terrain (Mode B — Data Asset — of the Texture Patch Stamp modifier).
 */
UCLASS()
class ARCEDITORTOOLS_API UArcTexturePatchAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Height")
	TObjectPtr<UTexture2D> HeightTexture;

	UPROPERTY(EditAnywhere, Category = "Height")
	EArcTextureSourceChannel HeightSourceChannel = EArcTextureSourceChannel::Red;

	/** height (cm) = (texel - HeightZeroInEncoding) * HeightEncodingScale. */
	UPROPERTY(EditAnywhere, Category = "Height")
	double HeightEncodingScale = 1000.0;

	/** height (cm) = (texel - HeightZeroInEncoding) * HeightEncodingScale. */
	UPROPERTY(EditAnywhere, Category = "Height")
	double HeightZeroInEncoding = 0.5;

	UPROPERTY(EditAnywhere, Category = "WeightLayers")
	TArray<FArcTexturePatchWeightLayer> WeightLayers;
};
