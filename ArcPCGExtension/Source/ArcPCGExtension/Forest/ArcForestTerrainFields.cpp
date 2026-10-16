// Copyright Lukasz Baran. All Rights Reserved.

#include "Forest/ArcForestTerrainFields.h"

namespace ArcForestTerrainFieldsLocal
{
	/** Value a flat/unknown SunExposure cell takes — the middle of the range, never an edge. */
	constexpr float FlatExposure = 0.5f;

	/** Value an Elevation cell takes when there is no usable range or no valid data at all. */
	constexpr float NeutralElevation = 0.5f;

	/**
	 * Fill OutHeights with the height of the nearest VALID cell (the cell itself when it is valid),
	 * so a fallback-Z hole never enters a finite difference as a cliff.
	 *
	 * Multi-source breadth-first search seeded with every valid cell, expanding over 4-neighbours:
	 * that visits cells in non-decreasing Manhattan distance from the valid set, so each hole takes
	 * the height of a nearest valid cell (ties broken by scan/insertion order — arbitrary but
	 * deterministic, which is all the solver requires). O(CellCount).
	 *
	 * The queue is consumed with a HEAD INDEX, never RemoveAt(0) — a per-element removal from the
	 * front of a TArray shifts the whole array on every pop.
	 *
	 * An empty HitGrid means "no mask supplied" and every cell counts as valid.
	 * Returns false when NO cell is valid (OutHeights is then zero-filled and meaningless).
	 */
	bool ReplicateFromNearestValid(
		TArrayView<const float> HeightGrid,
		TArrayView<const bool> HitGrid,
		FIntPoint Dims,
		TArray<float>& OutHeights)
	{
		const int32 CellCount = Dims.X * Dims.Y;
		const bool bHaveMask = HitGrid.Num() == CellCount;

		OutHeights.SetNumUninitialized(CellCount);

		TArray<bool> bResolved;
		bResolved.SetNumZeroed(CellCount);

		TArray<int32> Queue;
		Queue.Reserve(CellCount);

		for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
		{
			if (!bHaveMask || HitGrid[CellIndex])
			{
				OutHeights[CellIndex] = HeightGrid[CellIndex];
				bResolved[CellIndex] = true;
				Queue.Add(CellIndex);
			}
		}

		if (Queue.IsEmpty())
		{
			for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
			{
				OutHeights[CellIndex] = 0.0f;
			}
			return false;
		}

		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 CellIndex = Queue[Head];
			const int32 Col = CellIndex % Dims.X;
			const int32 Row = CellIndex / Dims.X;
			const float Height = OutHeights[CellIndex];

			const int32 NeighbourCols[4] = { Col - 1, Col + 1, Col, Col };
			const int32 NeighbourRows[4] = { Row, Row, Row - 1, Row + 1 };

			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 NCol = NeighbourCols[Side];
				const int32 NRow = NeighbourRows[Side];
				if (NCol < 0 || NCol >= Dims.X || NRow < 0 || NRow >= Dims.Y)
				{
					continue;
				}

				const int32 NeighbourIndex = (NRow * Dims.X) + NCol;
				if (!bResolved[NeighbourIndex])
				{
					bResolved[NeighbourIndex] = true;
					OutHeights[NeighbourIndex] = Height;
					Queue.Add(NeighbourIndex);
				}
			}
		}

		return true;
	}

	/**
	 * Height difference per unit distance along ONE axis at Index, given the neighbour stride along
	 * that axis and the spacing between adjacent cell CENTRES.
	 *
	 * Interior: central difference over 2*Spacing.
	 * First/last cell on the axis: ONE-SIDED difference over Spacing (not 2*Spacing) — clamping a
	 * neighbour into a central difference would leave the two taps one cell apart while still
	 * dividing by 2*Spacing, halving the gradient along the whole border, i.e. along every partition
	 * seam under partitioned generation.
	 *
	 * Spacing <= 0 (degenerate axis, single cell, or zero-span bounds) -> no gradient along the axis.
	 */
	float AxisDerivative(
		const TArray<float>& Heights,
		int32 Index,
		int32 Stride,
		int32 AxisCoord,
		int32 AxisDim,
		float Spacing)
	{
		if (AxisDim <= 1 || Spacing <= 0.0f)
		{
			return 0.0f;
		}

		if (AxisCoord == 0)
		{
			return (Heights[Index + Stride] - Heights[Index]) / Spacing;
		}
		if (AxisCoord == AxisDim - 1)
		{
			return (Heights[Index] - Heights[Index - Stride]) / Spacing;
		}
		return (Heights[Index + Stride] - Heights[Index - Stride]) / (2.0f * Spacing);
	}

	/** Reset a field to the given footprint/dims with uninitialised values of the right length. */
	void PrepareField(FArcForestScalarField& Field, const FBox2f& Bounds, FIntPoint Dims, int32 CellCount)
	{
		Field.Bounds = Bounds;
		Field.Dims = Dims;
		Field.Values.SetNumUninitialized(CellCount);
	}

	/** Reset a field to "no field supplied" (empty -> samples as the neutral 1.0). */
	void ClearField(FArcForestScalarField& Field)
	{
		Field.Bounds = FBox2f(ForceInit);
		Field.Dims = FIntPoint::ZeroValue;
		Field.Values.Reset();
	}
}

namespace ArcForestTerrainFields
{
	void Derive(
		TArrayView<const float> HeightGrid,
		TArrayView<const bool> HitGrid,
		FIntPoint Dims,
		const FBox2f& Bounds,
		const FArcForestTerrainFieldParams& Params,
		FArcForestScalarField& OutSlope,
		FArcForestScalarField& OutSunExposure,
		FArcForestScalarField& OutElevation)
	{
		using namespace ArcForestTerrainFieldsLocal;

		const int32 CellCount = Dims.X * Dims.Y;
		if (Dims.X <= 0 || Dims.Y <= 0 || HeightGrid.Num() != CellCount)
		{
			// No usable grid: three empty fields, which sample as the neutral 1.0.
			ClearField(OutSlope);
			ClearField(OutSunExposure);
			ClearField(OutElevation);
			return;
		}

		PrepareField(OutSlope, Bounds, Dims, CellCount);
		PrepareField(OutSunExposure, Bounds, Dims, CellCount);
		PrepareField(OutElevation, Bounds, Dims, CellCount);

		TArray<float> Heights;
		const bool bAnyValid = ReplicateFromNearestValid(HeightGrid, HitGrid, Dims, Heights);
		if (!bAnyValid)
		{
			// Every cell missed the surface: there is no morphology to derive, so the fields are flat
			// by definition rather than derived from a fallback constant.
			for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
			{
				OutSlope.Values[CellIndex] = 0.0f;
				OutSunExposure.Values[CellIndex] = FlatExposure;
				OutElevation.Values[CellIndex] = NeutralElevation;
			}
			return;
		}

		// dx and dy SEPARATELY from Bounds/Dims: MaxGridDim clamps the dimensions, so the cells are
		// not square on a non-square or large footprint and a nominal cell size would be wrong.
		const FVector2f Size = Bounds.GetSize();
		const float Dx = (Dims.X > 1 && Size.X > UE_KINDA_SMALL_NUMBER) ? (Size.X / static_cast<float>(Dims.X)) : 0.0f;
		const float Dy = (Dims.Y > 1 && Size.Y > UE_KINDA_SMALL_NUMBER) ? (Size.Y / static_cast<float>(Dims.Y)) : 0.0f;

		const float SunAzimuthRadians = FMath::DegreesToRadians(Params.SunAzimuthDegrees);
		const bool bHaveSlopeRange = Params.MaxSlopeDegrees > UE_KINDA_SMALL_NUMBER;

		const float ElevationSpan = Params.ElevationMaxCm - Params.ElevationMinCm;
		const bool bHaveElevationRange = ElevationSpan > UE_KINDA_SMALL_NUMBER;

		for (int32 Row = 0; Row < Dims.Y; ++Row)
		{
			for (int32 Col = 0; Col < Dims.X; ++Col)
			{
				const int32 CellIndex = (Row * Dims.X) + Col;

				const float DhDx = AxisDerivative(Heights, CellIndex, /*Stride=*/1, Col, Dims.X, Dx);
				const float DhDy = AxisDerivative(Heights, CellIndex, /*Stride=*/Dims.X, Row, Dims.Y, Dy);

				const float GradientMagnitude = FMath::Sqrt((DhDx * DhDx) + (DhDy * DhDy));

				// Slope: atan of the gradient magnitude is the surface's tilt from horizontal.
				const float SlopeDegrees = FMath::RadiansToDegrees(FMath::Atan(GradientMagnitude));
				OutSlope.Values[CellIndex] = bHaveSlopeRange
					? FMath::Clamp(SlopeDegrees / Params.MaxSlopeDegrees, 0.0f, 1.0f)
					: ((GradientMagnitude > UE_SMALL_NUMBER) ? 1.0f : 0.0f);

				// SunExposure: the aspect is the direction of steepest DESCENT — the direction the
				// slope FACES. Atan2(DhDy, DhDx) would be steepest ASCENT, a 180-degree error.
				// Convention: 0 rad = +X, angles increasing toward +Y (see FArcForestTerrainFieldParams).
				// Flat ground has no aspect at all, so it takes the mid value explicitly instead of
				// letting Atan2(0,0) == 0 turn the azimuth into an arbitrary answer.
				if (GradientMagnitude > UE_SMALL_NUMBER)
				{
					const float Aspect = FMath::Atan2(-DhDy, -DhDx);
					OutSunExposure.Values[CellIndex] =
						FMath::Clamp(0.5f * (1.0f + FMath::Cos(Aspect - SunAzimuthRadians)), 0.0f, 1.0f);
				}
				else
				{
					OutSunExposure.Values[CellIndex] = FlatExposure;
				}

				// Elevation: ABSOLUTE range, never the grid's own extrema (partition-seam stability).
				OutElevation.Values[CellIndex] = bHaveElevationRange
					? FMath::Clamp((Heights[CellIndex] - Params.ElevationMinCm) / ElevationSpan, 0.0f, 1.0f)
					: NeutralElevation;
			}
		}
	}
}
