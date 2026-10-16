// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcTerrainDensityTiers.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainModule.h"
#include "Render/ArcTerrainTileAtlas.h"

namespace ArcTerrainDensityTiersPrivate_3d81
{
	int32 NumLadderEntries(const AArcTerrain& Terrain)
	{
		return FMath::Min(Terrain.HeightTiers.Num(), ArcTerrain::MaxTiers - 1);
	}

	constexpr int32 MaxDerivedSectorBudget = 256;
	constexpr int32 SectorOffsetSteps = 32;
	constexpr int32 NumSectorOffsets = SectorOffsetSteps / 2 + 1;
	constexpr double MaxDerivedSectorRadii = 8.0;

	// 2.5 km at LOD1, doubling per enumerator above it and flat past LOD4.
	double PresetExtentCm(int32 Level)
	{
		constexpr int32 FinestLevel = static_cast<int32>(EArcTerrainHeightLOD::LOD1);
		constexpr int32 CoarsestLevel = static_cast<int32>(EArcTerrainHeightLOD::LOD4);
		const int32 Step = FMath::Clamp(Level - FinestLevel, 0, CoarsestLevel - FinestLevel);
		return 250000.0 * static_cast<double>(static_cast<int64>(1) << Step);
	}

	void ResolveLevels(const AArcTerrain& Terrain, TArray<int32, TInlineAllocator<ArcTerrain::MaxTiers>>& OutLevels)
	{
		constexpr int32 MaxLevel = static_cast<int32>(EArcTerrainHeightLOD::LOD4);
		const int32 NumAuthored = NumLadderEntries(Terrain);
		OutLevels.Reset();
		OutLevels.Reserve(NumAuthored);

		int32 Previous = 0;
		for (int32 Index = 0; Index < NumAuthored; ++Index)
		{
			const int32 Authored = static_cast<int32>(Terrain.HeightTiers[Index].LOD);
			const int32 Level =
				FMath::Clamp(Authored, Previous + 1, MaxLevel - (NumAuthored - 1 - Index));
			OutLevels.Add(Level);
			Previous = Level;
		}
	}

	void ResolveBandOuterRadii(const AArcTerrain& Terrain,
		const TArray<int32, TInlineAllocator<ArcTerrain::MaxTiers>>& Levels,
		TArray<double, TInlineAllocator<ArcTerrain::MaxTiers>>& OutRadii)
	{
		OutRadii.Reset();
		OutRadii.Add(static_cast<double>(Terrain.PreviewRadius));
		for (int32 Index = 0; Index < Levels.Num(); ++Index)
		{
			const FArcTerrainHeightTier& Authored = Terrain.HeightTiers[Index];
			const double AuthoredExtentCm = static_cast<double>(Authored.ExtentKm) * 100000.0;
			const double ExtentCm = AuthoredExtentCm > 0.0 ? AuthoredExtentCm : PresetExtentCm(Levels[Index]);
			OutRadii.Add(OutRadii[Index] + ExtentCm);
		}
	}

	int32 SectorsCoveringDisc(double RadiusCm, double SectorSideCm)
	{
		if (RadiusCm <= 0.0 || SectorSideCm <= 0.0)
		{
			return 1;
		}
		const double OffsetStep = 1.0 / static_cast<double>(SectorOffsetSteps);
		const double Radii = FMath::Min(RadiusCm / SectorSideCm, MaxDerivedSectorRadii)
			+ UE_DOUBLE_SQRT_2 * 0.5 * OffsetStep;
		const double RadiiSq = Radii * Radii;
		const int32 Span = FMath::FloorToInt32(Radii) + 1;

		int32 Best = 0;
		for (int32 OffsetYIndex = 0; OffsetYIndex < NumSectorOffsets; ++OffsetYIndex)
		{
			const double OffsetY = static_cast<double>(OffsetYIndex) * OffsetStep;
			int32 Counts[NumSectorOffsets] = {};
			for (int32 Y = -Span; Y <= Span; ++Y)
			{
				const double DistY = FMath::Max3(0.0, static_cast<double>(Y) - OffsetY,
					OffsetY - static_cast<double>(Y + 1));
				const double HalfWidthSq = RadiiSq - DistY * DistY;
				if (HalfWidthSq < 0.0)
				{
					continue;
				}
				const double HalfWidth = FMath::Sqrt(HalfWidthSq);
				for (int32 OffsetXIndex = 0; OffsetXIndex < NumSectorOffsets; ++OffsetXIndex)
				{
					const double OffsetX = static_cast<double>(OffsetXIndex) * OffsetStep;
					const int64 First = FMath::CeilToInt64(OffsetX - HalfWidth - 1.0);
					const int64 Last = FMath::FloorToInt64(OffsetX + HalfWidth);
					Counts[OffsetXIndex] += static_cast<int32>(Last - First + 1);
				}
			}
			for (int32 OffsetXIndex = 0; OffsetXIndex < NumSectorOffsets; ++OffsetXIndex)
			{
				Best = FMath::Max(Best, Counts[OffsetXIndex]);
			}
		}
		return FMath::Clamp(Best, 1, MaxDerivedSectorBudget);
	}

	// A tier funds the disc residency ENUMERATES, not the one it admits: a resident sector stays wanted
	// out to the unload radius, and the trim keeps the farthest, so under-funding sheds the near band.
	void ResolveSectorBudgets(const AArcTerrain& Terrain,
		const TArray<int32, TInlineAllocator<ArcTerrain::MaxTiers>>& Levels,
		const TArray<double, TInlineAllocator<ArcTerrain::MaxTiers>>& Radii,
		TArray<int32, TInlineAllocator<ArcTerrain::MaxTiers>>& OutBudgets, int32 CellSectorShift,
		int32 NumResolvedTiers)
	{
		const double PreviewHysteresis = FMath::Max(static_cast<double>(Terrain.PreviewRadiusHysteresis), 1.0);
		const double CellSizeCm = Terrain.Config.CellSize();
		const int32 BaseCellSectorShift = FMath::Clamp(CellSectorShift, 0, 6);
		// Budget 0 is the base tier, so ladder entry N lands at budget N + 1.
		const int32 LastLadderIndex = NumResolvedTiers - 2;

		OutBudgets.Reset();
		OutBudgets.Add(FMath::Max(Terrain.MaxResidentSectors, 1));
		for (int32 Index = 0; Index < Levels.Num(); ++Index)
		{
			const bool bLastTier = Index == LastLadderIndex;
			const double UnloadRadiusCm =
				bLastTier ? Radii[Index + 1] * PreviewHysteresis : Radii[Index + 1];
			const double SectorSideCm = CellSizeCm
				* static_cast<double>(static_cast<int64>(1) << (BaseCellSectorShift + Levels[Index]));
			OutBudgets.Add(SectorsCoveringDisc(UnloadRadiusCm, SectorSideCm));
		}
	}

	constexpr int32 MinCoarseCellWeightQuads = 4;

	int32 NumWeightLadderEntries(const AArcTerrain& Terrain)
	{
		return FMath::Min(Terrain.WeightTiers.Num(), ArcTerrain::MaxTiers - 1);
	}

	void ResolveWeightCumulativeShifts(const AArcTerrain& Terrain,
		TArray<int32, TInlineAllocator<ArcTerrain::MaxTiers>>& OutShifts)
	{
		constexpr int32 MaxShift = static_cast<int32>(EArcTerrainWeightLOD::LOD3);
		const int32 NumAuthored = NumWeightLadderEntries(Terrain);
		OutShifts.Reset();
		OutShifts.Reserve(NumAuthored);

		int32 Previous = 0;
		for (int32 Index = 0; Index < NumAuthored; ++Index)
		{
			const int32 Authored = static_cast<int32>(Terrain.WeightTiers[Index].LOD);
			const int32 Shift = FMath::Clamp(Authored, Previous + 1, MaxShift - (NumAuthored - 1 - Index));
			OutShifts.Add(Shift);
			Previous = Shift;
		}
	}

	void ResolveWeightBandOuterRadii(const AArcTerrain& Terrain,
		const TArray<int32, TInlineAllocator<ArcTerrain::MaxTiers>>& CumulativeShifts,
		TArray<double, TInlineAllocator<ArcTerrain::MaxTiers>>& OutRadii)
	{
		OutRadii.Reset();
		OutRadii.Add(static_cast<double>(Terrain.WeightResidencyRadius));

		for (int32 Index = 0; Index < CumulativeShifts.Num(); ++Index)
		{
			const double ExtentCm = static_cast<double>(Terrain.WeightTiers[Index].ExtentKm) * 100000.0;
			OutRadii.Add(OutRadii[Index] + ExtentCm);
		}
	}

	void ResolveWeightGroupBudgets(const AArcTerrain& Terrain,
		const TArray<int32, TInlineAllocator<ArcTerrain::MaxTiers>>& CumulativeShifts,
		const TArray<double, TInlineAllocator<ArcTerrain::MaxTiers>>& Radii,
		TArray<int32, TInlineAllocator<ArcTerrain::MaxTiers>>& OutBudgets, int32 WeightSliceShift,
		int32 NumResolvedTiers)
	{
		const double PreviewHysteresis = FMath::Max(static_cast<double>(Terrain.PreviewRadiusHysteresis), 1.0);
		const double CellSizeCm = Terrain.Config.CellSize();
		const int32 BaseWeightSliceShift = FMath::Clamp(WeightSliceShift, 0, 6);
		const int32 LastLadderIndex = NumResolvedTiers - 2;

		OutBudgets.Reset();
		OutBudgets.Add(FMath::Max(Terrain.MaxResidentWeightCells, 1));
		for (int32 Index = 0; Index < CumulativeShifts.Num(); ++Index)
		{
			const bool bLastTier = Index == LastLadderIndex;
			const double UnloadRadiusCm =
				bLastTier ? Radii[Index + 1] * PreviewHysteresis : Radii[Index + 1];
			const double GroupSideCm = CellSizeCm
				* static_cast<double>(
					static_cast<int64>(1) << (BaseWeightSliceShift + CumulativeShifts[Index]));
			OutBudgets.Add(SectorsCoveringDisc(UnloadRadiusCm, GroupSideCm));
		}
	}

	void CopyCoarseShifts(const TArray<int32>& AuthoredShifts,
		TArray<int32, TInlineAllocator<ArcTerrain::MaxTiers>>& OutCoarseShifts)
	{
		OutCoarseShifts.Reset();
		const int32 NumCoarse = FMath::Max(AuthoredShifts.Num() - 1, 0);
		OutCoarseShifts.Reserve(NumCoarse);
		for (int32 Index = 1; Index < AuthoredShifts.Num(); ++Index)
		{
			OutCoarseShifts.Add(AuthoredShifts[Index]);
		}
	}
}

int32 ArcTerrain::ResolveMaxSubdivisionDepth(int32 FaceQuads)
{
	const int32 Ceiling =
		2 * static_cast<int32>(FMath::FloorLog2(static_cast<uint32>(FMath::Max(FaceQuads, 2))));
	return FMath::Max(Ceiling, 2);
}

int32 ArcTerrain::SumAuthoredResidentSectors(const AArcTerrain& Terrain)
{
	using namespace ArcTerrainDensityTiersPrivate_3d81;

	TArray<int32, TInlineAllocator<MaxTiers>> Levels;
	ResolveLevels(Terrain, Levels);
	TArray<double, TInlineAllocator<MaxTiers>> Radii;
	ResolveBandOuterRadii(Terrain, Levels, Radii);
	TArray<int32, TInlineAllocator<MaxTiers>> Budgets;
	ResolveSectorBudgets(Terrain, Levels, Radii, Budgets, Terrain.SectorShift, Levels.Num() + 1);

	int32 Sum = 0;
	for (int32 Index = 0; Index < Budgets.Num(); ++Index)
	{
		Sum += Budgets[Index];
	}
	return Sum;
}

void ArcTerrain::ResolveAuthoredHeightTierShifts(const AArcTerrain& Terrain, TArray<int32>& OutCumulativeShifts)
{
	using namespace ArcTerrainDensityTiersPrivate_3d81;

	TArray<int32, TInlineAllocator<MaxTiers>> Levels;
	ResolveLevels(Terrain, Levels);

	OutCumulativeShifts.Reset();
	OutCumulativeShifts.Reserve(Levels.Num() + 1);
	OutCumulativeShifts.Add(0);
	OutCumulativeShifts.Append(Levels);
}

void ArcTerrain::ResolveAuthoredWeightTierShifts(const AArcTerrain& Terrain, int32 ResolvedWeightQuads,
	TArray<int32>& OutCumulativeShifts, bool bEmitWarnings)
{
	using namespace ArcTerrainDensityTiersPrivate_3d81;

	TArray<int32, TInlineAllocator<MaxTiers>> CumulativeShifts;
	ResolveWeightCumulativeShifts(Terrain, CumulativeShifts);

	const int32 BaseCellWeightQuads = FMath::Max(ResolvedWeightQuads, 0);
	static bool bWarnedWeightQuadsIndivisible = false;
	static bool bWarnedWeightQuadsBelowFloor = false;
	for (int32 Index = 0; Index < CumulativeShifts.Num(); ++Index)
	{
		const int32 CumulativeShift = CumulativeShifts[Index];
		const int32 CellQuads = BaseCellWeightQuads >> CumulativeShift;
		const bool bIndivisible = (CellQuads << CumulativeShift) != BaseCellWeightQuads;
		const bool bBelowFloor = CellQuads < MinCoarseCellWeightQuads;
		if (!bIndivisible && !bBelowFloor)
		{
			continue;
		}

		if (bEmitWarnings && bIndivisible && !bWarnedWeightQuadsIndivisible)
		{
			bWarnedWeightQuadsIndivisible = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[DensityTiers] %ls: weight tier %d coarsens by %d bits, which does not divide the %d-quad "
				"cell raster — that tier and the %d coarser one(s) are dropped. Author "
				"Config.WeightTexelSizeWorld so the cell raster carries the tier shifts.",
				*Terrain.GetName(), Index + 1, CumulativeShift, BaseCellWeightQuads,
				CumulativeShifts.Num() - Index - 1);
		}
		if (bEmitWarnings && !bIndivisible && bBelowFloor && !bWarnedWeightQuadsBelowFloor)
		{
			bWarnedWeightQuadsBelowFloor = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[DensityTiers] %ls: weight tier %d resolves to %d quads per cell, under the floor of %d — "
				"that tier and the %d coarser one(s) are dropped. Lower Config.WeightTexelSizeWorld or "
				"reduce the WeightTiers shifts.",
				*Terrain.GetName(), Index + 1, CellQuads, MinCoarseCellWeightQuads,
				CumulativeShifts.Num() - Index - 1);
		}

		CumulativeShifts.SetNum(Index);
		break;
	}

	OutCumulativeShifts.Reset();
	OutCumulativeShifts.Reserve(CumulativeShifts.Num() + 1);
	OutCumulativeShifts.Add(0);
	OutCumulativeShifts.Append(CumulativeShifts);
}

FArcTerrainTierLadder ArcTerrain::ResolveTierLadder(const AArcTerrain& Terrain,
	const FArcTerrainTileAtlasLayout& Layout, bool bEmitWarnings)
{
	using namespace ArcTerrainDensityTiersPrivate_3d81;

	const int32 NumAuthored = NumLadderEntries(Terrain);
	static bool bWarnedLadderTruncated = false;
	if (bEmitWarnings && Terrain.HeightTiers.Num() > NumAuthored && !bWarnedLadderTruncated)
	{
		bWarnedLadderTruncated = true;
		UE_LOGF(LogArcTerrain, Warning,
			"[DensityTiers] %ls: HeightTiers holds %d entries; the ring uniform carries %d, so the last "
			"%d are dropped and the far field ends where entry %d does.",
			*Terrain.GetName(), Terrain.HeightTiers.Num(), MaxTiers - 1,
			Terrain.HeightTiers.Num() - NumAuthored, NumAuthored);
	}

	const int32 Available = FMath::Max(Layout.MaxResidentSectors, 0);
	int32 NumTiers = NumAuthored + 1;
	static bool bWarnedLadderShortened = false;
	if (Available < NumTiers)
	{
		const int32 Dropped = NumTiers - FMath::Max(Available, 1);
		NumTiers = FMath::Max(Available, 1);
		if (bEmitWarnings && Dropped > 0 && !bWarnedLadderShortened)
		{
			bWarnedLadderShortened = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[DensityTiers] %ls: the atlas funds %d height slice(s), %d short of a slice per ladder "
				"entry — the %d coarsest tier(s) are dropped entirely and nothing bakes, streams or "
				"draws for them. Raise arc.Terrain.Atlas.HeightBudgetMB or lower SectorShift / "
				"CellExtent.",
				*Terrain.GetName(), Available, Dropped, Dropped);
		}
	}

	TArray<int32> AuthoredShifts;
	ResolveAuthoredHeightTierShifts(Terrain, AuthoredShifts);
	TArray<int32, TInlineAllocator<MaxTiers>> Levels;
	CopyCoarseShifts(AuthoredShifts, Levels);
	TArray<double, TInlineAllocator<MaxTiers>> Radii;
	ResolveBandOuterRadii(Terrain, Levels, Radii);
	TArray<int32, TInlineAllocator<MaxTiers>> Budgets;
	ResolveSectorBudgets(Terrain, Levels, Radii, Budgets, Layout.SectorShift, NumTiers);

	int32 AuthoredSum = 0;
	for (int32 Index = 0; Index < NumTiers; ++Index)
	{
		AuthoredSum += Budgets[Index];
	}

	TArray<int32, TInlineAllocator<MaxTiers>> Granted;
	Granted.Reserve(NumTiers);
	int32 Remaining = Available;
	for (int32 Index = 0; Index < NumTiers; ++Index)
	{
		const int32 AuthoredBudget = Budgets[Index];
		int32 Grant = 0;
		if (Available > 0)
		{
			const int64 Proportional =
				(static_cast<int64>(AuthoredBudget) * static_cast<int64>(Available))
				/ static_cast<int64>(AuthoredSum);
			const int32 TiersAfter = NumTiers - 1 - Index;
			Grant = FMath::Clamp(FMath::Min(AuthoredBudget, static_cast<int32>(Proportional)), 1,
				Remaining - TiersAfter);
		}
		Granted.Add(Grant);
		Remaining -= Grant;
	}

	const double PreviewHysteresis = FMath::Max(static_cast<double>(Terrain.PreviewRadiusHysteresis), 1.0);
	const double StreamHysteresis = FMath::Max(static_cast<double>(Terrain.StreamHysteresis), 1.0);
	const double AuthoredStreamRadius = FMath::Max(static_cast<double>(Terrain.StreamRadius), 0.0);
	const int32 BaseSectorQuadsShift = Layout.GetSectorQuadsShift();

	FArcTerrainTierLadder Ladder;
	Ladder.Tiers.SetNum(NumTiers);

	FResolvedTier& Base = Ladder.Tiers[0];
	Base.CumulativeShift = 0;
	Base.PreviewRadiusCm = Radii[0];
	Base.BandOuterRadiusCm = Base.PreviewRadiusCm;
	const double BasePreviewUnloadRadiusCm = Base.PreviewRadiusCm * PreviewHysteresis;
	Base.StreamRadiusCm = FMath::Max(AuthoredStreamRadius, BasePreviewUnloadRadiusCm);
	Base.StreamUnloadRadiusCm = Base.StreamRadiusCm * StreamHysteresis;
	Base.SectorQuadsShift = BaseSectorQuadsShift;
	Base.CellSectorShift = Layout.SectorShift;
	Base.MaxResidentSectors = Granted[0];

	for (int32 Index = 1; Index < NumTiers; ++Index)
	{
		const int32 Level = Levels[Index - 1];
		const int32 CumulativeShift = Level;

		FResolvedTier& Tier = Ladder.Tiers[Index];
		Tier.CumulativeShift = CumulativeShift;
		Tier.SectorQuadsShift = BaseSectorQuadsShift + CumulativeShift;
		Tier.CellSectorShift = Layout.SectorShift + CumulativeShift;
		// Bands are additive, so no floor against the previous entry can be needed: this one starts exactly
		// where the previous one stops drawing and reaches a positive extent past it.
		Tier.PreviewRadiusCm = Radii[Index - 1];
		Tier.BandOuterRadiusCm = Radii[Index];
		Tier.BandUnloadRadiusCm = Tier.BandOuterRadiusCm * PreviewHysteresis;
		Tier.StreamRadiusCm = Tier.BandOuterRadiusCm * PreviewHysteresis;
		Tier.StreamUnloadRadiusCm = Tier.StreamRadiusCm * StreamHysteresis;
		Tier.MaxResidentSectors = Granted[Index];
	}

	return Ladder;
}

FArcTerrainWeightLadder ArcTerrain::ResolveWeightTierLadder(const AArcTerrain& Terrain,
	const FArcTerrainTileAtlasLayout& Layout, int32 ResolvedCellWeightQuads, bool bEmitWarnings)
{
	using namespace ArcTerrainDensityTiersPrivate_3d81;

	const int32 NumAuthored = NumWeightLadderEntries(Terrain);
	static bool bWarnedWeightLadderTruncated = false;
	if (bEmitWarnings && Terrain.WeightTiers.Num() > NumAuthored && !bWarnedWeightLadderTruncated)
	{
		bWarnedWeightLadderTruncated = true;
		UE_LOGF(LogArcTerrain, Warning,
			"[DensityTiers] %ls: WeightTiers holds %d entries; the ring uniform carries %d, so the last "
			"%d are dropped and the far field ends where entry %d does.",
			*Terrain.GetName(), Terrain.WeightTiers.Num(), MaxTiers - 1,
			Terrain.WeightTiers.Num() - NumAuthored, NumAuthored);
	}

	const int32 BaseCellWeightQuads = FMath::Max(ResolvedCellWeightQuads, 0);

	TArray<int32> AuthoredShifts;
	ResolveAuthoredWeightTierShifts(Terrain, ResolvedCellWeightQuads, AuthoredShifts, bEmitWarnings);
	TArray<int32, TInlineAllocator<MaxTiers>> CumulativeShifts;
	CopyCoarseShifts(AuthoredShifts, CumulativeShifts);

	// Group-span, missing ExtentKm, and atlas budget are residency drops — bake keeps those rungs.
	const int32 CellsPerWeightTileEdge = Terrain.Config.CellsPerTileEdge(Terrain.Config.WeightTileExtent);
	const int32 CellsPerGroupEdge = 1 << FMath::Clamp(Layout.WeightSliceShift, 0, 6);
	static bool bWarnedWeightGroupSpansTiles = false;
	if (CellsPerWeightTileEdge < CellsPerGroupEdge)
	{
		// A group wider than a tile has no single tile to cut its rect out of, at ANY tier.
		if (bEmitWarnings && !CumulativeShifts.IsEmpty() && !bWarnedWeightGroupSpansTiles)
		{
			bWarnedWeightGroupSpansTiles = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[DensityTiers] %ls: a weight group spans %d cells per edge but a weight tile only %d, so "
				"no coarse group fits in one tile and the whole coarse weight ladder (%d entries) is "
				"dropped. Raise Config.WeightTileExtent or lower WeightSliceShift.",
				*Terrain.GetName(), CellsPerGroupEdge, CellsPerWeightTileEdge, CumulativeShifts.Num());
		}
		CumulativeShifts.Reset();
	}

	static bool bWarnedWeightExtentMissing = false;
	for (int32 Index = 0; Index < CumulativeShifts.Num(); ++Index)
	{
		if (Terrain.WeightTiers[Index].ExtentKm > 0.0f)
		{
			continue;
		}

		if (bEmitWarnings && !bWarnedWeightExtentMissing)
		{
			bWarnedWeightExtentMissing = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[DensityTiers] %ls: weight tier %d has no authored Extent — that tier and the %d coarser "
				"one(s) are dropped. Author WeightTiers[%d].ExtentKm.",
				*Terrain.GetName(), Index + 1, CumulativeShifts.Num() - Index - 1, Index);
		}

		CumulativeShifts.SetNum(Index);
		break;
	}

	const int32 Available = FMath::Max(Layout.MaxResidentWeightCells, 0);
	int32 NumTiers = CumulativeShifts.Num() + 1;
	static bool bWarnedWeightLadderShortened = false;
	if (Available < NumTiers)
	{
		const int32 Dropped = NumTiers - FMath::Max(Available, 1);
		NumTiers = FMath::Max(Available, 1);
		if (bEmitWarnings && Dropped > 0 && !bWarnedWeightLadderShortened)
		{
			bWarnedWeightLadderShortened = true;
			UE_LOGF(LogArcTerrain, Warning,
				"[DensityTiers] %ls: the atlas funds %d weight slice(s), %d short of a slice per ladder "
				"entry — the %d coarsest tier(s) are dropped entirely and nothing bakes, streams or "
				"draws for them. Raise arc.Terrain.Atlas.WeightBudgetMB / MaxResidentWeightCells or "
				"lower WeightSliceShift.",
				*Terrain.GetName(), Available, Dropped, Dropped);
		}
	}

	TArray<double, TInlineAllocator<MaxTiers>> Radii;
	ResolveWeightBandOuterRadii(Terrain, CumulativeShifts, Radii);
	TArray<int32, TInlineAllocator<MaxTiers>> Budgets;
	ResolveWeightGroupBudgets(Terrain, CumulativeShifts, Radii, Budgets, Layout.WeightSliceShift, NumTiers);

	int32 AuthoredSum = 0;
	for (int32 Index = 0; Index < NumTiers; ++Index)
	{
		AuthoredSum += Budgets[Index];
	}

	TArray<int32, TInlineAllocator<MaxTiers>> Granted;
	Granted.Reserve(NumTiers);
	int32 Remaining = Available;
	for (int32 Index = 0; Index < NumTiers; ++Index)
	{
		const int32 AuthoredBudget = Budgets[Index];
		int32 Grant = 0;
		if (Available > 0)
		{
			const int64 Proportional =
				(static_cast<int64>(AuthoredBudget) * static_cast<int64>(Available))
				/ static_cast<int64>(AuthoredSum);
			const int32 TiersAfter = NumTiers - 1 - Index;
			Grant = FMath::Clamp(FMath::Min(AuthoredBudget, static_cast<int32>(Proportional)), 1,
				Remaining - TiersAfter);
		}
		Granted.Add(Grant);
		Remaining -= Grant;
	}

	const double PreviewHysteresis = FMath::Max(static_cast<double>(Terrain.PreviewRadiusHysteresis), 1.0);
	const double StreamHysteresis = FMath::Max(static_cast<double>(Terrain.StreamHysteresis), 1.0);
	const double AuthoredWeightStreamRadius =
		FMath::Max(static_cast<double>(Terrain.WeightStreamRadius), 0.0);
	const int32 BaseWeightQuadsShift = Layout.GetWeightQuadsShift();

	FArcTerrainWeightLadder Ladder;
	Ladder.Tiers.SetNum(NumTiers);

	FResolvedWeightTier& Base = Ladder.Tiers[0];
	Base.CumulativeShift = 0;
	Base.PreviewRadiusCm = Radii[0];
	Base.BandOuterRadiusCm = Base.PreviewRadiusCm;
	Base.BandUnloadRadiusCm = Base.BandOuterRadiusCm * PreviewHysteresis;
	Base.StreamRadiusCm = FMath::Max(AuthoredWeightStreamRadius, Base.BandUnloadRadiusCm);
	Base.StreamUnloadRadiusCm = Base.StreamRadiusCm * StreamHysteresis;
	Base.SliceShift = Layout.WeightSliceShift;
	Base.WeightQuadsShift = BaseWeightQuadsShift;
	Base.CellWeightQuads = BaseCellWeightQuads;
	Base.MaxResidentWeightGroups = Granted[0];

	for (int32 Index = 1; Index < NumTiers; ++Index)
	{
		const int32 CumulativeShift = CumulativeShifts[Index - 1];

		FResolvedWeightTier& Tier = Ladder.Tiers[Index];
		Tier.CumulativeShift = CumulativeShift;
		Tier.SliceShift = Layout.WeightSliceShift + CumulativeShift;
		Tier.WeightQuadsShift = BaseWeightQuadsShift + CumulativeShift;
		Tier.CellWeightQuads = BaseCellWeightQuads >> CumulativeShift;
		Tier.PreviewRadiusCm = Radii[Index - 1];
		Tier.BandOuterRadiusCm = Radii[Index];
		Tier.BandUnloadRadiusCm = Tier.BandOuterRadiusCm * PreviewHysteresis;
		Tier.StreamRadiusCm = Tier.BandOuterRadiusCm * PreviewHysteresis;
		Tier.StreamUnloadRadiusCm = Tier.StreamRadiusCm * StreamHysteresis;
		Tier.MaxResidentWeightGroups = Granted[Index];
	}

	return Ladder;
}
