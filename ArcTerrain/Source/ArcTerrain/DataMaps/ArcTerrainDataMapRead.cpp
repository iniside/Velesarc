// Copyright Lukasz Baran. All Rights Reserved.

#include "DataMaps/ArcTerrainDataMapRead.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR

#include "ArcTerrainActor.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileBake.h"
#include "DataMaps/ArcTerrainDataMapBake.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Stamps/ArcTerrainTexturePatchModifier.h"

namespace ArcTerrainDataMapReadLocal_4d2f
{
	bool HasSource(const UArcTerrainTexturePatchModifier& Modifier, EArcTerrainDataMap Map)
	{
		for (const TPair<EArcTerrainDataMap, const FArcTerrainDataMapSource*>& Entry :
			Modifier.GetDataMapSources())
		{
			if (Entry.Key == Map && Entry.Value != nullptr && Entry.Value->Texture != nullptr)
			{
				return true;
			}
		}
		return false;
	}

	bool GatherSourceBounds(const FArcTerrainState& State, EArcTerrainDataMap Map, FBox& OutBounds)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN GatherSourceBounds t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END GatherSourceBounds elapsed=%.3fs", FPlatformTime::Seconds() - RiverLogStart);
		};
		OutBounds = FBox(ForceInit);
		for (const FArcTerrainStampEntry& Entry : State.Stamps)
		{
			const UArcTerrainStampComponent* Stamp = Entry.Stamp.Get();
			if (Stamp == nullptr || Stamp->bIsDisabled)
			{
				continue;
			}

			const UArcTerrainTexturePatchModifier* Modifier = Cast<UArcTerrainTexturePatchModifier>(Stamp);
			if (Modifier == nullptr || !HasSource(*Modifier, Map))
			{
				continue;
			}

			const FBox Bounds = Stamp->ComputeBounds();
			if (!Bounds.IsValid)
			{
				continue;
			}
			OutBounds += Bounds;
		}
		return OutBounds.IsValid != 0;
	}

	int32 TileKeyFromTexel(int32 Texel, int32 TileQuads)
	{
		return FMath::FloorToInt32(static_cast<double>(Texel) / static_cast<double>(TileQuads));
	}

	const FArcTerrainDataMapCellMask* FindMask(const FArcTerrainDataMapCellBake& Bake,
		EArcTerrainDataMap Map)
	{
		return Bake.Masks.FindByPredicate([Map](const FArcTerrainDataMapCellMask& Candidate)
		{
			return Candidate.Map == Map;
		});
	}

	void CopyTileSamples(const FArcTerrainDataMapCellMask& Mask, const FArcTerrainTileGrid& Grid,
		FIntPoint TileKey, FIntPoint MinTexel, FIntPoint MaxTexel, FIntPoint OutDims,
		TArray<float>& OutValues)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN CopyTileSamples t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END CopyTileSamples elapsed=%.3fs", FPlatformTime::Seconds() - RiverLogStart);
		};
		const int64 TileMinX = static_cast<int64>(TileKey.X) * Grid.TileQuads;
		const int64 TileMinY = static_cast<int64>(TileKey.Y) * Grid.TileQuads;
		const int64 OwnedMinX = FMath::Max(static_cast<int64>(MinTexel.X), TileMinX);
		const int64 OwnedMinY = FMath::Max(static_cast<int64>(MinTexel.Y), TileMinY);
		const int64 OwnedMaxX = FMath::Min(static_cast<int64>(MaxTexel.X), TileMinX + Grid.TileQuads - 1);
		const int64 OwnedMaxY = FMath::Min(static_cast<int64>(MaxTexel.Y), TileMinY + Grid.TileQuads - 1);
		const int32 Resolution = Grid.Resolution();
		const float QuantRange = Mask.QuantMax - Mask.QuantMin;
		constexpr float QuantStep = 1.0f / 255.0f;

		for (int64 GlobalY = OwnedMinY; GlobalY <= OwnedMaxY; ++GlobalY)
		{
			const int32 SourceRow = static_cast<int32>(GlobalY - TileMinY + 1);
			const int32 OutRow = static_cast<int32>(GlobalY - MinTexel.Y);
			for (int64 GlobalX = OwnedMinX; GlobalX <= OwnedMaxX; ++GlobalX)
			{
				const int32 SourceColumn = static_cast<int32>(GlobalX - TileMinX + 1);
				const int32 OutColumn = static_cast<int32>(GlobalX - MinTexel.X);
				const uint8 Quantized = Mask.Texels[SourceRow * Resolution + SourceColumn];
				OutValues[OutRow * OutDims.X + OutColumn] =
					Mask.QuantMin + static_cast<float>(Quantized) * QuantStep * QuantRange;
			}
		}
	}
}

bool ArcTerrainDataMaps::FindDataMapSourceRegion(const AArcTerrain& Terrain,
	const FArcTerrainState& State, EArcTerrainDataMap Map, FBox& OutTerrainLocalBounds,
	FArcTerrainTileGrid& OutDataGrid)
{
	const double RiverLogStart = FPlatformTime::Seconds();
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN FindDataMapSourceRegion t=%.6f", RiverLogStart);
	ON_SCOPE_EXIT
	{
		UE_LOGF(LogTemp, Display,"[RiverGen] END FindDataMapSourceRegion elapsed=%.3fs", FPlatformTime::Seconds() - RiverLogStart);
	};
	OutTerrainLocalBounds = FBox(ForceInit);
	OutDataGrid = FArcTerrainTileGrid();

	FArcTerrainTileGrid Grid;
	if (!ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Data, 0, Grid))
	{
		return false;
	}

	FBox SourceBounds(ForceInit);
	if (!ArcTerrainDataMapReadLocal_4d2f::GatherSourceBounds(State, Map, SourceBounds))
	{
		return false;
	}

	const double TexelToLocal = static_cast<double>(Grid.TileExtentM) * 100.0
		/ static_cast<double>(Grid.TileQuads);
	if (!FMath::IsFinite(TexelToLocal) || TexelToLocal <= 0.0
		|| SourceBounds.Min.ContainsNaN() || SourceBounds.Max.ContainsNaN())
	{
		return false;
	}

	const double MinTexelX = FMath::FloorToDouble(SourceBounds.Min.X / TexelToLocal) - 1.0;
	const double MinTexelY = FMath::FloorToDouble(SourceBounds.Min.Y / TexelToLocal) - 1.0;
	const double MaxTexelX = FMath::CeilToDouble(SourceBounds.Max.X / TexelToLocal) + 1.0;
	const double MaxTexelY = FMath::CeilToDouble(SourceBounds.Max.Y / TexelToLocal) + 1.0;
	if (MinTexelX < static_cast<double>(MIN_int32) || MinTexelY < static_cast<double>(MIN_int32)
		|| MaxTexelX > static_cast<double>(MAX_int32) || MaxTexelY > static_cast<double>(MAX_int32))
	{
		return false;
	}

	OutTerrainLocalBounds = FBox(
		FVector(MinTexelX * TexelToLocal, MinTexelY * TexelToLocal, SourceBounds.Min.Z),
		FVector(MaxTexelX * TexelToLocal, MaxTexelY * TexelToLocal, SourceBounds.Max.Z));
	OutDataGrid = Grid;
	return true;
}

bool ArcTerrainDataMaps::ReadDataMapTexelsFromSources(const AArcTerrain& Terrain,
	FArcTerrainState& State, FArcTerrainSourceCache& SourceCache, EArcTerrainDataMap Map,
	FIntPoint MinTexel, FIntPoint MaxTexel, TArray<float>& OutValues,
	FIntPoint& OutDims, bool& bOutComplete)
{
	const double RiverLogStart = FPlatformTime::Seconds();
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN ReadDataMapTexelsFromSources t=%.6f", RiverLogStart);
	ON_SCOPE_EXIT
	{
		UE_LOGF(LogTemp, Display,"[RiverGen] END ReadDataMapTexelsFromSources elapsed=%.3fs", FPlatformTime::Seconds() - RiverLogStart);
	};
	using namespace ArcTerrainDataMapReadLocal_4d2f;

	OutValues.Reset();
	OutDims = FIntPoint::ZeroValue;
	bOutComplete = false;

	FArcTerrainTileGrid Grid;
	if (!ArcTerrainTileAccess::ResolveGrid(Terrain, EArcTerrainRail::Data, 0, Grid)
		|| MaxTexel.X < MinTexel.X || MaxTexel.Y < MinTexel.Y)
	{
		return false;
	}

	const int64 SpanX = static_cast<int64>(MaxTexel.X) - MinTexel.X + 1;
	const int64 SpanY = static_cast<int64>(MaxTexel.Y) - MinTexel.Y + 1;
	if (SpanX <= 0 || SpanY <= 0 || SpanX > MAX_int32 || SpanY > MAX_int32
		|| SpanX * SpanY > MAX_int32)
	{
		return false;
	}

	const double TexelToLocal = static_cast<double>(Grid.TileExtentM) * 100.0
		/ static_cast<double>(Grid.TileQuads);
	if (!FMath::IsFinite(TexelToLocal) || TexelToLocal <= 0.0)
	{
		return false;
	}

	FBox SourceBounds(ForceInit);
	if (!GatherSourceBounds(State, Map, SourceBounds))
	{
		return false;
	}

	OutDims = FIntPoint(static_cast<int32>(SpanX), static_cast<int32>(SpanY));
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN data samples allocation t=%.6f", FPlatformTime::Seconds());
	OutValues.Init(0.0f, static_cast<int32>(SpanX * SpanY));
	UE_LOGF(LogTemp, Display,"[RiverGen] END data samples allocation t=%.6f", FPlatformTime::Seconds());

	TArray<FArcTerrainDataMapWindow> MaskWindows;
	ArcTerrainDataMaps::GatherMaskWindows(State, MaskWindows);
	FArcTerrainDataMapBakeScope Scope(SourceCache);
	const int32 MinTileX = TileKeyFromTexel(MinTexel.X, Grid.TileQuads);
	const int32 MinTileY = TileKeyFromTexel(MinTexel.Y, Grid.TileQuads);
	const int32 MaxTileX = TileKeyFromTexel(MaxTexel.X, Grid.TileQuads);
	const int32 MaxTileY = TileKeyFromTexel(MaxTexel.Y, Grid.TileQuads);
	bool bComplete = true;

	for (int64 TileY = MinTileY; TileY <= MaxTileY; ++TileY)
	{
		for (int64 TileX = MinTileX; TileX <= MaxTileX; ++TileX)
		{
			const FIntPoint TileKey(static_cast<int32>(TileX), static_cast<int32>(TileY));
			const double RiverTileStart = FPlatformTime::Seconds();
			UE_LOGF(LogTemp, Display,"[RiverGen] TILE BEGIN key=%ls range=(%d,%d)-(%d,%d) quads=%d", *TileKey.ToString(), MinTileX, MinTileY, MaxTileX, MaxTileY, Grid.TileQuads);
			ON_SCOPE_EXIT
			{
				UE_LOGF(LogTemp, Display,"[RiverGen] TILE END key=%ls elapsed=%.3fs completeSoFar=%d", *TileKey.ToString(), FPlatformTime::Seconds() - RiverTileStart, bComplete);
			};
			TArray<UArcTerrainStampComponent*> SortedStamps;
			UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN data tile GatherSortedStamps t=%.6f", FPlatformTime::Seconds());
			ArcTerrainSubsystemPrivate::GatherSortedStamps(
				State, ArcTerrainTileBake::TileGatherBounds(Grid, TileKey), SortedStamps);
			UE_LOGF(LogTemp, Display,"[RiverGen] END data tile GatherSortedStamps t=%.6f", FPlatformTime::Seconds());

			FArcTerrainDataMapTileSnapshot Snapshot;
			UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN data tile GatherDataMapTileSnapshot t=%.6f", FPlatformTime::Seconds());
			ArcTerrainDataMaps::GatherDataMapTileSnapshot(Terrain.GetTerrainTransform(), TileKey, SortedStamps,
				MaskWindows, State.DataWindowRevision, 0.0, Grid.TileQuads, TexelToLocal, Scope, Snapshot);
			UE_LOGF(LogTemp, Display,"[RiverGen] END data tile GatherDataMapTileSnapshot t=%.6f", FPlatformTime::Seconds());

			TArray<FArcTerrainDataMapTileSnapshot::FMaskWork> RequestedWork;
			UE_LOGF(LogTemp, Display,"[RiverGen] TILE inputs stamps=%d mapWork=%d maskWindows=%d", SortedStamps.Num(), Snapshot.Work.Num(), MaskWindows.Num());
			for (FArcTerrainDataMapTileSnapshot::FMaskWork& Work : Snapshot.Work)
			{
				if (Work.Map == Map)
				{
					RequestedWork.Add(MoveTemp(Work));
				}
			}
			Snapshot.Work = MoveTemp(RequestedWork);
			if (Snapshot.Work.Num() == 0)
			{
				continue;
			}

			bComplete &= !Snapshot.Work[0].bUnresolved;
			UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN data tile BakeDataMapTileSnapshot t=%.6f", FPlatformTime::Seconds());
			const FArcTerrainDataMapTileBakeResult Result =
				ArcTerrainDataMaps::BakeDataMapTileSnapshot(Snapshot);
			UE_LOGF(LogTemp, Display,"[RiverGen] END data tile BakeDataMapTileSnapshot t=%.6f", FPlatformTime::Seconds());
			ArcTerrainDataMaps::ReportDataMapTileBakeWarnings(Result, MaskWindows, Scope);
			const FArcTerrainDataMapCellMask* Mask = FindMask(Result.Bake, Map);
			if (Mask == nullptr)
			{
				bComplete = false;
				continue;
			}
			bComplete &= !Mask->bUnresolved;
			if (Mask->Quads == 0)
			{
				continue;
			}
			if (Mask->Quads != Grid.TileQuads
				|| Mask->Texels.Num() != Grid.Resolution() * Grid.Resolution()
				|| !FMath::IsFinite(Mask->QuantMin) || !FMath::IsFinite(Mask->QuantMax))
			{
				bComplete = false;
				continue;
			}

			CopyTileSamples(*Mask, Grid, TileKey, MinTexel, MaxTexel, OutDims, OutValues);
		}
	}

	bOutComplete = bComplete;
	return true;
}

bool UArcTerrainSubsystem::GetDataMapSourceRegion(const AArcTerrain& Terrain,
	EArcTerrainDataMap Map, FBox& OutTerrainLocalBounds, FArcTerrainTileGrid& OutDataGrid)
{
	check(IsInGameThread());
	OutTerrainLocalBounds = FBox(ForceInit);
	OutDataGrid = FArcTerrainTileGrid();

	const FArcTerrainState* State = Terrains.Find(&Terrain);
	return State != nullptr && ArcTerrainDataMaps::FindDataMapSourceRegion(
		Terrain, *State, Map, OutTerrainLocalBounds, OutDataGrid);
}

bool UArcTerrainSubsystem::SampleDataMapTexelsFromSources(const AArcTerrain& Terrain,
	EArcTerrainDataMap Map, FIntPoint MinTexel, FIntPoint MaxTexel,
	TArray<float>& OutValues, FIntPoint& OutDims, bool& bOutComplete)
{
	check(IsInGameThread());
	OutValues.Reset();
	OutDims = FIntPoint::ZeroValue;
	bOutComplete = false;

	FArcTerrainState* State = Terrains.Find(&Terrain);
	return State != nullptr && ArcTerrainDataMaps::ReadDataMapTexelsFromSources(
		Terrain, *State, SourceCache, Map, MinTexel, MaxTexel, OutValues, OutDims, bOutComplete);
}

#endif
