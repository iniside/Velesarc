// Copyright Lukasz Baran. All Rights Reserved.

#include "Forest/ArcForestLightModel.h"

namespace ArcForestLightModelLocal
{
	/** Fallback grid resolution when the caller passes a non-positive CellSize. */
	static constexpr float DefaultCellSize = 1.0f;

	/**
	 * Grid geometry resolved once per compute: origin + resolution + integer dimensions over the
	 * XY bounds. Cell (CX,CY) maps to linear index CY*DimX + CX.
	 */
	struct FGridGeometry
	{
		FVector2f Origin = FVector2f::ZeroVector;
		float CellSize = DefaultCellSize;
		int32 DimX = 1;
		int32 DimY = 1;

		int32 CellCount() const
		{
			return DimX * DimY;
		}
	};

	/**
	 * Bounds the light field covers. Prefer the caller-supplied Params.Bounds2D; fall back to an
	 * AABB over the plant positions when the caller left bounds invalid/degenerate (useful for
	 * tests that only pass positions). Returns false only when there is nothing to bound.
	 */
	bool ResolveBounds(TArrayView<const FVector2f> Positions, const FBox2f& ParamBounds, FBox2f& OutBounds)
	{
		if (ParamBounds.bIsValid != 0)
		{
			const FVector2f Size = ParamBounds.Max - ParamBounds.Min;
			if (Size.X > 0.0f && Size.Y > 0.0f)
			{
				OutBounds = ParamBounds;
				return true;
			}
		}

		OutBounds = FBox2f(ForceInit);
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			OutBounds += Positions[Index];
		}
		return OutBounds.bIsValid != 0;
	}

	/** Map a world XY position to its clamped cell coordinates (FloorToInt handles negatives). */
	FIntPoint ClampedCell(FVector2f Position, const FGridGeometry& Grid)
	{
		const int32 RawX = FMath::FloorToInt((Position.X - Grid.Origin.X) / Grid.CellSize);
		const int32 RawY = FMath::FloorToInt((Position.Y - Grid.Origin.Y) / Grid.CellSize);
		return FIntPoint(
			FMath::Clamp(RawX, 0, Grid.DimX - 1),
			FMath::Clamp(RawY, 0, Grid.DimY - 1));
	}

	/**
	 * Splat a plant crown's LAI into its band grid as a disc: every cell whose CENTRE lies within
	 * CrownRadius of the plant position gets += CrownLAIDensity. The scanned cell rectangle is
	 * CLAMPED to the grid so no out-of-bounds write can occur even for crowns straddling the edge.
	 */
	void SplatCrown(
		TArray<float>& BandGrid,
		FVector2f Position,
		float CrownRadius,
		float CrownLAIDensity,
		const FGridGeometry& Grid)
	{
		if (CrownRadius <= 0.0f || CrownLAIDensity == 0.0f)
		{
			return;
		}

		const int32 RawMinX = FMath::FloorToInt((Position.X - CrownRadius - Grid.Origin.X) / Grid.CellSize);
		const int32 RawMaxX = FMath::FloorToInt((Position.X + CrownRadius - Grid.Origin.X) / Grid.CellSize);
		const int32 RawMinY = FMath::FloorToInt((Position.Y - CrownRadius - Grid.Origin.Y) / Grid.CellSize);
		const int32 RawMaxY = FMath::FloorToInt((Position.Y + CrownRadius - Grid.Origin.Y) / Grid.CellSize);

		const int32 MinX = FMath::Clamp(RawMinX, 0, Grid.DimX - 1);
		const int32 MaxX = FMath::Clamp(RawMaxX, 0, Grid.DimX - 1);
		const int32 MinY = FMath::Clamp(RawMinY, 0, Grid.DimY - 1);
		const int32 MaxY = FMath::Clamp(RawMaxY, 0, Grid.DimY - 1);

		// If the crown's whole bracket is off one side of the grid, RawMax < 0 or RawMin > Dim-1;
		// the clamp collapses the range to a single edge cell, and the centre-distance test below
		// rejects it unless it genuinely overlaps — so no spurious edge writes.
		const float RadiusSq = CrownRadius * CrownRadius;
		const float HalfCell = 0.5f * Grid.CellSize;

		for (int32 CellY = MinY; CellY <= MaxY; ++CellY)
		{
			const float CentreY = Grid.Origin.Y + (static_cast<float>(CellY) * Grid.CellSize) + HalfCell;
			for (int32 CellX = MinX; CellX <= MaxX; ++CellX)
			{
				const float CentreX = Grid.Origin.X + (static_cast<float>(CellX) * Grid.CellSize) + HalfCell;
				const FVector2f Delta(CentreX - Position.X, CentreY - Position.Y);
				if (Delta.SizeSquared() <= RadiusSq)
				{
					BandGrid[(CellY * Grid.DimX) + CellX] += CrownLAIDensity;
				}
			}
		}
	}

	/**
	 * Geometry of the strided transmittance raster laid over an LAI grid: how many LAI cells collapse
	 * into one raster cell per axis, the resulting raster dims, and the world footprint the raster
	 * spans.
	 *
	 * Bounds deliberately spans Dims * Stride * CellSize — the raster's OWN cell rectangle — and NOT
	 * the LAI grid's extent. That is what makes FArcForestScalarField's samplers agree exactly with
	 * the block index of a position's LAI cell: the sampler derives its cell size as
	 * Bounds.Size / Dims, so only this footprint yields StrideX * CellSize, and
	 * floor(floor(x / CellSize) / Stride) == floor(x / (Stride * CellSize)) for integer Stride > 0.
	 * Sizing it to the grid extent instead would shear the mapping by up to a block whenever the grid
	 * dims are not an exact multiple of the stride. The last block of each axis may be PARTIAL (the
	 * raster covers at or past the grid edge); the mean is taken over the cells that actually exist.
	 */
	struct FRasterLayout
	{
		FBox2f Bounds = FBox2f(ForceInit);
		FIntPoint Dims = FIntPoint::ZeroValue;
		int32 StrideX = 1;
		int32 StrideY = 1;

		int32 CellCount() const
		{
			return Dims.X * Dims.Y;
		}
	};

	/**
	 * Lay a raster over a GridDimX x GridDimY LAI grid at the requested stride, growing the EFFECTIVE
	 * stride as needed so neither raster axis exceeds MaxTransmittanceDim.
	 *
	 * The cap is enforced by raising the stride rather than by clamping the dims: clamping the dims
	 * would leave the raster covering only part of the footprint (silently truncated field), whereas a
	 * coarser stride still covers all of it at lower resolution. With
	 * EffStride >= ceil(Dim / MaxDim) it follows that ceil(Dim / EffStride) <= MaxDim.
	 */
	FRasterLayout ResolveRasterLayout(FVector2f Origin, float CellSize, int32 GridDimX, int32 GridDimY, int32 Stride)
	{
		const int32 RequestedStride = FMath::Max(1, Stride);

		FRasterLayout Layout;
		Layout.StrideX = FMath::Max(RequestedStride, FMath::DivideAndRoundUp(GridDimX, ArcForestLight::MaxTransmittanceDim));
		Layout.StrideY = FMath::Max(RequestedStride, FMath::DivideAndRoundUp(GridDimY, ArcForestLight::MaxTransmittanceDim));
		Layout.Dims.X = FMath::Max(1, FMath::DivideAndRoundUp(GridDimX, Layout.StrideX));
		Layout.Dims.Y = FMath::Max(1, FMath::DivideAndRoundUp(GridDimY, Layout.StrideY));

		const FVector2f Span(
			static_cast<float>(Layout.Dims.X * Layout.StrideX) * CellSize,
			static_cast<float>(Layout.Dims.Y * Layout.StrideY) * CellSize);
		Layout.Bounds = FBox2f(Origin, Origin + Span);
		return Layout;
	}

	/**
	 * The every-early-out answer: a full-light raster over Params.Bounds2D. Mirrors the
	 * OutIncidentLight = 1.0f prefill — an empty or malformed stand casts no shade, so every cell
	 * reads 1.0 rather than stale or absent data.
	 *
	 * An invalid/degenerate Bounds2D leaves the field EMPTY on purpose: there is no footprint to size
	 * a grid from, and FArcForestScalarField's samplers already answer an empty grid as 1.0, so the
	 * consumer still sees full light.
	 */
	void FillUnityTransmittance(
		FArcForestScalarField& OutField,
		const FBox2f& ParamBounds,
		float CellSize,
		int32 Stride)
	{
		const FVector2f Size = ParamBounds.Max - ParamBounds.Min;
		if (ParamBounds.bIsValid == 0 || Size.X <= 0.0f || Size.Y <= 0.0f)
		{
			OutField.Bounds = FBox2f(ForceInit);
			OutField.Dims = FIntPoint::ZeroValue;
			OutField.Values.Reset();
			return;
		}

		// Same dims the sweep would have produced for this footprint (ArcForestLightModel's own
		// unbounded CeilToInt), then strided and capped exactly as the real emit path does.
		const int32 GridDimX = FMath::Max(1, FMath::CeilToInt(Size.X / CellSize));
		const int32 GridDimY = FMath::Max(1, FMath::CeilToInt(Size.Y / CellSize));
		const FRasterLayout Layout = ResolveRasterLayout(ParamBounds.Min, CellSize, GridDimX, GridDimY, Stride);

		OutField.Bounds = Layout.Bounds;
		OutField.Dims = Layout.Dims;
		OutField.Values.SetNumUninitialized(Layout.CellCount());
		for (int32 Index = 0; Index < OutField.Values.Num(); ++Index)
		{
			OutField.Values[Index] = 1.0f;
		}
	}

	/**
	 * Snapshot the CURRENT band accumulators into the strided transmittance raster.
	 *
	 * Called at the one instant that is correct for the query height (see the capture cases on
	 * ComputeIncidentLight) — it reads whatever the accumulators hold right now and must NOT be
	 * deferred.
	 *
	 * Aggregation is MEAN-LAI-THEN-EXP: sum the included bands' LAI over the block, divide by the
	 * block's real cell count, and take one exp(). Because exp is convex this is not the mean of the
	 * per-cell transmittances (Jensen) — a deliberate smoothing, documented on the declaration.
	 *
	 * Allocation-free given a correctly pre-sized OutValues: the band sums are accumulated IN PLACE in
	 * the output buffer (hence the zero-fill first) and converted to transmittance in a second pass,
	 * so no per-emit scratch array is needed on the worker thread. The traversal is sequential over
	 * each band grid row with the small raster as the scattered target, not the reverse.
	 */
	void EmitTransmittance(
		TArray<float>& OutValues,
		const TArray<float>* const* IncludedBandGrids,
		int32 NumIncludedBands,
		const FGridGeometry& Grid,
		const FRasterLayout& Layout,
		float ExtinctionK)
	{
		OutValues.SetNumUninitialized(Layout.CellCount());
		FMemory::Memzero(OutValues.GetData(), static_cast<SIZE_T>(OutValues.Num()) * sizeof(float));

		for (int32 BandSlot = 0; BandSlot < NumIncludedBands; ++BandSlot)
		{
			const TArray<float>& BandGrid = *IncludedBandGrids[BandSlot];
			for (int32 CellY = 0; CellY < Grid.DimY; ++CellY)
			{
				const int32 GridRowBase = CellY * Grid.DimX;
				const int32 RasterRowBase = (CellY / Layout.StrideY) * Layout.Dims.X;
				for (int32 RasterX = 0; RasterX < Layout.Dims.X; ++RasterX)
				{
					const int32 BeginX = RasterX * Layout.StrideX;
					const int32 EndX = FMath::Min(BeginX + Layout.StrideX, Grid.DimX);
					float BlockSum = 0.0f;
					for (int32 CellX = BeginX; CellX < EndX; ++CellX)
					{
						BlockSum += BandGrid[GridRowBase + CellX];
					}
					OutValues[RasterRowBase + RasterX] += BlockSum;
				}
			}
		}

		for (int32 RasterY = 0; RasterY < Layout.Dims.Y; ++RasterY)
		{
			// Partial trailing blocks are normal (the raster covers at or past the grid edge), so the
			// divisor is the block's REAL cell count — never Stride*Stride, which would dilute the edge.
			const int32 BlockHeight = FMath::Min(Layout.StrideY, Grid.DimY - (RasterY * Layout.StrideY));
			const int32 RasterRowBase = RasterY * Layout.Dims.X;
			for (int32 RasterX = 0; RasterX < Layout.Dims.X; ++RasterX)
			{
				const int32 BlockWidth = FMath::Min(Layout.StrideX, Grid.DimX - (RasterX * Layout.StrideX));
				const int32 BlockCells = FMath::Max(1, BlockWidth * BlockHeight);
				const int32 FlatIndex = RasterRowBase + RasterX;
				const float MeanLAI = OutValues[FlatIndex] / static_cast<float>(BlockCells);
				OutValues[FlatIndex] = FMath::Exp(-ExtinctionK * MeanLAI);
			}
		}
	}
}

EArcForestLayer FArcForestLightModel::LayerForHeight(
	float Height,
	float ShrubMinHeight,
	float UnderstoryMinHeight,
	float CanopyMinHeight)
{
	if (Height < ShrubMinHeight)
	{
		return EArcForestLayer::Field;
	}
	if (Height < UnderstoryMinHeight)
	{
		return EArcForestLayer::Shrub;
	}
	if (Height < CanopyMinHeight)
	{
		return EArcForestLayer::Understory;
	}
	return EArcForestLayer::Canopy;
}

void FArcForestLightModel::ComputeIncidentLight(
	TArrayView<const FVector2f> Positions,
	TArrayView<const float> Heights,
	TArrayView<const float> CrownRadii,
	TArrayView<const float> CrownLAIDensity,
	TArrayView<const EArcForestLayer> Bands,
	const FArcForestSimParams& Params,
	float CellSize,
	TArray<float>& OutIncidentLight,
	FArcForestScalarField* OutTransmittance,
	float QueryHeightCm,
	int32 Stride,
	EArcForestLayer QueryBand) const
{
	// PERF (M5): dominant cost is the crown LAI splat = Sum over plants of (crown_area / cell_area)
	// grid writes, plus exactly one FMath::Exp per plant for the read. Per-band grids add a fixed
	// GArcForestLayerCount (4) * DimX * DimY * 4 bytes of accumulator memory and up to 4 cell reads
	// per plant. For a 100 m x 100 m region at 1 m cells: 10,000 cells -> ~160 KB of grids; 10,000
	// plants of ~2 m crown radius -> ~12.6 cells each -> ~126k splat writes + 10,000 exp() calls.
	// Both are bounded and cheap. Grid resolution is a caller param (~1 m field; pass coarser
	// CellSize for up-band passes — per-band resolution is a future optimization, single uniform
	// resolution here).
	const int32 NumPlants = Positions.Num();
	OutIncidentLight.Reset();
	OutIncidentLight.SetNumUninitialized(NumPlants);
	for (int32 Index = 0; Index < NumPlants; ++Index)
	{
		// Default to full light; overwritten by the sweep. Also the safe result if we early-out
		// (degenerate bounds, or malformed parallel arrays) — no shading rather than garbage.
		OutIncidentLight[Index] = 1.0f;
	}

	// Resolved once, ahead of the early-outs: the unity transmittance fill needs the SAME effective
	// cell size the sweep would have used, or an early-out raster would not match the shape of a
	// non-early-out one for identical bounds.
	const float EffectiveCellSize = (CellSize > 0.0f) ? CellSize : ArcForestLightModelLocal::DefaultCellSize;

	if (NumPlants == 0)
	{
		if (OutTransmittance != nullptr)
		{
			ArcForestLightModelLocal::FillUnityTransmittance(
				*OutTransmittance, Params.Bounds2D, EffectiveCellSize, Stride);
		}
		return;
	}

	// Parallel-array contract guard: a length mismatch means we cannot trust indexed reads, so we
	// degrade to "no shading" (already filled) rather than risk an out-of-bounds access.
	if (Heights.Num() < NumPlants || CrownRadii.Num() < NumPlants || CrownLAIDensity.Num() < NumPlants)
	{
		if (OutTransmittance != nullptr)
		{
			ArcForestLightModelLocal::FillUnityTransmittance(
				*OutTransmittance, Params.Bounds2D, EffectiveCellSize, Stride);
		}
		return;
	}

	FBox2f Bounds(ForceInit);
	if (!ArcForestLightModelLocal::ResolveBounds(Positions, Params.Bounds2D, Bounds))
	{
		if (OutTransmittance != nullptr)
		{
			ArcForestLightModelLocal::FillUnityTransmittance(
				*OutTransmittance, Params.Bounds2D, EffectiveCellSize, Stride);
		}
		return;
	}

	ArcForestLightModelLocal::FGridGeometry Grid;
	Grid.Origin = Bounds.Min;
	Grid.CellSize = EffectiveCellSize;
	const FVector2f BoundsSize = Bounds.Max - Bounds.Min;
	Grid.DimX = FMath::Max(1, FMath::CeilToInt(BoundsSize.X / Grid.CellSize));
	Grid.DimY = FMath::Max(1, FMath::CeilToInt(BoundsSize.Y / Grid.CellSize));

	// One LAI accumulator per height band. The core sweep is height-based; the per-band split
	// exists solely to gate the intra-layer self-shading flag (see header). When Bands is absent
	// every plant uses band 0 and the read includes all grids, collapsing to a single grid.
	TArray<float> BandGrids[GArcForestLayerCount];
	for (int32 Band = 0; Band < GArcForestLayerCount; ++Band)
	{
		BandGrids[Band].SetNumZeroed(Grid.CellCount());
	}

	const bool bHaveBands = Bands.Num() == NumPlants;

	// Height-descending order with a total, deterministic tie-break by ascending index. A total
	// order makes the sort result reproducible regardless of the (unstable) sort's internals.
	TArray<int32> Order;
	Order.SetNumUninitialized(NumPlants);
	for (int32 Index = 0; Index < NumPlants; ++Index)
	{
		Order[Index] = Index;
	}
	Order.Sort([&Heights](const int32& A, const int32& B) -> bool
	{
		const float HeightA = Heights[A];
		const float HeightB = Heights[B];
		if (HeightA != HeightB)
		{
			return HeightA > HeightB;
		}
		return A < B;
	});

	const float ExtinctionK = Params.CanopyExtinction;

	// ---- Optional query-height transmittance raster --------------------------------------------
	// Set up here so the mid-sweep capture below is a straight write into an already-sized buffer;
	// nothing is allocated per group, and on the second and later solver iterations SetNumUninitialized
	// on the reused field keeps the existing allocation (the worker must not allocate per iteration).
	const bool bWantTransmittance = OutTransmittance != nullptr;
	ArcForestLightModelLocal::FRasterLayout RasterLayout;
	const TArray<float>* IncludedBandGrids[GArcForestLayerCount] = {};
	int32 NumIncludedBands = 0;
	bool bTransmittanceCaptured = false;
	if (bWantTransmittance)
	{
		RasterLayout = ArcForestLightModelLocal::ResolveRasterLayout(
			Grid.Origin, Grid.CellSize, Grid.DimX, Grid.DimY, Stride);
		OutTransmittance->Bounds = RasterLayout.Bounds;
		OutTransmittance->Dims = RasterLayout.Dims;

		// Self-band gate, identical in rule to the per-plant read below: exclude the query height's own
		// band only when that band's flag is OFF, and treat an absent Bands array as all-ON.
		const int32 QueryBandIndex = FMath::Clamp(
			static_cast<int32>(QueryBand), 0, GArcForestLayerCount - 1);
		const bool bIncludeQuerySelfBand = !bHaveBands || Params.IntraLayerShading[QueryBandIndex];
		for (int32 Band = 0; Band < GArcForestLayerCount; ++Band)
		{
			if (Band == QueryBandIndex && !bIncludeQuerySelfBand)
			{
				continue;
			}
			IncludedBandGrids[NumIncludedBands] = &BandGrids[Band];
			++NumIncludedBands;
		}
	}

	// Sweep tallest-first in exactly-equal-height GROUPS. Within a group all reads happen before
	// any writes so equal-height plants do NOT shade each other (strictly-taller tie rule) and the
	// result is independent of intra-group ordering.
	int32 GroupStart = 0;
	while (GroupStart < NumPlants)
	{
		const float GroupHeight = Heights[Order[GroupStart]];
		// Always include GroupStart itself, then extend over exactly-equal heights. Starting at
		// GroupStart+1 guarantees the group is non-empty and GroupStart strictly advances every
		// iteration — this also defuses a non-finite (NaN) height, which fails every `== GroupHeight`
		// test (NaN != NaN) and would otherwise leave the group empty and spin the outer loop forever.
		// A NaN height simply forms its own singleton group; valid finite heights are unaffected.
		int32 GroupEnd = GroupStart + 1;
		while (GroupEnd < NumPlants && Heights[Order[GroupEnd]] == GroupHeight)
		{
			++GroupEnd;
		}

		// CAPTURE CASE 1 — group boundary. We are immediately before Phase A of the first group at or
		// below the query height, i.e. straight after Phase B of the last STRICTLY TALLER group, so the
		// accumulators hold exactly the taller groups' writes: bit-identical to what a plant of height
		// QueryHeightCm reads. This must happen BEFORE this group's Phase B or the raster would include
		// same-height plants the strictly-taller rule excludes. (A NaN GroupHeight fails this test and
		// falls through to case 2.)
		if (bWantTransmittance && !bTransmittanceCaptured && GroupHeight <= QueryHeightCm)
		{
			ArcForestLightModelLocal::EmitTransmittance(
				OutTransmittance->Values, IncludedBandGrids, NumIncludedBands, Grid, RasterLayout, ExtinctionK);
			bTransmittanceCaptured = true;
		}

		// Phase A — reads. Each plant sees only shade splatted by strictly-taller groups.
		for (int32 OrderIndex = GroupStart; OrderIndex < GroupEnd; ++OrderIndex)
		{
			const int32 PlantIndex = Order[OrderIndex];
			const FIntPoint Cell = ArcForestLightModelLocal::ClampedCell(Positions[PlantIndex], Grid);
			const int32 CellIndex = (Cell.Y * Grid.DimX) + Cell.X;

			const int32 BandIndex = bHaveBands ? static_cast<int32>(Bands[PlantIndex]) : 0;
			// Same-band contribution is excluded only when this band's self-shading flag is OFF.
			// Cross-band shade is always included. Without a Bands array we behave as all-ON.
			const bool bIncludeSelfBand = !bHaveBands || Params.IntraLayerShading[BandIndex];

			float AccumulatedLAI = 0.0f;
			for (int32 Band = 0; Band < GArcForestLayerCount; ++Band)
			{
				if (Band == BandIndex && !bIncludeSelfBand)
				{
					continue;
				}
				AccumulatedLAI += BandGrids[Band][CellIndex];
			}

			OutIncidentLight[PlantIndex] = FMath::Exp(-ExtinctionK * AccumulatedLAI);
		}

		// Phase B — writes. Splat each plant's crown into its own band grid.
		for (int32 OrderIndex = GroupStart; OrderIndex < GroupEnd; ++OrderIndex)
		{
			const int32 PlantIndex = Order[OrderIndex];
			const int32 BandIndex = bHaveBands ? static_cast<int32>(Bands[PlantIndex]) : 0;
			ArcForestLightModelLocal::SplatCrown(
				BandGrids[BandIndex],
				Positions[PlantIndex],
				CrownRadii[PlantIndex],
				CrownLAIDensity[PlantIndex],
				Grid);
		}

		GroupStart = GroupEnd;
	}

	// CAPTURE CASE 2 — post-sweep. The sweep never reached a group at or below the query height, so
	// EVERY plant is strictly taller than it and the finished accumulators ARE the correct snapshot.
	// This is the production case (after the first iteration every survivor has outgrown the seedling
	// query height); without it the reused buffer would still hold the PREVIOUS iteration's raster.
	if (bWantTransmittance && !bTransmittanceCaptured)
	{
		ArcForestLightModelLocal::EmitTransmittance(
			OutTransmittance->Values, IncludedBandGrids, NumIncludedBands, Grid, RasterLayout, ExtinctionK);
	}
}
