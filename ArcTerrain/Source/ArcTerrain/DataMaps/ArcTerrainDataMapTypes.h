// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/ArcTerrainPatchAsset.h" // EArcTerrainSourceChannel
#include "ArcTerrainDataMapTypes.generated.h"

class UTexture2D;

/**
 * Index into per-cell data-map storage and the key of the CPU sampler. NOT an authoring choice: a
 * modifier exposes one property per mask, so this never reaches a details panel.
 */
UENUM()
enum class EArcTerrainDataMap : uint8
{
	Deposits,
	Wear,
	Flow,
	Soil,
	Scree,
	RockMap,
	FlowMapX,
	FlowMapY,
	Rivers,
	Count UMETA(Hidden)
};

/**
 * One authored guidance mask: a texture channel plus its linear decode. The unit and the meaning of
 * 0 and 1 are stated by the property that holds it, not by this struct.
 *
 * value = (texel - ZeroInEncoding) * EncodingScale
 */
USTRUCT()
struct FArcTerrainDataMapSource
{
	GENERATED_BODY()

	FArcTerrainDataMapSource() = default;

	FArcTerrainDataMapSource(float InEncodingScale, float InZeroInEncoding)
		: EncodingScale(InEncodingScale)
		, ZeroInEncoding(InZeroInEncoding)
	{
	}

	/** Mask texture. Null means this modifier carries no mask of its type. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	TObjectPtr<UTexture2D> Texture;

	/** Texel channel decoded as the mask. Single-channel sources ignore this. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	EArcTerrainSourceChannel SourceChannel = EArcTerrainSourceChannel::R;

	/** value = (texel - ZeroInEncoding) * EncodingScale. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	float EncodingScale = 1.0f;

	/** value = (texel - ZeroInEncoding) * EncodingScale. */
	UPROPERTY(EditAnywhere, Category = "Data Maps")
	float ZeroInEncoding = 0.0f;
};
