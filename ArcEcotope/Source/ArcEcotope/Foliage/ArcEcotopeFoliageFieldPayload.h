// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcEcotopeFoliageFieldPayload.generated.h"

namespace ArcEcotopeFoliageFieldLimits
{
	constexpr double TargetTexelSizeCm = 100.0;

	/** Raised, never truncated: a volume larger than this gets a coarser texel rather than a clipped field. */
	constexpr int32 MaxFieldTexels = 2048;

	/** Saturation guard: about 64 crowns overlapping one texel. */
	constexpr float MaxOccupancy = 64.0f;

	/** Written as the blob's first int32 and into the payload; a reader disagreeing with it re-derives. */
	constexpr int32 FieldPayloadRev = 2;
}

/**
 * One volume's forest field as it is stored: RGBA16F texels in row-major order, plus the raster
 * placement the shader needs and the identity of the inputs it was derived from.
 *
 * NonTransactional on the owning volume: a transaction inlines every byte of a serialized array, and
 * this one is tens of megabytes.
 */
USTRUCT()
struct ARCECOTOPE_API FArcEcotopeFoliageFieldPayload
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Foliage")
	FIntPoint Dimensions = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, Category = "Foliage")
	double TexelSizeCm = 0.0;

	/** Volume-local cm of the raster's minimum corner. */
	UPROPERTY(VisibleAnywhere, Category = "Foliage")
	FVector2D OriginLocal = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Foliage")
	uint64 SourceSiteSnapshotHash = 0;

	UPROPERTY(VisibleAnywhere, Category = "Foliage")
	uint64 SourceStateChecksum = 0;

	UPROPERTY(VisibleAnywhere, Category = "Foliage")
	int32 PayloadRev = 0;

	/** FFloat16Color texels, row-major, Dimensions.X * Dimensions.Y of them. Never made visible: a
	 *  details panel expands every element of an array this size. */
	UPROPERTY()
	TArray<uint8> Texels;

	bool IsEmpty() const
	{
		return Texels.IsEmpty();
	}

	/** Dimensions representable, and the byte count exactly matching them. */
	bool IsValid() const;

	void Reset();
};
