// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainModule.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileManifest.h"
#include "ArcTerrainTileNames.h"
#include "Components/SceneComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Render/ArcTerrainTileAtlas.h"
#include "RHIGlobals.h"

// FILE-SCOPED and unique (CLAUDE.md C++ Code Style) — nothing here may gain external linkage.
namespace ArcTerrainActorPrivate_4c07
{
	uint32 Metres(EArcTerrainExtent Extent)
	{
		return static_cast<uint32>(Extent);
	}

	/** D3D12's 2D limit — the smallest of the platforms this project targets — when the RHI has not
	 *  reported yet (commandlets, early startup). Mirrors ArcTerrainTileAtlas.cpp. */
	constexpr int32 FallbackMaxTextureDimension = 16384;

	int32 ResolveMaxTextureDimension()
	{
		const int32 Reported = static_cast<int32>(GetMax2DTextureDimension());
		return Reported > 0 ? Reported : FallbackMaxTextureDimension;
	}

	/** Slice side (texels) a shift resolves to, in int64 so an out-of-range authored shift reports
	 *  instead of wrapping. */
	int64 SliceSide(int32 Quads, int32 Shift, int32 Apron)
	{
		return (static_cast<int64>(Quads) << FMath::Clamp(Shift, 0, 30)) + Apron;
	}

	/** Largest shift whose slice side stays within MaxSide. Only meaningful when shift 0 already fits;
	 *  the caller reports that case separately. */
	int32 LargestFittingShift(int32 Quads, int32 Apron, int32 MaxSide)
	{
		int32 Shift = 0;
		while (Shift < 30 && SliceSide(Quads, Shift + 1, Apron) <= MaxSide)
		{
			++Shift;
		}
		return Shift;
	}

	double Megabytes(int64 Bytes)
	{
		return static_cast<double>(Bytes) / (1024.0 * 1024.0);
	}

	/** Largest enumerated extent (metres) whose texel count at TexelCm stays within MaxQuads; 0 when even
	 *  the smallest enumerator is over. */
	uint32 LargestExtentWithinQuads(int32 TexelCm, int32 MaxQuads)
	{
		if (TexelCm <= 0)
		{
			return 0;
		}
		uint32 Best = 0;
		for (EArcTerrainExtent Candidate : ArcTerrain::GExtentValues)
		{
			const int64 Quads = static_cast<int64>(Metres(Candidate)) * 100 / TexelCm;
			if (Quads <= MaxQuads && Metres(Candidate) > Best)
			{
				Best = Metres(Candidate);
			}
		}
		return Best;
	}

	/** Largest enumerated extent (metres) holding at most MaxQuads weight texels a side, given
	 *  CellWeightQuads texels per CellMetres of cell. */
	uint32 LargestWeightExtentWithinQuads(uint32 CellMetres, int32 CellWeightQuads, int32 MaxQuads)
	{
		if (CellMetres == 0u || CellWeightQuads <= 0)
		{
			return 0;
		}
		uint32 Best = 0;
		for (EArcTerrainExtent Candidate : ArcTerrain::GExtentValues)
		{
			const int64 Quads =
				static_cast<int64>(Metres(Candidate) / CellMetres) * static_cast<int64>(CellWeightQuads);
			if (Quads <= MaxQuads && Metres(Candidate) > Best)
			{
				Best = Metres(Candidate);
			}
		}
		return Best;
	}
}

AArcTerrain::AArcTerrain()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

#if WITH_EDITORONLY_DATA
	// The terrain manages its own cell streaming (invoker rings); World Partition must NOT stream the
	// manager actor by pivot/bounds. The stamp index + terrain frame must stay resident for every cell
	// they affect, so mark the actor always-loaded (WP bakes this into the runtime grid at cook — the
	// flag is editor-only data). Same pattern as AInfo / AWorldSettings-style always-loaded actors.
	bIsSpatiallyLoaded = false;
#endif // WITH_EDITORONLY_DATA

	// 384 slices is 103 MB at N<=4, 206 MB at N<=8 — both inside the 512 MB WeightBudgetMB; past that
	// the ladder self-shortens.
	MaxResidentWeightCells = 384;
	WeightTiers.SetNum(3);
	WeightTiers[0].LOD = EArcTerrainWeightLOD::LOD1;
	WeightTiers[0].ExtentKm = 2.f;
	WeightTiers[1].LOD = EArcTerrainWeightLOD::LOD2;
	WeightTiers[1].ExtentKm = 4.f;
	WeightTiers[2].LOD = EArcTerrainWeightLOD::LOD3;
	WeightTiers[2].ExtentKm = 8.f;
}

TArray<FName> AArcTerrain::GetTerrainChannels() const
{
	if (const UArcTerrainChannelSet* ChannelSet = Config.ChannelSet)
	{
		return ChannelSet->Channels;
	}
	return TArray<FName>();
}

FTransform AArcTerrain::GetTerrainTransform() const
{
	return GetActorTransform();
}

FVector AArcTerrain::WorldToTerrainLocal(const FVector& WorldPos) const
{
	return GetTerrainTransform().InverseTransformPosition(WorldPos);
}

void AArcTerrain::RefreshDerivedGridSummary()
{
	using namespace ArcTerrainActorPrivate_4c07;

	const int32 HeightTileRes = Config.HeightTileQuads() + 3;
	const int32 WeightTileRes = Config.AuthoredWeightTileQuads() + 3;
	const int32 DataTileRes = Config.DataTileQuads() + 3;
	const int32 HeightSliceSide = (Config.QuadsPerCell() << FMath::Max(SectorShift, 0)) + 3;

	const double HeightTileBytes = static_cast<double>(HeightTileRes) * HeightTileRes * 8.0;
	const double WeightTileBytes = static_cast<double>(WeightTileRes) * WeightTileRes;
	const double DataTileBytes = static_cast<double>(DataTileRes) * DataTileRes;

	Config.DerivedGridSummary = FString::Printf(
		TEXT("cell %d tex | height tile %d² = %.2f MB | weight tile (authored) %d² = %.2f MB/plane | ")
		TEXT("data tile %d² = %.0f KB/mask | height slice %d²"),
		Config.QuadsPerCell(), HeightTileRes, HeightTileBytes / 1.0e6, WeightTileRes,
		WeightTileBytes / 1.0e6, DataTileRes, DataTileBytes / 1.0e3, HeightSliceSide);

	const int32 HeightQuads = Config.QuadsPerCell();
	const int32 AuthoredSectors = ArcTerrain::SumAuthoredResidentSectors(*this);
	const FArcTerrainTileAtlasLayout SummaryLayout = ArcTerrainAtlas::ResolveTileAtlasLayout(HeightQuads,
		ArcTerrainAtlas::WeightQuadsUpperBound(HeightQuads, Config.AuthoredCellWeightQuads()),
		GetTerrainChannels().Num(), AuthoredSectors, MaxResidentWeightCells,
		SectorShift, WeightSliceShift, HeightOptions.BaseHeightCm, false);
	const bool bSummaryLayoutValid = SummaryLayout.IsValid();
	for (FArcTerrainHeightTier& AuthoredTier : HeightTiers)
	{
		AuthoredTier.RowSummary = bSummaryLayoutValid
			? TEXT("dropped — the ladder funds no slice for this entry")
			: TEXT("no atlas — the height slice layout does not resolve at this grid");
	}
	for (FArcTerrainWeightTier& AuthoredWeightTier : WeightTiers)
	{
		AuthoredWeightTier.RowSummary = bSummaryLayoutValid
			? TEXT("dropped — the ladder funds no slice for this entry")
			: TEXT("no atlas — the weight slice layout does not resolve at this grid");
	}

	if (bSummaryLayoutValid)
	{
		const FArcTerrainTierLadder SummaryLadder =
			ArcTerrain::ResolveTierLadder(*this, SummaryLayout, false);
		const int64 SummarySliceBytes =
			ArcTerrainAtlas::HeightSliceBytes(SummaryLayout.GetHeightSliceSide());
		for (int32 Tier = 1; Tier < SummaryLadder.Tiers.Num(); ++Tier)
		{
			if (!HeightTiers.IsValidIndex(Tier - 1))
			{
				continue;
			}
			const FResolvedTier& TierEntry = SummaryLadder.Tiers[Tier];
			const int32 ResolvedLevel = TierEntry.CumulativeShift
				- static_cast<int32>(EArcTerrainHeightLOD::LOD1) + 1;
			HeightTiers[Tier - 1].RowSummary = FString::Printf(
				TEXT("LOD%d · %.0f cm/tex · %.1f – %.1f km · %d slice · %.0f MB"), ResolvedLevel,
				static_cast<double>(Config.TexelSizeCm()) * (1 << TierEntry.CumulativeShift),
				TierEntry.PreviewRadiusCm * 0.00001, TierEntry.BandOuterRadiusCm * 0.00001,
				TierEntry.MaxResidentSectors,
				Megabytes(SummarySliceBytes * static_cast<int64>(TierEntry.MaxResidentSectors)));
		}

		const FArcTerrainWeightLadder SummaryWeightLadder =
			ArcTerrain::ResolveWeightTierLadder(*this, SummaryLayout, SummaryLayout.WeightQuads, false);
		const int64 SummaryWeightSliceBytes = ArcTerrainAtlas::WeightSliceBytes(
			SummaryLayout.GetWeightSliceSide(), SummaryLayout.NumWeightArrays);
		for (int32 Tier = 1; Tier < SummaryWeightLadder.Tiers.Num(); ++Tier)
		{
			if (!WeightTiers.IsValidIndex(Tier - 1))
			{
				continue;
			}
			const FResolvedWeightTier& WeightTierEntry = SummaryWeightLadder.Tiers[Tier];
			const int32 ResolvedWeightLevel = WeightTierEntry.CumulativeShift
				- static_cast<int32>(EArcTerrainWeightLOD::LOD1) + 1;
			WeightTiers[Tier - 1].RowSummary = FString::Printf(
				TEXT("LOD%d · %.0f cm/tex · %.1f – %.1f km · %d slice · %.0f MB"), ResolvedWeightLevel,
				static_cast<double>(Config.WeightTexelSizeWorld) * (1 << WeightTierEntry.CumulativeShift),
				WeightTierEntry.PreviewRadiusCm * 0.00001, WeightTierEntry.BandOuterRadiusCm * 0.00001,
				WeightTierEntry.MaxResidentWeightGroups,
				Megabytes(SummaryWeightSliceBytes
					* static_cast<int64>(WeightTierEntry.MaxResidentWeightGroups)));
		}

		if (SummaryLadder.Tiers.Num() > 1)
		{
			TArray<FString> TierLines;
			for (int32 Tier = 0; Tier < SummaryLadder.Tiers.Num(); ++Tier)
			{
				const FResolvedTier& TierEntry = SummaryLadder.Tiers[Tier];
				const double SectorWorldSideM =
					static_cast<double>(
						static_cast<int64>(Config.TexelSizeCm()) << TierEntry.SectorQuadsShift) / 100.0;
				const double CmPerTexel =
					static_cast<double>(Config.TexelSizeCm()) * (1 << TierEntry.CumulativeShift);
				TierLines.Add(FString::Printf(
					TEXT("tier%d: sector %.0f m, %.0f cm/tex, preview %.2f km, stream %.2f km"),
					Tier, SectorWorldSideM, CmPerTexel,
					TierEntry.PreviewRadiusCm * 0.00001, TierEntry.StreamRadiusCm * 0.00001));
			}
			Config.DerivedGridSummary += TEXT(" || ") + FString::Join(TierLines, TEXT(" | "));
		}

		const int64 HeightBudgetBytes = ArcTerrainAtlas::GetHeightBudgetBytes();
		const int32 SlicesInBudget =
			ArcTerrainAtlas::MaxAllocatableSlicesWithinBudget(SummarySliceBytes, HeightBudgetBytes);
		Config.DerivedGridSummary += FString::Printf(
			TEXT(" || atlas allocates %d slice + 1 fill slice = %.0f MB, ladder asks %d, budget fits %d ")
			TEXT("slice + 1 fill slice = %.0f MB of %.0f MB"),
			SummaryLayout.MaxResidentSectors,
			Megabytes(SummarySliceBytes * (static_cast<int64>(SummaryLayout.MaxResidentSectors) + 1)),
			AuthoredSectors, SlicesInBudget,
			Megabytes(SummarySliceBytes * (static_cast<int64>(SlicesInBudget) + 1)),
			Megabytes(HeightBudgetBytes));
	}

	if (!GridInvariantReport.IsEmpty())
	{
		Config.DerivedGridSummary =
			FString::Printf(TEXT("INVALID: %s || %s"), *GridInvariantReport, *Config.DerivedGridSummary);
	}
}

void AArcTerrain::GatherGridViolations(TArray<FString>& Violations) const
{
	using namespace ArcTerrainActorPrivate_4c07;

	const uint32 CellMetres = Metres(Config.CellExtent);
	const int32 TexelCm = Config.TexelSizeCm();
	if (TexelCm <= 0 || CellMetres == 0u)
	{
		Violations.Add(FString::Printf(
			TEXT("TexelSize (%d cm) / CellExtent (%u m) holds an enumerator this build does not know — ")
			TEXT("pick both from the list. Every derived count below is meaningless until then."),
			TexelCm, CellMetres));
		return;
	}

	const int32 HeightQuads = Config.QuadsPerCell();
	const int32 CellWeightQuads = Config.AuthoredCellWeightQuads();

	const EArcTerrainExtent TileExtents[] = {
		Config.HeightTileExtent, Config.WeightTileExtent, Config.DataTileExtent };
	const TCHAR* const TileExtentNames[] = {
		TEXT("HeightTileExtent"), TEXT("WeightTileExtent"), TEXT("DataTileExtent") };

	for (int32 RailIndex = 0; RailIndex < UE_ARRAY_COUNT(TileExtents); ++RailIndex)
	{
		if (Metres(TileExtents[RailIndex]) >= CellMetres)
		{
			continue;
		}
		Violations.Add(FString::Printf(
			TEXT("%s (%u m) is below CellExtent (%u m) — raise %s to at least %u m. One cell's payload ")
			TEXT("has to land in ONE file; a tile narrower than a cell splits it across two."),
			TileExtentNames[RailIndex], Metres(TileExtents[RailIndex]), CellMetres,
			TileExtentNames[RailIndex], CellMetres));
	}

	if (WeightSliceShift > SectorShift)
	{
		Violations.Add(FString::Printf(
			TEXT("WeightSliceShift (%d) is above SectorShift (%d) — lower WeightSliceShift to %d. A ")
			TEXT("weight slice spanning two sectors needs a refcount the eviction path does not have, so ")
			TEXT("the atlas resolves it down and the panel would otherwise disagree with the atlas."),
			WeightSliceShift, SectorShift, SectorShift));
	}

	const int32 MaxTextureDim = ResolveMaxTextureDimension();
	if (HeightQuads + 3 > MaxTextureDim)
	{
		Violations.Add(FString::Printf(
			TEXT("CellExtent %u m at %d cm/texel is %d texels a side, past this device's %d-texel texture ")
			TEXT("limit on its own — no SectorShift fixes it. Lower CellExtent or coarsen TexelSize."),
			CellMetres, TexelCm, HeightQuads + 3, MaxTextureDim));
	}
	else
	{
		const int32 MaxSectorShift = LargestFittingShift(HeightQuads, 3, MaxTextureDim);
		if (SectorShift > MaxSectorShift)
		{
			Violations.Add(FString::Printf(
				TEXT("SectorShift %d makes the height atlas slice %lld texels a side, past this device's ")
				TEXT("%d-texel texture limit — set SectorShift to at most %d."),
				SectorShift, SliceSide(HeightQuads, SectorShift, 3), MaxTextureDim, MaxSectorShift));
		}
	}

	constexpr int32 MaxWeightSliceQuads = ArcTerrain::MaxWeightSliceSide - 3;
	if (CellWeightQuads > MaxWeightSliceQuads)
	{
		if (HeightQuads > MaxWeightSliceQuads)
		{
			Violations.Add(FString::Printf(
				TEXT("the weight raster is floored at the height raster (%d texels a cell), already past ")
				TEXT("the %d-texel weight slice — no Weight Texel Size fixes it. Lower CellExtent or ")
				TEXT("coarsen TexelSize."),
				HeightQuads, ArcTerrain::MaxWeightSliceSide));
		}
		else
		{
			Violations.Add(FString::Printf(
				TEXT("Weight Texel Size %.3f cm resolves to %d weight texels a cell, past the %d-texel ")
				TEXT("weight slice — raise Weight Texel Size to at least %.3f cm."),
				Config.WeightTexelSizeWorld, CellWeightQuads, ArcTerrain::MaxWeightSliceSide,
				Config.CellSize() / static_cast<double>(MaxWeightSliceQuads)));
		}
	}
	else
	{
		const int32 MaxWeightSliceShift =
			LargestFittingShift(CellWeightQuads, 3, ArcTerrain::MaxWeightSliceSide);
		if (WeightSliceShift > MaxWeightSliceShift)
		{
			Violations.Add(FString::Printf(
				TEXT("WeightSliceShift %d makes the weight atlas slice %lld texels a side, past %d — set ")
				TEXT("WeightSliceShift to at most %d."),
				WeightSliceShift, SliceSide(CellWeightQuads, WeightSliceShift, 3),
				ArcTerrain::MaxWeightSliceSide, MaxWeightSliceShift));
		}
	}

	const EArcTerrainExtent HeightRailExtents[] = { Config.HeightTileExtent, Config.DataTileExtent };
	const TCHAR* const HeightRailNames[] = { TEXT("HeightTileExtent"), TEXT("DataTileExtent") };
	const uint32 MaxHeightRailMetres = LargestExtentWithinQuads(TexelCm, ArcTerrain::MaxTileQuads);
	for (int32 RailIndex = 0; RailIndex < UE_ARRAY_COUNT(HeightRailExtents); ++RailIndex)
	{
		const int32 RailQuads = Config.TileQuads(HeightRailExtents[RailIndex]);
		if (RailQuads <= ArcTerrain::MaxTileQuads)
		{
			continue;
		}
		if (MaxHeightRailMetres >= CellMetres)
		{
			Violations.Add(FString::Printf(
				TEXT("%s %u m at %d cm/texel is %d texels a side, past the %d-texel tile ceiling — lower ")
				TEXT("%s to at most %u m."),
				HeightRailNames[RailIndex], Metres(HeightRailExtents[RailIndex]), TexelCm, RailQuads,
				ArcTerrain::MaxTileQuads, HeightRailNames[RailIndex], MaxHeightRailMetres));
		}
		else
		{
			Violations.Add(FString::Printf(
				TEXT("%s %u m is %d texels a side, past the %d-texel tile ceiling, and no listed extent is ")
				TEXT("both within it and at least CellExtent (%u m) — lower CellExtent or coarsen ")
				TEXT("TexelSize."),
				HeightRailNames[RailIndex], Metres(HeightRailExtents[RailIndex]), RailQuads,
				ArcTerrain::MaxTileQuads, CellMetres));
		}
	}

	const int32 PatchHeightQuads = Config.TileQuads(CollisionPatchExtent);
	if (PatchHeightQuads > ArcTerrain::MaxTileQuads)
	{
		Violations.Add(FString::Printf(
			TEXT("CollisionPatchExtent %u m at %d cm/texel is %d height texels a side, past the %d-texel ")
			TEXT("ceiling — collision cannot form a heightfield on this lattice. Lower ")
			TEXT("CollisionPatchExtent to at most %u m."),
			Metres(CollisionPatchExtent), TexelCm, PatchHeightQuads, ArcTerrain::MaxTileQuads,
			LargestExtentWithinQuads(TexelCm, ArcTerrain::MaxTileQuads)));
	}

	const int32 WeightTileQuads = Config.AuthoredWeightTileQuads();
	if (CellWeightQuads > 0 && WeightTileQuads > ArcTerrain::MaxTileQuads)
	{
		const uint32 MaxWeightRailMetres =
			LargestWeightExtentWithinQuads(CellMetres, CellWeightQuads, ArcTerrain::MaxTileQuads);
		if (MaxWeightRailMetres >= CellMetres)
		{
			Violations.Add(FString::Printf(
				TEXT("WeightTileExtent %u m holds %d weight texels a side at %d texels a cell, past the ")
				TEXT("%d-texel tile ceiling — lower WeightTileExtent to at most %u m."),
				Metres(Config.WeightTileExtent), WeightTileQuads, CellWeightQuads,
				ArcTerrain::MaxTileQuads, MaxWeightRailMetres));
		}
		else
		{
			Violations.Add(FString::Printf(
				TEXT("WeightTileExtent %u m holds %d weight texels a side, past the %d-texel tile ceiling, ")
				TEXT("and CellExtent (%u m) alone already holds %d — coarsen Weight Texel Size or lower ")
				TEXT("CellExtent."),
				Metres(Config.WeightTileExtent), WeightTileQuads, ArcTerrain::MaxTileQuads, CellMetres,
				CellWeightQuads));
		}
	}

	// The side checks above bound ONE slice against a texture dimension. The array is MaxResident* slices
	// of that side plus the fill slice in a SINGLE committed resource, and that total is what a driver
	// refuses — no dimension or layer limit is reached first.
	const int64 HeightSliceSideTexels = SliceSide(HeightQuads, SectorShift, 3);
	const int32 AuthoredSectors = ArcTerrain::SumAuthoredResidentSectors(*this);
	const int32 NumAuthoredTiers = FMath::Min(HeightTiers.Num(), ArcTerrain::MaxTiers - 1);
	if (HeightSliceSideTexels <= MaxTextureDim)
	{
		const int64 HeightSliceCost = ArcTerrainAtlas::HeightSliceBytes(HeightSliceSideTexels);
		const int64 HeightBudget = ArcTerrainAtlas::GetHeightBudgetBytes();
		const int32 SectorsInBudget =
			ArcTerrainAtlas::MaxAllocatableSlicesWithinBudget(HeightSliceCost, HeightBudget);
		if (SectorsInBudget < 1)
		{
			Violations.Add(FString::Printf(
				TEXT("one height atlas slice is %lld² x 4 B = %.1f MB, past arc.Terrain.Atlas.HeightBudgetMB ")
				TEXT("(%.0f MB) on its own — no MaxResidentSectors fits. Lower SectorShift or CellExtent, ")
				TEXT("coarsen TexelSize, or raise the budget."),
				HeightSliceSideTexels, Megabytes(HeightSliceCost), Megabytes(HeightBudget)));
		}
		else if (AuthoredSectors > SectorsInBudget)
		{
			Violations.Add(FString::Printf(
				TEXT("MaxResidentSectors %d plus %d ladder tier(s) asks for %d height slices = %.1f MB in one ")
				TEXT("resource (%d slices + 1 fill slice at %lld² x 4 B), past ")
				TEXT("arc.Terrain.Atlas.HeightBudgetMB (%.0f MB) — bring the TOTAL down to at most %d ")
				TEXT("(%.1f MB), on MaxResidentSectors or by removing a HeightTiers entry. The ")
				TEXT("atlas trims to that at runtime and the ladder then drops its coarsest tiers, so ")
				TEXT("residency silently ends shorter than the radii ask for."),
				MaxResidentSectors, NumAuthoredTiers, AuthoredSectors,
				Megabytes(HeightSliceCost * (static_cast<int64>(AuthoredSectors) + 1)), AuthoredSectors,
				HeightSliceSideTexels, Megabytes(HeightBudget), SectorsInBudget,
				Megabytes(HeightSliceCost * (static_cast<int64>(SectorsInBudget) + 1))));
		}
	}

	const int32 WeightQuads = ArcTerrainAtlas::WeightQuadsUpperBound(HeightQuads, CellWeightQuads);
	const int64 WeightSliceSideTexels = SliceSide(WeightQuads, WeightSliceShift, 3);
	if (WeightSliceSideTexels <= ArcTerrain::MaxWeightSliceSide)
	{
		const int32 NumWeightArrays = FMath::Max(FMath::DivideAndRoundUp(GetTerrainChannels().Num(), 4), 1);
		const int64 WeightSliceCost =
			ArcTerrainAtlas::WeightSliceBytes(WeightSliceSideTexels, NumWeightArrays);
		const int64 WeightBudget = ArcTerrainAtlas::GetWeightBudgetBytes();
		const int32 CellsInBudget =
			ArcTerrainAtlas::MaxAllocatableSlicesWithinBudget(WeightSliceCost, WeightBudget);
		if (CellsInBudget < 1)
		{
			Violations.Add(FString::Printf(
				TEXT("one weight atlas slice is %lld² x 4 B x %d arrays = %.1f MB, past ")
				TEXT("arc.Terrain.Atlas.WeightBudgetMB (%.0f MB) on its own — no MaxResidentWeightCells ")
				TEXT("fits. Lower WeightSliceShift, coarsen Weight Texel Size, or raise the budget."),
				WeightSliceSideTexels, NumWeightArrays, Megabytes(WeightSliceCost),
				Megabytes(WeightBudget)));
		}
		else if (MaxResidentWeightCells > CellsInBudget)
		{
			Violations.Add(FString::Printf(
				TEXT("MaxResidentWeightCells %d allocates %.1f MB of weight atlas (%d slices + 1 fill slice ")
				TEXT("at %lld² x 4 B x %d arrays), past arc.Terrain.Atlas.WeightBudgetMB (%.0f MB) — set ")
				TEXT("MaxResidentWeightCells to at most %d (%.1f MB). The atlas trims to that at runtime, so ")
				TEXT("the far cells shade with the base layer."),
				MaxResidentWeightCells,
				Megabytes(WeightSliceCost * (static_cast<int64>(MaxResidentWeightCells) + 1)),
				MaxResidentWeightCells, WeightSliceSideTexels, NumWeightArrays, Megabytes(WeightBudget),
				CellsInBudget, Megabytes(WeightSliceCost * (static_cast<int64>(CellsInBudget) + 1))));
		}
	}

	if (HeightTiers.IsEmpty())
	{
		return;
	}

	constexpr int32 MaxAuthoredTiers = ArcTerrain::MaxTiers - 1;
	if (HeightTiers.Num() > MaxAuthoredTiers)
	{
		Violations.Add(FString::Printf(
			TEXT("HeightTiers holds %d entries; the ring uniform carries %d, so the last %d are dropped and ")
			TEXT("never bake, stream or draw — remove them, or raise the LOD of the entries that stay."),
			HeightTiers.Num(), MaxAuthoredTiers, HeightTiers.Num() - MaxAuthoredTiers));
	}

	if (HeightQuads + 3 <= MaxTextureDim)
	{
		const int32 ResolvedSectorShift =
			FMath::Min(SectorShift, LargestFittingShift(HeightQuads, 3, MaxTextureDim));
		const uint32 RequiredTileMetres = CellMetres << ResolvedSectorShift;
		const uint32 LargestExtentMetres = Metres(EArcTerrainExtent::M8192);
		if (Metres(Config.HeightTileExtent) != RequiredTileMetres)
		{
			if (RequiredTileMetres <= LargestExtentMetres)
			{
				Violations.Add(FString::Printf(
					TEXT("HeightTileExtent %u m is not CellExtent (%u m) << SectorShift (%d, resolved) = %u m ")
					TEXT("— set HeightTileExtent to %u m. A coarse tier fills its whole slice from ONE tile, ")
					TEXT("so a tile that is not exactly a sector writes a rectangle of the wrong size with no ")
					TEXT("compile error and no runtime complaint."),
					Metres(Config.HeightTileExtent), CellMetres, ResolvedSectorShift, RequiredTileMetres,
					RequiredTileMetres));
			}
			else
			{
				int32 LargestTileShift = 0;
				while ((CellMetres << (LargestTileShift + 1)) <= LargestExtentMetres)
				{
					++LargestTileShift;
				}
				Violations.Add(FString::Printf(
					TEXT("CellExtent (%u m) << SectorShift (%d, resolved) is %u m and no listed ")
					TEXT("HeightTileExtent reaches it — lower SectorShift to %d. A coarse tier fills its ")
					TEXT("whole slice from ONE tile, so the tile has to be exactly a sector, and no ")
					TEXT("HeightTileExtent edit can satisfy that here."),
					CellMetres, ResolvedSectorShift, RequiredTileMetres, LargestTileShift));
			}
		}
	}

}

void AArcTerrain::ValidateGridInvariants()
{
	GridInvariantReport.Reset();

	TArray<FString> Violations;
	GatherGridViolations(Violations);
	for (const FString& Violation : Violations)
	{
		UE_LOGF(LogArcTerrain, Error,"%ls: %ls", *GetName(), *Violation);
	}
	GridInvariantReport = FString::Join(Violations, TEXT(" | "));
}

void AArcTerrain::EnforceFaceGridInvariant()
{
	const int32 Resolved =
		FMath::Clamp(FaceQuadsShift, ArcTerrain::MinFaceQuadsShift, ArcTerrain::MaxFaceQuadsShift);
	if (Resolved == FaceQuadsShift)
	{
		return;
	}

	UE_LOGF(LogArcTerrain, Warning,
		"%ls: FaceQuadsShift (%d) is outside [%d, %d] — clamped to %d. Above the ceiling the float32 "
		"corner decode stops being exact at the depth the face needs; below the floor the world "
		"extent tiles into orders of magnitude more faces than the base mesh is costed for.",
		*GetName(), FaceQuadsShift, ArcTerrain::MinFaceQuadsShift, ArcTerrain::MaxFaceQuadsShift, Resolved);
	FaceQuadsShift = Resolved;
}

void AArcTerrain::EnforceFaceGridExtentInvariant()
{
	const FArcTerrainFaceGrid Grid =
		ArcTerrain::ResolveFaceGrid(Config.TexelSizeCm(), FaceQuadsShift, WorldExtentCm);
	if (!Grid.IsValid() || Grid.RequestedFacesPerEdge <= static_cast<int64>(Grid.NumFacesPerEdge))
	{
		bWarnedFaceGridClamped = false;
		return;
	}
	if (bWarnedFaceGridClamped)
	{
		return;
	}
	bWarnedFaceGridClamped = true;

	UE_LOGF(LogArcTerrain, Warning,
		"%ls: WorldExtentCm (%.0f) asks for %lld faces per edge at FaceQuadsShift %d — clamped to %d "
		"(%d faces). The base mesh therefore covers %.0f cm, not the authored extent; raise "
		"FaceQuadsShift to cover more world with the same face count.",
		*GetName(), WorldExtentCm, Grid.RequestedFacesPerEdge, FaceQuadsShift, Grid.NumFacesPerEdge,
		Grid.NumFaces(),
		static_cast<double>(Grid.NumFacesPerEdge) * static_cast<double>(Grid.FaceQuads)
			* static_cast<double>(Config.TexelSizeCm()));
}

void AArcTerrain::PostLoad()
{
	Super::PostLoad();

	ValidateGridInvariants();
	EnforceFaceGridInvariant();
	EnforceFaceGridExtentInvariant();
	RefreshDerivedGridSummary();
}

void AArcTerrain::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	ValidateGridInvariants();
	EnforceFaceGridInvariant();
	EnforceFaceGridExtentInvariant();
	RefreshDerivedGridSummary();

	if (UWorld* World = GetWorld())
	{
		if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
		{
			Subsystem->RegisterTerrain(*this);
		}
	}
}

void AArcTerrain::Destroyed()
{
	if (UWorld* World = GetWorld())
	{
		if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
		{
			Subsystem->UnregisterTerrain(*this);
		}
	}

	Super::Destroyed();
}

#if WITH_EDITOR
void AArcTerrain::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Live terrain-material swap (no map reload): drop the subsystem's cached resolution for this
	// terrain and recreate its cells' MIDs against the new base material. Property == nullptr covers
	// undo/redo and paste (PostEditUndo routes here with a null property) — refresh conservatively,
	// it's cosmetic and cheap.
	if (PropertyChangedEvent.Property == nullptr
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AArcTerrain, TerrainMaterial))
	{
		if (UWorld* World = GetWorld())
		{
			if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
			{
				Subsystem->OnTerrainMaterialChanged(*this);
			}
		}
	}

	ValidateGridInvariants();
	EnforceFaceGridInvariant();
	EnforceFaceGridExtentInvariant();
	RefreshDerivedGridSummary();

	// Config geometry (TexelSize / CellExtent / ChannelSet) changed: Config is ALWAYS editable now, so
	// this can fire at any time and must FULLY invalidate: every cached cell (tiles, collision, preview)
	// was built against the OLD layout, so flush the terrain's whole cell set via OnTerrainConfigChanged;
	// cells come back from the stamps and modifiers that shape them. On-disk tiles need no sweep — the
	// header carries the raster, so one written at the old layout reads as a miss. Property == nullptr
	// covers undo/redo/paste (PostEditUndo routes here with a null property) — a reverted Config edit
	// must invalidate too.
	const bool bConfigMaybeChanged = PropertyChangedEvent.Property == nullptr
		|| (PropertyChangedEvent.MemberProperty != nullptr
			&& PropertyChangedEvent.MemberProperty->GetFName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, Config));
	if (bConfigMaybeChanged)
	{
		if (UWorld* World = GetWorld())
		{
			if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
			{
				Subsystem->BakeLightingDerivedKnobs(*this);
				Subsystem->OnTerrainConfigChanged(*this);
			}
		}
	}

	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& (PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, CollisionPatchQuads)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, CollisionPatchExtent)))
	{
		if (UWorld* World = GetWorld())
		{
			if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
			{
				Subsystem->OnTerrainCollisionRasterChanged(*this);
			}
		}
	}

	// Lighting-knob edit: every one of these is folded into a built lighting mesh and nothing dirties a
	// lighting patch when they move. Gated on non-Interactive like the routes below — they are
	// slider-editable and a drag would rebuild every cell's proxy and BLAS per frame.
	const bool bLightingMasterKnobChanged = PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& (PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingDetailPerDistance)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingSinkPerDistance)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingRadius)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingRadiusHysteresis)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingNearBandFraction)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingFarRadius)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingFarBlockCells));
	const bool bLightingKnobChanged = bLightingMasterKnobChanged
		|| (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& (PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingNearToleranceCm)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingMidToleranceCm)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingSkirtToleranceMultiplier)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingFarBlockToleranceCm)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingFarSinkPerDistance)
			|| PropertyChangedEvent.GetPropertyName()
				== GET_MEMBER_NAME_CHECKED(AArcTerrain, LightingNearSinkPerDistance)));
	if (bLightingKnobChanged)
	{
		if (UWorld* World = GetWorld())
		{
			if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
			{
				if (bLightingMasterKnobChanged)
				{
					Subsystem->BakeLightingDerivedKnobs(*this);
				}
				Subsystem->OnTerrainLightingChanged(*this);
			}
		}
	}

	// Height-options edit: BaseHeightCm is the level the composite starts from, so a change re-composites
	// and re-keys every section — but the spatial frame is untouched, so this routes to the lighter
	// OnTerrainHeightOptionsChanged, NOT OnTerrainConfigChanged (full spatial teardown). GATED on
	// non-Interactive: the field is slider-editable and an Interactive drag fires PostEditChangeProperty
	// every frame — re-composite only on the final ValueSet.
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& PropertyChangedEvent.MemberProperty != nullptr
		&& PropertyChangedEvent.MemberProperty->GetFName()
			== GET_MEMBER_NAME_CHECKED(AArcTerrain, HeightOptions))
	{
		if (UWorld* World = GetWorld())
		{
			if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
			{
				Subsystem->OnTerrainHeightOptionsChanged(*this);
			}
		}
	}

	// Density-ladder edit: an LOD moves S(T) for every coarse tier at once, and nothing else on this path
	// notices — no bound is dirtied, so the bake memo keeps holding coarse tiles current under a key it
	// never recomputes. Property == nullptr covers undo/redo/paste. GATED on non-Interactive like the
	// HeightOptions route: the extent is slider-editable, and a drag would fire this every frame, each one
	// discarding every in-flight coarse bake decision and re-clearing every coarse slice.
	const bool bHeightTiersMaybeChanged = PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& (PropertyChangedEvent.Property == nullptr
			|| (PropertyChangedEvent.MemberProperty != nullptr
				&& PropertyChangedEvent.MemberProperty->GetFName()
					== GET_MEMBER_NAME_CHECKED(AArcTerrain, HeightTiers)));
	if (bHeightTiersMaybeChanged)
	{
		if (UWorld* World = GetWorld())
		{
			if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
			{
				Subsystem->OnTerrainHeightTiersChanged(*this);
			}
		}
	}

	const bool bWeightTiersMaybeChanged = PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& (PropertyChangedEvent.Property == nullptr
			|| (PropertyChangedEvent.MemberProperty != nullptr
				&& PropertyChangedEvent.MemberProperty->GetFName()
					== GET_MEMBER_NAME_CHECKED(AArcTerrain, WeightTiers)));
	if (bWeightTiersMaybeChanged)
	{
		if (UWorld* World = GetWorld())
		{
			if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
			{
				Subsystem->OnTerrainWeightTiersChanged(*this);
			}
		}
	}

	const FName ChangedPropertyName = PropertyChangedEvent.GetPropertyName();
	const FName ChangedMemberName = PropertyChangedEvent.MemberProperty != nullptr
		? PropertyChangedEvent.MemberProperty->GetFName()
		: NAME_None;
	const bool bStreamLayoutMaybeChanged = PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& (PropertyChangedEvent.Property == nullptr
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, PreviewRadius)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, PreviewRadiusHysteresis)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, StreamRadius)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, StreamHysteresis)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, WeightStreamRadius)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, WeightResidencyRadius)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, MaxResidentSectors)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, MaxResidentWeightCells)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, SectorShift)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, WeightSliceShift)
			|| ChangedMemberName == GET_MEMBER_NAME_CHECKED(AArcTerrain, HeightTiers)
			|| ChangedMemberName == GET_MEMBER_NAME_CHECKED(AArcTerrain, WeightTiers));
	if (bStreamLayoutMaybeChanged)
	{
		if (UWorld* World = GetWorld())
		{
			if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
			{
				Subsystem->OnTerrainStreamLayoutChanged(*this);
			}
		}
	}

	const bool bRenderGridChanged = PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& (PropertyChangedEvent.Property == nullptr
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, WorldExtentCm)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, FaceQuadsShift)
			|| ChangedMemberName == GET_MEMBER_NAME_CHECKED(AArcTerrain, WorldExtentCm)
			|| ChangedMemberName == GET_MEMBER_NAME_CHECKED(AArcTerrain, FaceQuadsShift));
	const bool bRenderDiscreteChanged = PropertyChangedEvent.Property == nullptr
		|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, CBTPoolDepth)
		|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, bEnableDistanceCulling);
	const bool bRenderSliderChanged = PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive
		&& (PropertyChangedEvent.Property == nullptr
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, DistanceCullRadius)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, ConservativeMinHeightCm)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, ConservativeMaxHeightCm)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, TargetTrianglePixels)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, MaxTriangleEdgePixels)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, MergeThresholdFraction)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, TargetErrorPixels)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, FlatAreaRelief)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, FlatReliefSagCmPerMeterSq)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, FrustumCullMargin)
			|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AArcTerrain, TessFrustumCullMargin));
	if (bRenderGridChanged || bRenderDiscreteChanged || bRenderSliderChanged)
	{
		if (UWorld* World = GetWorld())
		{
			if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
			{
				Subsystem->OnTerrainRenderChanged(*this);
			}
		}
	}
}

void AArcTerrain::CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
	TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile)
{
	const ULevel* Level = GetLevel();
	if (Level == nullptr || Level->IsWorldPartitionRuntimeCell())
	{
		UE_LOGF(LogArcTerrain, Error,
			"%ls: cooking into package '%ls' from a runtime cell level - World Partition has re-outered "
			"this actor into a generated streaming cell, so the tiles would be staged under that cell's name "
			"while the runtime reads the map's. Keep the terrain actor always-loaded (bIsSpatiallyLoaded = "
			"false) and off every Data Layer. No tiles staged.",
			*GetName(), PackageFilename);
		return;
	}

	const FString SourceDir = ArcTerrainTileNames::MakeTileDirForWrite(*this);
	if (SourceDir.IsEmpty())
	{
		UE_LOGF(LogArcTerrain, Error,
			"%ls: no baked tile directory resolved while cooking '%ls'; the packaged build will have no "
			"terrain data.",
			*GetName(), PackageFilename);
		return;
	}

	const FString DestDir = FPaths::GetPath(PackageFilename)
		/ FPaths::GetCleanFilename(FPaths::GetPath(SourceDir))
		/ FPaths::GetCleanFilename(SourceDir);

	constexpr EArcTerrainRail TileRails[ArcTerrain::NumTileRails] = {
		EArcTerrainRail::Height, EArcTerrainRail::Weight, EArcTerrainRail::Data };

	IFileManager& FileManager = IFileManager::Get();
	TArray<FString> FileNames;
	FileManager.FindFiles(FileNames, *(SourceDir / ArcTerrainTileManifest::ManifestFileName), /*Files=*/true,
		/*Directories=*/false);
	for (EArcTerrainRail Rail : TileRails)
	{
		TArray<FString> RailFileNames;
		FileManager.FindFiles(RailFileNames, *(SourceDir / ArcTerrainTileNames::MakeTileFileWildcard(Rail)),
			/*Files=*/true, /*Directories=*/false);
		FileNames.Append(MoveTemp(RailFileNames));
	}

	int32 NumWritten = 0;
	int32 NumFailed = 0;
	int64 TotalBytes = 0;
	for (const FString& FileName : FileNames)
	{
		const FString SourcePath = SourceDir / FileName;
		TArray<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *SourcePath))
		{
			UE_LOGF(LogArcTerrain, Error,"%ls: failed to read baked tile file '%ls' while cooking.",
				*GetName(), *SourcePath);
			++NumFailed;
			continue;
		}

		const FString DestPath = DestDir / FileName;
		WriteAdditionalFile(*DestPath, FileData.GetData(), static_cast<int64>(FileData.Num()));
		++NumWritten;
		TotalBytes += FileData.Num();
	}

	if (NumWritten == 0)
	{
		UE_LOGF(LogArcTerrain, Error,"%ls: no baked tile files staged from '%ls'; the packaged build will render blank terrain.",
			*GetName(), *SourceDir);
		return;
	}

	if (NumFailed > 0)
	{
		UE_LOGF(LogArcTerrain, Error,
			"%ls: %d of %d baked tile file(s) from '%ls' could not be read; the packaged build would ship an "
			"incomplete tile set and render holes. Staging is not complete.",
			*GetName(), NumFailed, NumFailed + NumWritten, *SourceDir);
		return;
	}

	UE_LOGF(LogArcTerrain, Display,"%ls: staged %d baked tile file(s), %lld bytes, from '%ls' into '%ls'.",
		*GetName(), NumWritten, TotalBytes, *SourceDir, *DestDir);
}
#endif
