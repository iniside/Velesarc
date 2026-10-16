// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcTerrainBakedField.generated.h"

/**
 * Raster-only baked scalar field: a flat float buffer + grid dimensions. Independent Runtime
 * mirror of the editor-side FArcBakedHeightField (ArcEditorTools) — no spatial metadata
 * (GridOrigin / CellDimensions), which is reconstructed from Dims at consume time using the
 * normalized [-0.5, +0.5] convention so the layout is independent of coverage.
 *
 * ContentId is regenerated on every bake and keys both the GPU source-texture cache and the DDC
 * entry for the baked buffer.
 */
USTRUCT()
struct FArcTerrainBakedField
{
	GENERATED_BODY()

	/** Row-major, Dims.X inner (X varies fastest). */
	UPROPERTY()
	TArray<float> Values;

	UPROPERTY()
	FIntPoint Dims = FIntPoint::ZeroValue;

	/** Regenerated on every bake — keys the GPU source cache + DDC. */
	UPROPERTY()
	FGuid ContentId;

	bool IsValid() const
	{
		return Dims.X >= 2 && Dims.Y >= 2 && Values.Num() == Dims.X * Dims.Y;
	}

	void Reset()
	{
		Values.Reset();
		Dims = FIntPoint::ZeroValue;
		ContentId.Invalidate();
	}
};
