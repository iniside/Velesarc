// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainTileBake.h"

#if WITH_EDITOR

#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainDiagnostics.h"
#include "ArcTerrainTileAccess.h"
#include "ArcTerrainTileCache.h"
#include "Render/ArcTerrainDensityTiers.h"
#include "ArcTerrainSubsystem.h"
#include "Collision/ArcTerrainCollisionBuilder.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "DataMaps/ArcTerrainDataMapKey.h"
#include "Hash/Blake3.h"
#include "Weights/ArcTerrainWeightBuilder.h"

namespace ArcTerrainTileBake_3d95
{
	double BeginLeafTiming()
	{
		return ArcTerrainDiag::IsTiming() ? FPlatformTime::Seconds() : 0.0;
	}

	void EndLeafTiming(ArcTerrainDiag::ELeafTiming Leaf, double StartSeconds)
	{
		if (StartSeconds != 0.0)
		{
			ArcTerrainDiag::Frame().Tick.LeafSeconds[static_cast<int32>(Leaf)] +=
				FPlatformTime::Seconds() - StartSeconds;
		}
	}

	template <typename PodType>
	void HashPod(FBlake3& Hash, const PodType& Value)
	{
		Hash.Update(&Value, sizeof(PodType));
	}
}

namespace ArcTerrainTileBake
{
	bool ResolveTileGrid(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		int32 ResolvedCellWeightQuads, FArcTerrainTileGrid& OutGrid)
	{
		if (!ArcTerrainTileAccess::IsValidTier(Tier))
		{
			return false;
		}

		const FArcTerrainConfig& Config = Terrain.Config;

		FArcTerrainTileGrid Grid;
		switch (Rail)
		{
		case EArcTerrainRail::Weight:
		{
			const int32 CumulativeShift = ArcTerrainTileAccess::ResolveWeightTierShift(Terrain, Tier);
			if (CumulativeShift == INDEX_NONE)
			{
				return false;
			}
			Grid = ArcTerrainTileFormat::WeightTileGrid(Config,
				ResolvedCellWeightQuads >> CumulativeShift, CumulativeShift);
			break;
		}
		case EArcTerrainRail::Data:
			if (Tier != 0)
			{
				return false;
			}
			Grid = ArcTerrainTileFormat::DataTileGrid(Config);
			break;
		default:
		{
			const int32 CumulativeShift = ArcTerrainTileAccess::ResolveHeightTierShift(Terrain, Tier);
			if (CumulativeShift == INDEX_NONE)
			{
				return false;
			}
			Grid = ArcTerrainTileFormat::HeightTileGrid(Config, CumulativeShift);
			break;
		}
		}

		if (!Grid.IsValid())
		{
			return false;
		}
		OutGrid = Grid;
		return true;
	}

	bool ResolveBakeTileGrid(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		int32 ResolvedCellWeightQuads, FArcTerrainTileGrid& OutGrid)
	{
		if (!ArcTerrainTileAccess::IsValidTier(Tier))
		{
			return false;
		}

		const FArcTerrainConfig& Config = Terrain.Config;

		FArcTerrainTileGrid Grid;
		switch (Rail)
		{
		case EArcTerrainRail::Weight:
		{
			TArray<int32> CumulativeShifts;
			ArcTerrain::ResolveAuthoredWeightTierShifts(Terrain, ResolvedCellWeightQuads, CumulativeShifts);
			if (!CumulativeShifts.IsValidIndex(Tier))
			{
				return false;
			}
			const int32 CumulativeShift = CumulativeShifts[Tier];
			Grid = ArcTerrainTileFormat::WeightTileGrid(Config,
				ResolvedCellWeightQuads >> CumulativeShift, CumulativeShift);
			break;
		}
		case EArcTerrainRail::Data:
			if (Tier != 0)
			{
				return false;
			}
			Grid = ArcTerrainTileFormat::DataTileGrid(Config);
			break;
		default:
		{
			TArray<int32> CumulativeShifts;
			ArcTerrain::ResolveAuthoredHeightTierShifts(Terrain, CumulativeShifts);
			if (!CumulativeShifts.IsValidIndex(Tier))
			{
				return false;
			}
			const int32 CumulativeShift = CumulativeShifts[Tier];
			Grid = ArcTerrainTileFormat::HeightTileGrid(Config, CumulativeShift);
			break;
		}
		}

		if (!Grid.IsValid())
		{
			return false;
		}
		OutGrid = Grid;
		return true;
	}

	void GatherTileKeysIntersectingBounds(const FArcTerrainTileGrid& Grid, const FBox& LocalBounds,
		TArray<FIntPoint>& OutKeys)
	{
		OutKeys.Reset();
		if (!Grid.IsValid())
		{
			return;
		}
		if (!LocalBounds.IsValid
			|| !FMath::IsFinite(LocalBounds.Min.X) || !FMath::IsFinite(LocalBounds.Min.Y)
			|| !FMath::IsFinite(LocalBounds.Min.Z) || !FMath::IsFinite(LocalBounds.Max.X)
			|| !FMath::IsFinite(LocalBounds.Max.Y) || !FMath::IsFinite(LocalBounds.Max.Z))
		{
			return;
		}

		FIntPoint MinKey = FIntPoint::ZeroValue;
		FIntPoint MaxKey = FIntPoint::ZeroValue;
		if (!ArcTerrainTileFormat::TileKeyFromLocal(Grid,
				FVector2D(LocalBounds.Min.X, LocalBounds.Min.Y), MinKey)
			|| !ArcTerrainTileFormat::TileKeyFromLocal(Grid,
				FVector2D(LocalBounds.Max.X, LocalBounds.Max.Y), MaxKey))
		{
			return;
		}

		// Expand one key so TileGatherBounds' raster apron, not the tile rectangle, can still hit.
		const int32 CandidateMinX = (MinKey.X > MIN_int32) ? (MinKey.X - 1) : MIN_int32;
		const int32 CandidateMinY = (MinKey.Y > MIN_int32) ? (MinKey.Y - 1) : MIN_int32;
		const int32 CandidateMaxX = (MaxKey.X < MAX_int32) ? (MaxKey.X + 1) : MAX_int32;
		const int32 CandidateMaxY = (MaxKey.Y < MAX_int32) ? (MaxKey.Y + 1) : MAX_int32;
		if (CandidateMinX > CandidateMaxX || CandidateMinY > CandidateMaxY)
		{
			return;
		}

		const int64 CountX = static_cast<int64>(CandidateMaxX) - static_cast<int64>(CandidateMinX) + 1;
		const int64 CountY = static_cast<int64>(CandidateMaxY) - static_cast<int64>(CandidateMinY) + 1;
		const int64 Total = CountX * CountY;
		if (Total > 0 && Total <= MAX_int32)
		{
			OutKeys.Reserve(static_cast<int32>(Total));
		}

		for (int32 TileY = CandidateMinY; TileY <= CandidateMaxY; ++TileY)
		{
			for (int32 TileX = CandidateMinX; TileX <= CandidateMaxX; ++TileX)
			{
				const FIntPoint Key(TileX, TileY);
				const FBox GatherBounds = TileGatherBounds(Grid, Key);
				if (!GatherBounds.IsValid || !GatherBounds.Intersect(LocalBounds))
				{
					continue;
				}
				OutKeys.Add(Key);
			}
		}
	}

	bool GatherTileBakeInputs(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		const AArcTerrain& Terrain, FArcTerrainState& State,
		FArcTerrainTileBakeInputs& OutInputs)
	{
		const int32 WeightQuads = ArcTerrainSubsystemPrivate::ResolveWeightQuads(Terrain.Config, State);
		FArcTerrainTileGrid Grid;
		if (!ResolveBakeTileGrid(Terrain, Rail, Tier, WeightQuads, Grid))
		{
			return false;
		}

		OutInputs.Grid = Grid;
		OutInputs.GatherBounds = TileGatherBounds(Grid, TileKey);
		const double GatherStartSeconds = ArcTerrainTileBake_3d95::BeginLeafTiming();

		struct FGatheredStamp
		{
			UArcTerrainStampComponent* Stamp = nullptr;
			double Priority = 0.0;
			FString PathName;
		};

		TArray<FGatheredStamp> Gathered;
		const FArcTerrainBakeTileRef TileRef{Rail, Tier, TileKey};
		if (const TArray<TWeakObjectPtr<UArcTerrainStampComponent>>* ContributorList =
			State.Contributors.Find(TileRef))
		{
			Gathered.Reserve(ContributorList->Num());
			for (const TWeakObjectPtr<UArcTerrainStampComponent>& Weak : *ContributorList)
			{
				UArcTerrainStampComponent* Stamp = Weak.Get();
				if (Stamp == nullptr || Stamp->bIsDisabled)
				{
					continue;
				}

				const FArcTerrainStampEntry* Entry = State.Stamps.FindByPredicate(
					[Stamp](const FArcTerrainStampEntry& Candidate)
					{
						return Candidate.Stamp.Get() == Stamp;
					});
				const FBox StampBounds = (Entry != nullptr) ? Entry->Bounds : Stamp->ComputeBounds();
				if (!StampBounds.IsValid || !StampBounds.Intersect(OutInputs.GatherBounds))
				{
					continue;
				}

				FGatheredStamp& GatheredStamp = Gathered.AddDefaulted_GetRef();
				GatheredStamp.Stamp = Stamp;
				GatheredStamp.Priority = Stamp->Priority;
				GatheredStamp.PathName = Stamp->GetPathName();
			}
		}

		Gathered.Sort([](const FGatheredStamp& A, const FGatheredStamp& B)
		{
			if (A.Priority != B.Priority)
			{
				return A.Priority < B.Priority;
			}
			return A.PathName < B.PathName;
		});

		OutInputs.SortedStamps.Reset(Gathered.Num());
		for (const FGatheredStamp& GatheredStamp : Gathered)
		{
			OutInputs.SortedStamps.Add(GatheredStamp.Stamp);
		}

		ArcTerrainTileBake_3d95::EndLeafTiming(
			ArcTerrainDiag::ELeafTiming::TileKeyGather, GatherStartSeconds);
		return true;
	}

	FBox TileGatherBounds(const FArcTerrainTileGrid& Grid, FIntPoint TileKey)
	{
		if (!Grid.IsValid())
		{
			return FBox(ForceInit);
		}

		// Spans the RASTER, not the tile rectangle: the apron texels carry the neighbour's global indices,
		// so a stamp reaching only into the apron contributes to a texel this tile writes. Gathering over
		// the exact rectangle would give that shared global texel a different contributor set in each of
		// the two tiles — a seam.
		const double TexelToLocal =
			static_cast<double>(Grid.TileExtentM) * 100.0 / static_cast<double>(Grid.TileQuads);
		const FIntPoint TexelOrigin =
			ArcTerrainSubsystemPrivate::ComputeGlobalTexelOrigin(TileKey, Grid.TileQuads);
		const double SpanCm = static_cast<double>(Grid.Resolution() - 1) * TexelToLocal;

		const FVector Min(static_cast<double>(TexelOrigin.X) * TexelToLocal,
			static_cast<double>(TexelOrigin.Y) * TexelToLocal, -ArcTerrain::GCellZHalfExtent);
		const FVector Max(Min.X + SpanCm, Min.Y + SpanCm, ArcTerrain::GCellZHalfExtent);
		return FBox(Min, Max);
	}

	ArcTerrainCpuComposite::FCellRaster MakeTileRaster(const FArcTerrainTileGrid& Grid, FIntPoint TileKey)
	{
		ArcTerrainCpuComposite::FCellRaster Raster;
		if (!Grid.IsValid())
		{
			return Raster;
		}

		Raster.TexelOrigin =
			ArcTerrainSubsystemPrivate::ComputeGlobalTexelOrigin(TileKey, Grid.TileQuads);
		Raster.SamplesPerSide = Grid.Resolution();
		Raster.TexelToLocal = static_cast<float>(
			static_cast<double>(Grid.TileExtentM) * 100.0 / static_cast<double>(Grid.TileQuads));
		return Raster;
	}

	FIntPoint TileKeyFromCell(const FArcTerrainTileGrid& Grid, FArcTerrainCellKey CellKey)
	{
		return ArcTerrainTileAccess::TileKeyFromCell(Grid, CellKey);
	}

	FIoHash ComputeTileCacheKey(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		const AArcTerrain& Terrain, const FArcTerrainState& State,
		const FArcTerrainTileBakeInputs& Inputs,
		const ArcTerrainDataMaps::FArcTerrainDataMapKeyInputs* DataInputs)
	{
		using namespace ArcTerrainTileBake_3d95;

		const FArcTerrainConfig& Config = Terrain.Config;
		const FArcTerrainTileGrid& Grid = Inputs.Grid;
		if (!Grid.IsValid())
		{
			return FIoHash();
		}

		const FTransform TerrainTransform = Terrain.GetTerrainTransform();

		// The shared composite recipe: it carries BaseHeightCm and the stamp-to-terrain placements, which
		// is why this function takes the actor and not the config.
		const double RecipeStartSeconds = BeginLeafTiming();
		const FIoHash RecipeKey = ArcTerrainCollision::ComputeCellCollisionKey(
			Config, Terrain.HeightOptions.BaseHeightCm, TileKey, TerrainTransform, Inputs.SortedStamps);
		EndLeafTiming(ArcTerrainDiag::ELeafTiming::TileKeyRecipe, RecipeStartSeconds);

		FBlake3 Hash;

		// OWN format version, disjoint from every per-cell payload key in the same bucket.
		static const FGuid FormatVersionKey(TEXT("5B8F2D14-7C0A-4B36-8E91-3D6420A7F5C2"));
		HashPod(Hash, FormatVersionKey);

		// TileVersion FOLDS IN, so bumping it is all a format change ever has to do. Bumping it alone makes
		// the reader reject the bytes while the manifest still names the tile and this key still matches —
		// the tile is then neither served nor rebaked, permanently, and the atlas has no data there.
		HashPod(Hash, ArcTerrainTileFormat::TileVersion);

		const uint8 RailValue = static_cast<uint8>(Rail);
		HashPod(Hash, RailValue);
		HashPod(Hash, TileKey.X);
		HashPod(Hash, TileKey.Y);
		// Tier itself is never hashed: it addresses the tile only through the lattice, and TileExtentM and
		// TexelSizeCm below both shift with it.
		HashPod(Hash, Grid.TileQuads);
		HashPod(Hash, Grid.TileExtentM);
		HashPod(Hash, Grid.TexelSizeCm);
		HashPod(Hash, RecipeKey);

		if (Rail == EArcTerrainRail::Weight)
		{
			const double WeightStartSeconds = BeginLeafTiming();
			// The recipe key EXCLUDES weight-only layers, so a weight-only edit would leave it unmoved.
			TArray<ArcTerrainWeights::FArcTerrainWeightLayerIdentity> PresentLayers;
			ArcTerrainSubsystemPrivate::GatherPresentWeightLayerIdentities(
				Config, Inputs.SortedStamps, PresentLayers);
			TOptional<uint32> BaseChannelNameHash;
			if (Config.ChannelSet != nullptr && Config.ChannelSet->Channels.IsValidIndex(ArcTerrain::GBaseChannelIndex))
			{
				BaseChannelNameHash = ArcTerrain::ChannelNameHash(
					Config.ChannelSet->Channels[ArcTerrain::GBaseChannelIndex]);
			}
			const FIoHash WeightKey = ArcTerrainWeights::ComputeCellWeightKey(RecipeKey, PresentLayers,
				BaseChannelNameHash, Grid.Resolution());
			HashPod(Hash, WeightKey);
			EndLeafTiming(ArcTerrainDiag::ELeafTiming::TileKeyWeight, WeightStartSeconds);
		}
		else if (Rail == EArcTerrainRail::Data)
		{
			const double DataStartSeconds = BeginLeafTiming();
			check(DataInputs != nullptr);
			// Data masks key on their own recipe: no stamp height field reaches them, and nothing here may
			// invalidate geometry or weights.
			const FIoHash DataKey = ArcTerrainDataMaps::ComputeCellDataKey(TileKey, Config, *DataInputs);
			HashPod(Hash, DataKey);
			EndLeafTiming(ArcTerrainDiag::ELeafTiming::TileKeyData, DataStartSeconds);
		}

		return Hash.Finalize();
	}
}

#endif // WITH_EDITOR
