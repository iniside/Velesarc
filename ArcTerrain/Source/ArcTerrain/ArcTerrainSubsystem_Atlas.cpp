// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainSubsystem.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainDebugDraw.h"
#include "ArcTerrainDiagnostics.h"
#include "ArcTerrainSettings.h"
#include "ArcTerrainInvokers.h"
#include "ArcTerrainModule.h"
#include "Collision/ArcTerrainCollisionBuilder.h"
#include "Collision/ArcTerrainCollisionComponent.h"
#include "Collision/ArcTerrainHeightfieldRef.h"
#include "Composite/ArcTerrainCompositeShaders.h"
#include "Composite/ArcTerrainCpuComposite.h"
#include "DataMaps/ArcTerrainDataMapSample.h"
#include "Lighting/ArcTerrainFarOcclusionComponent.h"
#include "Lighting/ArcTerrainLightingComponent.h"
#include "Render/ArcTerrainAtlasPrepare.h"
#include "Render/ArcTerrainCBTRenderer.h"
#include "Render/ArcTerrainCellMath.h"
#include "Runtime/ArcTerrainRuntimeWorkCoordinator.h"
#include "Render/ArcTerrainDensityTiers.h"
#include "Render/ArcTerrainHeightBounds.h"
#include "Render/ArcTerrainRenderComponent.h"
#include "Render/ArcTerrainVertexFactory.h"
#include "Stamps/ArcTerrainMeshModifier.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "Surface/ArcTerrainSurfaceSample.h"
#include "Weights/ArcTerrainWeightSlicePacking.h" // GArcTerrainWeightMaxSlices (atlas slice budget)
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h" // FTextureSource dims (native density resolve) + CPU source cache
#include "Engine/Texture2DArray.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/Optional.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PooledRenderTarget.h"
#include "RHIGPUReadback.h"
#include "RHIGlobals.h"
#include "RenderResource.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "UObject/ObjectKey.h"

#if WITH_EDITOR
#include "ArcTerrainTileBake.h" // tile lattice + ComputeTileCacheKey
#include "ArcTerrainTileBlobs.h" // the three rails' DDC payloads
#include "ArcTerrainTileCache.h" // async DDC probe handle polling (height + weight bake rails)
#include "ArcTerrainTileIO.h"
#include "ArcTerrainTileNames.h"
#include "Async/Async.h"
#include "TimerManager.h"
#include "DataMaps/ArcTerrainDataMapBake.h"
#include "DataMaps/ArcTerrainDataMapKey.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Stamps/ArcTerrainTexturePatchModifier.h"
#include "Subsystems/ImportSubsystem.h"
#include "Weights/ArcTerrainWeightBuilder.h" // FArcTerrainWeightPlane + ComputeCellWeightKey
#include "UObject/UnrealType.h" // FPropertyChangedEvent + GET_MEMBER_NAME_CHECKED (settings-changed handler)
#endif

bool UArcTerrainSubsystem::IsCellPublishable(AArcTerrain& Terrain, FArcTerrainCellKey Key,
	const FArcTerrainCellState& Cell, const FArcTerrainTileGrid& HeightGrid) const
{
	// bCompositeFullyResolved is NOT a term, and must not become one: this runs only for a sector that
	// holds no slice, where CompositeCell has nowhere to publish and defers, so the flag cannot move
	// until admission grants the slice — gating on it closes a cycle with no entry point. Admitting a
	// partially resolved cell costs nothing: the slice clear publishes base level for it and its own
	// backoff rail (bCompositeResolveRetryPending) recomposites until the source resolves.
	//
	// NO TILE ON DISK IS A PUBLISHABLE STATE, not a wait: that cell's ground is the modifier stack's
	// composite, and gating on the tile would leave a terrain with no baked tiles permanently invisible.
	// A cell the modifier stack has touched (Version > 0) publishes that composite even while its tile is
	// still coming, so only untouched ground whose tile is known but not loaded holds the sector out —
	// admitting that one would draw the slice clear at base level and then pop.
	const FIntPoint HeightTileKey = ArcTerrainTileAccess::TileKeyFromCell(HeightGrid, Key);
	return Cell.Version > 0 || !HeightGrid.IsValid()
		|| !ArcTerrainTileAccess::HasTile(Terrain, EArcTerrainRail::Height, 0, HeightTileKey, &HeightGrid)
		|| ArcTerrainTileAccess::FindLoadedTile(Terrain, EArcTerrainRail::Height, 0, HeightTileKey).IsValid();
}

// FILE-SCOPED and unique (CLAUDE.md C++ Code Style) — nothing here may gain external linkage.
namespace ArcTerrainAtlasFillLocal_2c74
{
	// The staging texture is CELL-sized, so the cut runs here and the render thread only locks and
	// copies. False leaves OutBytes empty, which is what makes "no tile" a source the fill simply does
	// not have rather than a partial one.
	bool CutCellFloatPlane(const FArcTerrainTileGrid& Grid, const FArcTerrainTilePayloadPtr& Payload,
		int32 PlaneIndex, FArcTerrainCellKey Key, int32 ExpectedResolution, TArray<uint8>& OutBytes,
		FVector2f& OutHeightRange)
	{
		OutBytes.Reset();
		OutHeightRange = ArcTerrainTileAtlasRender::InvalidHeightRange();
		if (!Payload.IsValid())
		{
			return false;
		}

		TArray<float> Floats;
		int32 Resolution = 0;
		if (!ArcTerrainTileAccess::ExtractCellFloats(Grid, *Payload, PlaneIndex, Key, Floats, Resolution)
			|| Resolution != ExpectedResolution)
		{
			return false;
		}

		float MinHeight = TNumericLimits<float>::Max();
		float MaxHeight = -TNumericLimits<float>::Max();
		for (float Height : Floats)
		{
			if (!FMath::IsFinite(Height))
			{
				MinHeight = TNumericLimits<float>::Max();
				MaxHeight = -TNumericLimits<float>::Max();
				break;
			}
			MinHeight = FMath::Min(MinHeight, Height);
			MaxHeight = FMath::Max(MaxHeight, Height);
		}

		OutHeightRange = FVector2f(MinHeight, MaxHeight);
		OutBytes.SetNumUninitialized(Floats.Num() * sizeof(float));
		FMemory::Memcpy(OutBytes.GetData(), Floats.GetData(), Floats.Num() * sizeof(float));
		return true;
	}

	// The whole coarse tile IS the slice: routed through the cell cutter the sub-rect resolves to a
	// cell-sized 35 against a 1027 payload, and the cut fails into a diagnostic counter, not an error.
	bool BuildCoarseSectorAtlasFill(FArcTerrainSectorId SectorId, const FArcTerrainTilePayloadPtr& HeightTile,
		const FArcTerrainTileAtlasLayout& Layout, const FArcTerrainTileAtlas& Atlas,
		FArcTerrainAtlasCellFill& OutFill)
	{
		const int32 HeightSlice = Atlas.FindHeightSlice(SectorId);
		if (HeightSlice == INDEX_NONE || !HeightTile.IsValid())
		{
			ArcTerrainDiag::NoteAtlasHeightMiss(ArcTerrainDiag::EAtlasHeightMiss::NoTile);
			return false;
		}

		const int32 SliceSide = Layout.GetHeightSliceSide();
		const int64 SliceTexels = static_cast<int64>(SliceSide) * SliceSide;
		if (HeightTile->Resolution() != SliceSide || HeightTile->Floats.Num() != SliceTexels)
		{
			ArcTerrainDiag::NoteAtlasHeightMiss(ArcTerrainDiag::EAtlasHeightMiss::CoarseResolutionMismatch);
			ArcTerrainDiag::NoteCoarseResolutionMismatch(SectorId.Key, SectorId.Tier, HeightTile->Resolution(),
				SliceSide);
			return false;
		}

		ArcTerrainDiag::NoteAtlasFill(ArcTerrainDiag::EAtlasFillSource::Tile);
		OutFill.HeightSlice = HeightSlice;
		OutFill.HeightDestOrigin = FIntPoint::ZeroValue;
		OutFill.HeightTileRes = SliceSide;
		OutFill.HeightWrite = FArcTerrainAtlasCellFill::EHeightWrite::CoarseWholeSlice;
		OutFill.HeightRange = HeightTile->Header.PlaneQuantMin.IsValidIndex(0)
			&& HeightTile->Header.PlaneQuantMax.IsValidIndex(0)
			? FVector2f(HeightTile->Header.PlaneQuantMin[0], HeightTile->Header.PlaneQuantMax[0])
			: ArcTerrainTileAtlasRender::InvalidHeightRange();
		OutFill.HeightStagingBytes.SetNumUninitialized(HeightTile->Floats.Num() * sizeof(float));
		FMemory::Memcpy(OutFill.HeightStagingBytes.GetData(), HeightTile->Floats.GetData(),
			HeightTile->Floats.Num() * sizeof(float));
		return true;
	}

	// THE ONE HASH -> GLOBAL INDEX TRANSLATION ON THE READ SIDE. The file names each plane by its
	// channel's name hash; every consumer downstream addresses the atlas by (index/4, index%4), so the
	// resolution happens here, against the CURRENT channel set, and the result is never written back.
	// OutChannels is FArcTerrainAtlasCellFill::BakedWeightSliceChannels: source slice -> GLOBAL channel
	// index, in the order the staging layers are filled.
	//
	// THE SLICE BUDGET LIVES HERE, not in the tile file: the 5-bit slot table addresses at most
	// GArcTerrainWeightMaxSlices layers.
	bool CutWeightPlanes(FIntPoint Key, const FArcTerrainTilePayloadPtr& Payload,
		const UArcTerrainChannelSet& ChannelSet, int32 ExpectedResolution,
		TFunctionRef<bool(int32 PlaneIndex, TArray<uint8>& OutBytes, int32& OutResolution)> ExtractPlane,
		TArray<TArray<uint8>>& OutPlanes, TArray<int32>& OutChannels, bool& bOutUnresolvable)
	{
		OutPlanes.Reset();
		OutChannels.Reset();
		bOutUnresolvable = false;
		if (!Payload.IsValid())
		{
			return false;
		}
		if (Payload->Planes.IsEmpty())
		{
			bOutUnresolvable = true;
			return false;
		}

		TArray<uint32> ChannelHashes;
		ChannelHashes.Reserve(ChannelSet.Channels.Num());
		for (FName Channel : ChannelSet.Channels)
		{
			ChannelHashes.Add(ArcTerrain::ChannelNameHash(Channel));
		}

		for (int32 PlaneIndex = 0; PlaneIndex < Payload->Planes.Num(); ++PlaneIndex)
		{
			const uint32 PlaneId = Payload->Header.PlaneIds[PlaneIndex];
			const int32 ChannelIndex = ChannelHashes.IndexOfByKey(PlaneId);
			if (ChannelIndex == INDEX_NONE)
			{
				// A channel deleted or renamed since the bake. Skipping is the only safe answer: the plane
				// has no address in this set, and any substitute would paint another channel's slot.
				//
				// Game thread only, so the set needs no lock. Bounded by the number of DISTINCT unknown ids,
				// not by one flag for the whole session: a second renamed channel has to be reportable too.
				static TSet<uint32> LoggedUnknownPlaneIds;
				bool bAlreadyLogged = false;
				LoggedUnknownPlaneIds.Add(PlaneId, &bAlreadyLogged);
				if (!bAlreadyLogged)
				{
					UE_LOGF(LogArcTerrain, Warning,
						"[Weight] Tile (%d,%d) carries plane id 0x%08X, which names no channel of the "
						"terrain's channel set — the plane is dropped from the atlas. Logged once per id.",
						Payload->TileKey.X, Payload->TileKey.Y, PlaneId);
				}
				continue;
			}

			TArray<uint8> Bytes;
			int32 Resolution = 0;
			if (!ExtractPlane(PlaneIndex, Bytes, Resolution) || Resolution != ExpectedResolution)
			{
				OutPlanes.Reset();
				OutChannels.Reset();
				return false;
			}
			OutPlanes.Add(MoveTemp(Bytes));
			OutChannels.Add(ChannelIndex);
		}

		if (OutPlanes.IsEmpty())
		{
			// Every id named no channel: re-walking this payload against this set cannot ever answer
			// differently, so the caller must stop marking the cell for refill.
			bOutUnresolvable = true;
			return false;
		}

		if (OutPlanes.Num() <= ArcTerrainWeights::GArcTerrainWeightMaxSlices)
		{
			return true;
		}

		// Keep the top-K by MAX weight IN THIS CELL and log the drop — never a silent truncation.
		// Two-pass: rank, then rebuild the kept lists (no RemoveAt-in-loop).
		struct FPlaneRank
		{
			int32 PlaneIndex = 0;
			uint8 MaxWeight = 0;
		};
		TArray<FPlaneRank> Ranks;
		Ranks.Reserve(OutPlanes.Num());
		for (int32 PlaneIndex = 0; PlaneIndex < OutPlanes.Num(); ++PlaneIndex)
		{
			uint8 MaxWeight = 0;
			for (uint8 Texel : OutPlanes[PlaneIndex])
			{
				MaxWeight = FMath::Max(MaxWeight, Texel);
			}
			Ranks.Add(FPlaneRank{ PlaneIndex, MaxWeight });
		}
		Ranks.Sort([&OutChannels](const FPlaneRank& A, const FPlaneRank& B)
		{
			if (A.MaxWeight != B.MaxWeight)
			{
				return A.MaxWeight > B.MaxWeight;
			}
			return OutChannels[A.PlaneIndex] < OutChannels[B.PlaneIndex];
		});

		TArray<int32> KeptRanks;
		KeptRanks.Reserve(ArcTerrainWeights::GArcTerrainWeightMaxSlices);
		for (int32 Rank = 0; Rank < ArcTerrainWeights::GArcTerrainWeightMaxSlices; ++Rank)
		{
			KeptRanks.Add(Ranks[Rank].PlaneIndex);
		}
		KeptRanks.Sort([&OutChannels](int32 A, int32 B) { return OutChannels[A] < OutChannels[B]; });

		TArray<int32> DroppedChannels;
		DroppedChannels.Reserve(Ranks.Num() - ArcTerrainWeights::GArcTerrainWeightMaxSlices);
		for (int32 Rank = ArcTerrainWeights::GArcTerrainWeightMaxSlices; Rank < Ranks.Num(); ++Rank)
		{
			DroppedChannels.Add(OutChannels[Ranks[Rank].PlaneIndex]);
		}
		DroppedChannels.Sort();

		TArray<TArray<uint8>> KeptPlanes;
		TArray<int32> KeptChannels;
		KeptPlanes.Reserve(KeptRanks.Num());
		KeptChannels.Reserve(KeptRanks.Num());
		for (int32 PlaneIndex : KeptRanks)
		{
			KeptPlanes.Add(MoveTemp(OutPlanes[PlaneIndex]));
			KeptChannels.Add(OutChannels[PlaneIndex]);
		}

		UE_LOGF(LogArcTerrain, Warning,
			"[Weight] Cut key (%d,%d): %d weight planes exceed the %d-slice atlas budget — kept top-%d "
			"by max weight in this cut, dropped %d (channels %ls).",
			Key.X, Key.Y, OutPlanes.Num(), ArcTerrainWeights::GArcTerrainWeightMaxSlices,
			ArcTerrainWeights::GArcTerrainWeightMaxSlices, DroppedChannels.Num(),
			*FString::JoinBy(DroppedChannels, TEXT(","), [](int32 Channel) { return FString::FromInt(Channel); }));

		OutPlanes = MoveTemp(KeptPlanes);
		OutChannels = MoveTemp(KeptChannels);
		return true;
	}

	bool CutCellWeightPlanes(FArcTerrainCellKey Key, const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayloadPtr& Payload, const UArcTerrainChannelSet& ChannelSet,
		int32 ExpectedResolution, TArray<TArray<uint8>>& OutPlanes, TArray<int32>& OutChannels,
		bool& bOutUnresolvable)
	{
		return CutWeightPlanes(Key, Payload, ChannelSet, ExpectedResolution,
			[&Grid, &Payload, Key](int32 PlaneIndex, TArray<uint8>& OutBytes, int32& OutResolution)
			{
				return ArcTerrainTileAccess::ExtractCellPlane(Grid, *Payload, PlaneIndex, Key, OutBytes,
					OutResolution);
			},
			OutPlanes, OutChannels, bOutUnresolvable);
	}

	bool CutGroupWeightPlanes(FIntPoint GroupKey, const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayloadPtr& Payload, const UArcTerrainChannelSet& ChannelSet,
		int32 ExpectedResolution, int32 GroupQuads, int32 GroupsPerTileEdge,
		TArray<TArray<uint8>>& OutPlanes, TArray<int32>& OutChannels, bool& bOutUnresolvable)
	{
		return CutWeightPlanes(GroupKey, Payload, ChannelSet, ExpectedResolution,
			[&Grid, &Payload, GroupKey, GroupQuads, GroupsPerTileEdge](int32 PlaneIndex,
				TArray<uint8>& OutBytes, int32& OutResolution)
			{
				return ArcTerrainTileAccess::ExtractGroupPlane(Grid, *Payload, PlaneIndex, GroupKey,
					GroupQuads, GroupsPerTileEdge, OutBytes, OutResolution);
			},
			OutPlanes, OutChannels, bOutUnresolvable);
	}

	// One coarse tile is the group's whole weight state, so the fill takes a single group-sized sub-rect of
	// it into a slice-sized destination — not the per-cell walk tier 0 runs.
	bool BuildCoarseGroupAtlasFill(FArcTerrainWeightGroupId GroupId, const FArcTerrainTileGrid& TierGrid,
		const FArcTerrainTilePayloadPtr& WeightTile, const UArcTerrainChannelSet& ChannelSet,
		int32 GroupQuads, int32 GroupsPerTileEdge, const FArcTerrainTileAtlasLayout& Layout,
		const FArcTerrainTileAtlas& Atlas, FArcTerrainAtlasCellFill& OutFill)
	{
		const int32 WeightSlice = Atlas.FindWeightSlice(GroupId);
		if (WeightSlice == INDEX_NONE || !WeightTile.IsValid())
		{
			return false;
		}

		// The CUT side, not the tier grid's tile side: the group's texel count is tier-independent, and the
		// value is handed straight to the cut as its expected resolution.
		const int32 CutSide = Layout.GetWeightSliceSide();
		bool bUnresolvable = false;
		if (!CutGroupWeightPlanes(GroupId.Key, TierGrid, WeightTile, ChannelSet, CutSide, GroupQuads,
			GroupsPerTileEdge, OutFill.WeightStagingPlanes, OutFill.BakedWeightSliceChannels, bUnresolvable))
		{
			return false;
		}

		OutFill.WeightSlice = WeightSlice;
		OutFill.WeightDestOrigin = FIntPoint::ZeroValue;
		OutFill.WeightTileRes = CutSide;
		return true;
	}
}

namespace ArcTerrainAtlasScheduleLocal_9c31
{
	struct FAuthorityEventTelemetry
	{
		explicit FAuthorityEventTelemetry(FArcTerrainState& InState)
			: State(InState)
		{
		}

		~FAuthorityEventTelemetry()
		{
			if (bHeightDirtied)
			{
				State.PipelineTelemetry.IncrementAtlasEffect(EArcTerrainPipelineAtlasEffect::HeightDirtied);
			}
			if (bWeightDirtied)
			{
				State.PipelineTelemetry.IncrementAtlasEffect(EArcTerrainPipelineAtlasEffect::WeightDirtied);
			}
			if (bAdmissionWoken)
			{
				State.PipelineTelemetry.IncrementAtlasEffect(EArcTerrainPipelineAtlasEffect::AdmissionWoken);
			}
			if (!bHeightDirtied && !bWeightDirtied && !bAdmissionWoken)
			{
				State.PipelineTelemetry.IncrementAtlasIgnored(
					bMatchedTarget && !bExplicitIgnore
						? EArcTerrainPipelineAtlasIgnore::AlreadySatisfied
						: IgnoreReason);
			}
		}

		void MarkHeightTarget(bool bAlreadyDirty)
		{
			bMatchedTarget = true;
			bHeightDirtied |= !bAlreadyDirty;
		}

		void MarkWeightTarget(bool bAlreadyDirty)
		{
			bMatchedTarget = true;
			bWeightDirtied |= !bAlreadyDirty;
		}

		void MarkAdmissionTarget(bool bAlreadyWoken)
		{
			bMatchedTarget = true;
			bAdmissionWoken |= !bAlreadyWoken;
		}

		void SetIgnored(EArcTerrainPipelineAtlasIgnore Reason)
		{
			bExplicitIgnore = true;
			IgnoreReason = Reason;
		}

		FArcTerrainState& State;
		bool bHeightDirtied = false;
		bool bWeightDirtied = false;
		bool bAdmissionWoken = false;
		bool bMatchedTarget = false;
		bool bExplicitIgnore = false;
		EArcTerrainPipelineAtlasIgnore IgnoreReason = EArcTerrainPipelineAtlasIgnore::NoMatchingTarget;
	};

	void MapTileToCells(const FArcTerrainTileGrid& Grid, FIntPoint TileKey, TArray<FArcTerrainCellKey>& OutCells)
	{
		OutCells.Reset();
		if (!Grid.IsValid() || Grid.CellsPerTileEdge <= 0)
		{
			return;
		}
		const int32 MinX = TileKey.X * Grid.CellsPerTileEdge;
		const int32 MinY = TileKey.Y * Grid.CellsPerTileEdge;
		OutCells.Reserve(Grid.CellsPerTileEdge * Grid.CellsPerTileEdge);
		for (int32 OffsetY = 0; OffsetY < Grid.CellsPerTileEdge; ++OffsetY)
		{
			for (int32 OffsetX = 0; OffsetX < Grid.CellsPerTileEdge; ++OffsetX)
			{
				OutCells.Emplace(MinX + OffsetX, MinY + OffsetY);
			}
		}
	}

	bool IsHeightDesired(const FArcTerrainState& State, FArcTerrainSectorId Id)
	{
		for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
		{
			if (Sector.Id == Id)
			{
				return true;
			}
		}
		return false;
	}

	bool IsWeightDesired(const FArcTerrainState& State, FArcTerrainWeightGroupId Id)
	{
		for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
		{
			if (Group.Tier == Id.Tier && Group.Key == Id.Key)
			{
				return true;
			}
		}
		return false;
	}

	const FArcTerrainResidentSector* FindDesiredHeight(const FArcTerrainState& State, FArcTerrainSectorId Id)
	{
		for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
		{
			if (Sector.Id == Id)
			{
				return &Sector;
			}
		}
		return nullptr;
	}

	const FArcTerrainResidentWeightGroup* FindDesiredWeight(const FArcTerrainState& State,
		FArcTerrainWeightGroupId Id)
	{
		for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
		{
			if (Group.Tier == Id.Tier && Group.Key == Id.Key)
			{
				return &Group;
			}
		}
		return nullptr;
	}

	void OrphanHeightWork(FArcTerrainAtlasSchedule& Schedule, FArcTerrainSectorId Id)
	{
		Schedule.PendingHeightAdmissions.Remove(Id);
		Schedule.DirtyHeightSectors.Remove(Id);
		Schedule.WokenHeightAdmissions.Remove(Id);
	}

	void OrphanWeightWork(FArcTerrainAtlasSchedule& Schedule, FArcTerrainWeightGroupId Id)
	{
		Schedule.PendingWeightAdmissions.Remove(Id);
		Schedule.DirtyWeightGroups.Remove(Id);
		Schedule.WokenWeightAdmissions.Remove(Id);
	}

	void WakeNearestRefusedHeight(FArcTerrainState& State)
	{
		FArcTerrainSectorId BestId;
		double BestDistSq = TNumericLimits<double>::Max();
		bool bFound = false;
		for (const FArcTerrainSectorId& Id : State.AtlasSchedule.RefusedHeightSectors)
		{
			const FArcTerrainResidentSector* Desired = FindDesiredHeight(State, Id);
			if (Desired == nullptr)
			{
				continue;
			}
			if (!bFound || Desired->DistSq < BestDistSq)
			{
				bFound = true;
				BestDistSq = Desired->DistSq;
				BestId = Id;
			}
		}
		if (bFound)
		{
			State.AtlasSchedule.WokenHeightAdmissions.Add(BestId);
		}
	}

	void WakeNearestRefusedWeight(FArcTerrainState& State)
	{
		FArcTerrainWeightGroupId BestId;
		double BestDistSq = TNumericLimits<double>::Max();
		bool bFound = false;
		for (const FArcTerrainWeightGroupId& Id : State.AtlasSchedule.RefusedWeightGroups)
		{
			const FArcTerrainResidentWeightGroup* Desired = FindDesiredWeight(State, Id);
			if (Desired == nullptr)
			{
				continue;
			}
			if (!bFound || Desired->DistSq < BestDistSq)
			{
				bFound = true;
				BestDistSq = Desired->DistSq;
				BestId = Id;
			}
		}
		if (bFound)
		{
			State.AtlasSchedule.WokenWeightAdmissions.Add(BestId);
		}
	}

	void ReseedCurrentWindow(FArcTerrainState& State, FArcTerrainTileAtlas* Atlas,
		FAuthorityEventTelemetry* EventTelemetry = nullptr)
	{
		FArcTerrainAtlasSchedule& Schedule = State.AtlasSchedule;
		for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
		{
			Schedule.PendingHeightAdmissions.Add(Sector.Id);
			const bool bAlreadyWoken = Schedule.WokenHeightAdmissions.Contains(Sector.Id);
			Schedule.WokenHeightAdmissions.Add(Sector.Id);
			if (EventTelemetry != nullptr)
			{
				EventTelemetry->MarkAdmissionTarget(bAlreadyWoken);
			}
			if (Atlas != nullptr && Atlas->IsSectorAdmitted(Sector.Id))
			{
				const bool bAlreadyDirty = Schedule.DirtyHeightSectors.Contains(Sector.Id);
				Schedule.DirtyHeightSectors.Add(Sector.Id);
				if (EventTelemetry != nullptr)
				{
					EventTelemetry->MarkHeightTarget(bAlreadyDirty);
				}
			}
		}
		for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
		{
			const FArcTerrainWeightGroupId GroupId{ Group.Tier, Group.Key };
			Schedule.PendingWeightAdmissions.Add(GroupId);
			const bool bAlreadyWoken = Schedule.WokenWeightAdmissions.Contains(GroupId);
			Schedule.WokenWeightAdmissions.Add(GroupId);
			if (EventTelemetry != nullptr)
			{
				EventTelemetry->MarkAdmissionTarget(bAlreadyWoken);
			}
			if (Atlas != nullptr && Atlas->IsWeightGroupAdmitted(GroupId))
			{
				const bool bAlreadyDirty = Schedule.DirtyWeightGroups.Contains(GroupId);
				Schedule.DirtyWeightGroups.Add(GroupId);
				if (EventTelemetry != nullptr)
				{
					EventTelemetry->MarkWeightTarget(bAlreadyDirty);
				}
			}
		}
		if (Atlas != nullptr)
		{
			TArray<FArcTerrainSectorId> Admitted;
			Atlas->GetAdmittedSectors(Admitted);
			for (const FArcTerrainSectorId& SectorId : Admitted)
			{
				const bool bAlreadyDirty = Schedule.DirtyHeightSectors.Contains(SectorId);
				Schedule.DirtyHeightSectors.Add(SectorId);
				if (EventTelemetry != nullptr)
				{
					EventTelemetry->MarkHeightTarget(bAlreadyDirty);
				}
			}
			TArray<FArcTerrainWeightGroupId> AdmittedWeights;
			Atlas->GetAdmittedWeightGroups(AdmittedWeights);
			for (const FArcTerrainWeightGroupId& GroupId : AdmittedWeights)
			{
				const bool bAlreadyDirty = Schedule.DirtyWeightGroups.Contains(GroupId);
				Schedule.DirtyWeightGroups.Add(GroupId);
				if (EventTelemetry != nullptr)
				{
					EventTelemetry->MarkWeightTarget(bAlreadyDirty);
				}
			}
		}
	}

	void InvalidateQueuedAtlasWork(FArcTerrainState& State)
	{
		FArcTerrainAtlasSchedule& Schedule = State.AtlasSchedule;
		Schedule.PendingHeightAdmissions.Reset();
		Schedule.DirtyHeightSectors.Reset();
		Schedule.RefusedHeightSectors.Reset();
		Schedule.WokenHeightAdmissions.Reset();
		Schedule.PendingWeightAdmissions.Reset();
		Schedule.DirtyWeightGroups.Reset();
		Schedule.RefusedWeightGroups.Reset();
		Schedule.WokenWeightAdmissions.Reset();
		Schedule.DirtyAtlasCells.Reset();
		State.CbtTopologyCache = FArcTerrainCBTTopologyCache();
		State.CbtPublishCache.bPublished = false;
		Schedule.bCbtTopologyInFlight = false;
		Schedule.PendingCbtTopologySignature = 0;
		Schedule.PendingCbtLayoutGeneration = 0;
		Schedule.InFlight.Reset();
	}

	uint64 MakeCbtTopologySignature(const FArcTerrainFaceGrid& Grid, const FArcTerrainTileAtlasLayout& Layout)
	{
		uint64 Hash = HashCombine(GetTypeHash(Grid.FaceQuads), GetTypeHash(Grid.NumFacesPerEdge));
		Hash = HashCombine(Hash, GetTypeHash(Grid.RequestedFacesPerEdge));
		Hash = HashCombine(Hash, GetTypeHash(Grid.HalfFaces));
		Hash = HashCombine(Hash, GetTypeHash(Layout.SectorShift));
		Hash = HashCombine(Hash, GetTypeHash(Layout.HeightQuads));
		return Hash;
	}

	bool HasAtlasWork(const FArcTerrainState& State, bool bHasPendingAtlasFills)
	{
		const FArcTerrainAtlasSchedule& Schedule = State.AtlasSchedule;
		return Schedule.ResidencyPublicationRevision != Schedule.ConsumedResidencyPublicationRevision
			|| Schedule.DesiredAdmissionRevision != Schedule.ConsumedDesiredAdmissionRevision
			|| Schedule.LayoutGeneration != Schedule.ConsumedLayoutGeneration
			|| Schedule.RenderKnobRevision != Schedule.ConsumedRenderKnobRevision
			|| State.PendingAtlasAuthorityEvents.Num() > 0
			|| Schedule.WokenHeightAdmissions.Num() > 0
			|| Schedule.WokenWeightAdmissions.Num() > 0
			|| Schedule.DirtyHeightSectors.Num() > 0
			|| Schedule.DirtyWeightGroups.Num() > 0
			|| Schedule.DirtyAtlasCells.Num() > 0
			|| State.PendingRefillCells.Num() > 0
			|| Schedule.InFlight.Num() > 0
			|| bHasPendingAtlasFills
			|| Schedule.bCbtTopologyInFlight
			|| !State.CbtPublishCache.bPublished;
	}

	bool HasRemainingHeightFill(const FArcTerrainAtlasSchedule& Schedule, FArcTerrainSectorId SectorId)
	{
		if (Schedule.DirtyHeightSectors.Contains(SectorId))
		{
			return true;
		}
		for (const FArcTerrainAtlasInFlightKey& Key : Schedule.InFlight)
		{
			if (Key.SectorId == SectorId
				&& (Key.Kind == EArcTerrainAtlasPrepareKind::HeightCell
					|| Key.Kind == EArcTerrainAtlasPrepareKind::HeightCoarse))
			{
				return true;
			}
		}
		return false;
	}

	bool HasRemainingWeightFill(const FArcTerrainAtlasSchedule& Schedule, FArcTerrainWeightGroupId GroupId)
	{
		if (Schedule.DirtyWeightGroups.Contains(GroupId))
		{
			return true;
		}
		for (const FArcTerrainAtlasInFlightKey& Key : Schedule.InFlight)
		{
			if (Key.GroupId == GroupId
				&& (Key.Kind == EArcTerrainAtlasPrepareKind::WeightCell
					|| Key.Kind == EArcTerrainAtlasPrepareKind::WeightCoarse))
			{
				return true;
			}
		}
		return false;
	}

	bool TryMarkHeightRingReady(FArcTerrainTileAtlas& Atlas, const FArcTerrainAtlasSchedule& Schedule,
		FArcTerrainSectorId SectorId)
	{
		if (HasRemainingHeightFill(Schedule, SectorId) || Atlas.IsSectorAwaitingTile(SectorId))
		{
			return false;
		}
		return Atlas.MarkSectorRingReady(SectorId);
	}

	bool TryMarkWeightRingReady(FArcTerrainTileAtlas& Atlas, const FArcTerrainAtlasSchedule& Schedule,
		FArcTerrainWeightGroupId GroupId)
	{
		if (HasRemainingWeightFill(Schedule, GroupId) || Atlas.IsWeightGroupAwaitingTile(GroupId))
		{
			return false;
		}
		return Atlas.MarkWeightGroupRingReady(GroupId);
	}

	void DropDirtyAtlasCellsForGroup(FArcTerrainAtlasSchedule& Schedule, FArcTerrainWeightGroupId GroupId,
		int32 WeightSliceShift)
	{
		if (GroupId.Tier != 0)
		{
			return;
		}
		TArray<FArcTerrainCellKey> DropKeys;
		for (const FArcTerrainCellKey& CellKey : Schedule.DirtyAtlasCells)
		{
			if (ArcTerrainAtlas::GroupKeyFromCell(CellKey, WeightSliceShift) == GroupId.Key)
			{
				DropKeys.Add(CellKey);
			}
		}
		for (const FArcTerrainCellKey& CellKey : DropKeys)
		{
			Schedule.DirtyAtlasCells.Remove(CellKey);
		}
	}

	void ProcessAtlasAuthorityEvents(AArcTerrain& Terrain, FArcTerrainState& State,
		FArcTerrainTileAtlas* Atlas, const FArcTerrainTileAtlasLayout& Layout,
		const FArcTerrainTierLadder& Ladder, const FArcTerrainWeightLadder& WeightLadder)
	{
		TArray<FArcTerrainTileAuthorityEvent> Events = MoveTemp(State.PendingAtlasAuthorityEvents);
		State.PendingAtlasAuthorityEvents.Reset();
		for (const FArcTerrainTileAuthorityEvent& Event : Events)
		{
			State.PipelineTelemetry.IncrementAtlasAuthorityConsumed(Event.Reason);
			FAuthorityEventTelemetry EventTelemetry(State);
			if (Event.Reason == EArcTerrainTileAuthorityReason::PendingHeightReset
				|| Event.Reason == EArcTerrainTileAuthorityReason::LayoutChanged)
			{
				// PendingHeightReset/LayoutChanged zero key is not a real tile.
				ReseedCurrentWindow(State, Atlas, &EventTelemetry);
				continue;
			}
			if (Event.Reason == EArcTerrainTileAuthorityReason::StampBoundsChanged
				|| Event.Rail == EArcTerrainRail::Data)
			{
				EventTelemetry.SetIgnored(EArcTerrainPipelineAtlasIgnore::StampBoundsOrData);
				continue;
			}

			FArcTerrainTileGrid EventGrid;
			if (!ArcTerrainTileAccess::ResolveGrid(Terrain, Event.Rail, Event.Tier, EventGrid))
			{
				EventTelemetry.SetIgnored(EArcTerrainPipelineAtlasIgnore::GridUnavailable);
				continue;
			}

			if (Event.Rail == EArcTerrainRail::Height && Event.Tier != 0)
			{
				if (!Ladder.Tiers.IsValidIndex(Event.Tier))
				{
					EventTelemetry.SetIgnored(EArcTerrainPipelineAtlasIgnore::TierInvalid);
					continue;
				}
				const int32 CellSectorShift = Ladder.Tiers[Event.Tier].CellSectorShift;
				TArray<FArcTerrainSectorId> CoarseHeightCandidates;
				for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
				{
					if (Sector.Id.Tier == Event.Tier)
					{
						CoarseHeightCandidates.AddUnique(Sector.Id);
					}
				}
				if (Atlas != nullptr)
				{
					TArray<FArcTerrainSectorId> Admitted;
					Atlas->GetAdmittedSectors(Admitted);
					for (const FArcTerrainSectorId& SectorId : Admitted)
					{
						if (SectorId.Tier == Event.Tier)
						{
							CoarseHeightCandidates.AddUnique(SectorId);
						}
					}
				}
				for (const FArcTerrainSectorId& SectorId : CoarseHeightCandidates)
				{
					if (ArcTerrainSubsystemPrivate::CoarseSectorTileKey(EventGrid, SectorId.Key, CellSectorShift)
						!= Event.TileKey)
					{
						continue;
					}
					const bool bAlreadyDirty = State.AtlasSchedule.DirtyHeightSectors.Contains(SectorId);
					State.AtlasSchedule.DirtyHeightSectors.Add(SectorId);
					EventTelemetry.MarkHeightTarget(bAlreadyDirty);
					if (State.AtlasSchedule.PendingHeightAdmissions.Contains(SectorId))
					{
						const bool bAlreadyWoken = State.AtlasSchedule.WokenHeightAdmissions.Contains(SectorId);
						State.AtlasSchedule.WokenHeightAdmissions.Add(SectorId);
						EventTelemetry.MarkAdmissionTarget(bAlreadyWoken);
					}
				}
				continue;
			}

			if (Event.Rail == EArcTerrainRail::Weight && Event.Tier != 0)
			{
				if (!WeightLadder.Tiers.IsValidIndex(Event.Tier))
				{
					EventTelemetry.SetIgnored(EArcTerrainPipelineAtlasIgnore::TierInvalid);
					continue;
				}
				const int32 SliceShift = WeightLadder.Tiers[Event.Tier].SliceShift;
				TArray<FArcTerrainWeightGroupId> CoarseWeightCandidates;
				for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
				{
					if (Group.Tier == Event.Tier)
					{
						CoarseWeightCandidates.AddUnique(FArcTerrainWeightGroupId{ Group.Tier, Group.Key });
					}
				}
				if (Atlas != nullptr)
				{
					TArray<FArcTerrainWeightGroupId> AdmittedWeights;
					Atlas->GetAdmittedWeightGroups(AdmittedWeights);
					for (const FArcTerrainWeightGroupId& GroupId : AdmittedWeights)
					{
						if (GroupId.Tier == Event.Tier)
						{
							CoarseWeightCandidates.AddUnique(GroupId);
						}
					}
				}
				for (const FArcTerrainWeightGroupId& GroupId : CoarseWeightCandidates)
				{
					if (ArcTerrainSubsystemPrivate::CoarseWeightGroupTileKey(EventGrid, GroupId.Key, SliceShift)
						!= Event.TileKey)
					{
						continue;
					}
					const bool bAlreadyDirty = State.AtlasSchedule.DirtyWeightGroups.Contains(GroupId);
					State.AtlasSchedule.DirtyWeightGroups.Add(GroupId);
					EventTelemetry.MarkWeightTarget(bAlreadyDirty);
					if (State.AtlasSchedule.PendingWeightAdmissions.Contains(GroupId))
					{
						const bool bAlreadyWoken = State.AtlasSchedule.WokenWeightAdmissions.Contains(GroupId);
						State.AtlasSchedule.WokenWeightAdmissions.Add(GroupId);
						EventTelemetry.MarkAdmissionTarget(bAlreadyWoken);
					}
				}
				continue;
			}

			TArray<FArcTerrainCellKey> TileCells;
			MapTileToCells(EventGrid, Event.TileKey, TileCells);
			for (const FArcTerrainCellKey& CellKey : TileCells)
			{
				if (Event.Rail == EArcTerrainRail::Height)
				{
					const FArcTerrainSectorId SectorId{
						0, ArcTerrainAtlas::GroupKeyFromCell(CellKey, Layout.SectorShift) };
					if (IsHeightDesired(State, SectorId) || (Atlas != nullptr && Atlas->IsSectorAdmitted(SectorId)))
					{
						const bool bAlreadyDirty = State.AtlasSchedule.DirtyHeightSectors.Contains(SectorId);
						State.AtlasSchedule.DirtyHeightSectors.Add(SectorId);
						EventTelemetry.MarkHeightTarget(bAlreadyDirty);
						if (State.AtlasSchedule.PendingHeightAdmissions.Contains(SectorId))
						{
							const bool bAlreadyWoken = State.AtlasSchedule.WokenHeightAdmissions.Contains(SectorId);
							State.AtlasSchedule.WokenHeightAdmissions.Add(SectorId);
							EventTelemetry.MarkAdmissionTarget(bAlreadyWoken);
						}
					}
				}
				else if (Event.Rail == EArcTerrainRail::Weight)
				{
					const FArcTerrainWeightGroupId GroupId{
						0, ArcTerrainAtlas::GroupKeyFromCell(CellKey, Layout.WeightSliceShift) };
					if (IsWeightDesired(State, GroupId) || (Atlas != nullptr && Atlas->IsWeightGroupAdmitted(GroupId)))
					{
						const bool bAlreadyDirty = State.AtlasSchedule.DirtyWeightGroups.Contains(GroupId);
						State.AtlasSchedule.DirtyWeightGroups.Add(GroupId);
						EventTelemetry.MarkWeightTarget(bAlreadyDirty);
						if (State.AtlasSchedule.PendingWeightAdmissions.Contains(GroupId))
						{
							const bool bAlreadyWoken = State.AtlasSchedule.WokenWeightAdmissions.Contains(GroupId);
							State.AtlasSchedule.WokenWeightAdmissions.Add(GroupId);
							EventTelemetry.MarkAdmissionTarget(bAlreadyWoken);
						}
					}
					const bool bAlreadyDirtyCell = State.AtlasSchedule.DirtyAtlasCells.Contains(CellKey);
					State.AtlasSchedule.DirtyAtlasCells.Add(CellKey);
					EventTelemetry.MarkWeightTarget(bAlreadyDirtyCell);
					if (FArcTerrainCellState* Cell = State.Cells.Find(CellKey))
					{
						EventTelemetry.MarkWeightTarget(Cell->bAtlasRefillPending);
						Cell->bAtlasRefillPending = true;
					}
					else
					{
						const bool bAlreadyPending = State.PendingRefillCells.Contains(CellKey);
						State.PendingRefillCells.Add(CellKey);
						EventTelemetry.MarkWeightTarget(bAlreadyPending);
					}
				}
			}
		}
	}
}

bool UArcTerrainSubsystem::BuildCellHeightFill(FArcTerrainCellKey Key, const FArcTerrainCellState* Cell,
	const FArcTerrainTileGrid& HeightGrid, const FArcTerrainTilePayloadPtr& HeightTile,
	const FArcTerrainTileAtlasLayout& Layout, const FArcTerrainTileAtlas& Atlas,
	FArcTerrainAtlasCellFill& OutFill) const
{
	const FArcTerrainSectorId SectorId{ 0, ArcTerrainAtlas::GroupKeyFromCell(Key, Layout.SectorShift) };
	const int32 HeightSlice = Atlas.FindHeightSlice(SectorId);
	if (HeightSlice == INDEX_NONE)
	{
		return false;
	}

	OutFill.Key = Key;
	OutFill.HeightSlice = HeightSlice;
	OutFill.HeightTileRes = Layout.HeightQuads + 3;
	OutFill.HeightDestOrigin = ArcTerrainAtlas::HeightTileOriginInSlice(SectorId, Key, Layout);
	OutFill.HeightWrite = FArcTerrainAtlasCellFill::EHeightWrite::Tier0Cell;

	bool bAnySource = false;

	// A cell that composited this session publishes its own tile from CompositeCell, and copying a baked
	// payload over it would undo the edit the artist is mid-stroke on — unless the payload is newer than
	// that publication. Without the second clause, a tile arriving at a cell whose sector is already
	// admitted never reaches the atlas (the fill bounces off the gate, and the cell is no longer a
	// composite candidate either), so the slice keeps drawing the stale composite for as long as the
	// sector stays admitted.
	if (Cell == nullptr || Cell->AtlasHeightVersion == 0
		|| Cell->HeightTileSerial > Cell->AtlasHeightTileSerial)
	{
		if (ArcTerrainAtlasFillLocal_2c74::CutCellFloatPlane(HeightGrid, HeightTile, /*PlaneIndex=*/0, Key,
			OutFill.HeightTileRes, OutFill.HeightStagingBytes, OutFill.HeightRange))
		{
			bAnySource = true;
		}
		else if (HeightTile.IsValid())
		{
			ArcTerrainDiag::NoteAtlasHeightMiss(ArcTerrainDiag::EAtlasHeightMiss::CutFailed);
			ArcTerrainDiag::NoteAtlasCutFailure(Key, HeightTile->TileKey,
				static_cast<int32>(HeightTile->Rail), HeightTile->Resolution(), HeightTile->Floats.Num(),
				HeightGrid, OutFill.HeightTileRes);
		}
		else
		{
			ArcTerrainDiag::NoteAtlasHeightMiss(ArcTerrainDiag::EAtlasHeightMiss::NoTile);
		}
	}
	else
	{
		ArcTerrainDiag::NoteAtlasHeightMiss(ArcTerrainDiag::EAtlasHeightMiss::Withheld);
	}

	return bAnySource;
}

bool UArcTerrainSubsystem::BuildCellWeightFill(FArcTerrainCellKey Key, const FArcTerrainCellState* Cell,
	const UArcTerrainChannelSet* ChannelSet,
	const FArcTerrainTileGrid& WeightGrid, const FArcTerrainTilePayloadPtr& WeightTile,
	const FArcTerrainTileAtlasLayout& Layout, const FArcTerrainTileAtlas& Atlas,
	FArcTerrainAtlasCellFill& OutFill, bool& bOutWeightUnresolvable) const
{
	bOutWeightUnresolvable = false;

	const FIntPoint WeightGroupKey = ArcTerrainAtlas::GroupKeyFromCell(Key, Layout.WeightSliceShift);
	const int32 WeightSlice = Atlas.FindWeightSlice(FArcTerrainWeightGroupId{ 0, WeightGroupKey });
	if (WeightSlice == INDEX_NONE)
	{
		return false;
	}

	OutFill.Key = Key;
	OutFill.WeightSlice = WeightSlice;
	OutFill.WeightTileRes = Layout.WeightQuads + 3;
	OutFill.WeightDestOrigin = FIntPoint(
		(Key.X - (WeightGroupKey.X << Layout.WeightSliceShift)) * Layout.WeightQuads,
		(Key.Y - (WeightGroupKey.Y << Layout.WeightSliceShift)) * Layout.WeightQuads);

	bool bAnySource = false;
	if (Cell != nullptr && !Cell->TileSet.WeightRTs.IsEmpty())
	{
		// The live tiles already carry the terrain-global (channel/4, channel%4) packing the atlas is
		// keyed on, so they map one-to-one onto the atlas arrays.
		OutFill.LiveWeightResources.Reserve(Cell->TileSet.WeightRTs.Num());
		for (UTextureRenderTarget2D* WeightRT : Cell->TileSet.WeightRTs)
		{
			OutFill.LiveWeightResources.Add(
				WeightRT != nullptr ? WeightRT->GameThread_GetRenderTargetResource() : nullptr);
		}
		bAnySource = true;
	}
	else if (ChannelSet == nullptr)
	{
		// No channel set is not "not ready": no baked plane has an address at all, and no arrival of
		// bytes changes that.
		bOutWeightUnresolvable = true;
	}
	else if (ArcTerrainAtlasFillLocal_2c74::CutCellWeightPlanes(Key, WeightGrid, WeightTile, *ChannelSet,
		OutFill.WeightTileRes, OutFill.WeightStagingPlanes, OutFill.BakedWeightSliceChannels,
		bOutWeightUnresolvable))
	{
		bAnySource = true;
	}

	return bAnySource;
}

void UArcTerrainSubsystem::TickTileAtlases()
{
	using namespace ArcTerrainSubsystemPrivate;
	using namespace ArcTerrainAtlasScheduleLocal_9c31;

	if (!FApp::CanEverRender())
	{
		return;
	}

	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		AArcTerrain* Terrain = TerrainPair.Key.Get();
		if (Terrain == nullptr)
		{
			const double DeadReconcileStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
			InvalidateCBTBaseMesh(TerrainPair.Value);
			TerrainPair.Value.CbtPublishCache.bPublished = true;
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasResidencyReconcile,
				DeadReconcileStartSeconds);
			continue;
		}
		FArcTerrainState& State = TerrainPair.Value;
		FArcTerrainAtlasSchedule& Schedule = State.AtlasSchedule;

		bool bRingReadyFlipped = false;
		if (State.TileAtlas.IsValid())
		{
			bRingReadyFlipped = FlushPendingAtlasFills(*Terrain, State);
		}

		const FMatrix CurrentLocalToWorld = Terrain->GetActorTransform().ToMatrixWithScale();
		const bool bTransformChanged = !State.CbtPublishCache.bPublished
			|| CurrentLocalToWorld != State.CbtPublishCache.LocalToWorld;
		const bool bPreviewMissing = State.TileAtlas.IsValid()
			&& State.TileAtlas->GetResources().IsValid()
			&& !PreviewManager.HasAdmittedRender(*Terrain);
		const bool bBaseHeightChanged = State.TileAtlas.IsValid()
			&& State.TileAtlas->GetLayout().IsValid()
			&& State.TileAtlas->GetLayout().BaseHeightCm != Terrain->HeightOptions.BaseHeightCm;
		bool bHasPendingAtlasFills = false;
		for (const FPendingAtlasFill& Item : PendingAtlasFills)
		{
			if (Item.TerrainId == State.CollisionTerrainId)
			{
				bHasPendingAtlasFills = true;
				break;
			}
		}
		if (!HasAtlasWork(State, bHasPendingAtlasFills)
			&& !bTransformChanged && !bPreviewMissing && !bBaseHeightChanged)
		{
			if (bRingReadyFlipped && State.TileAtlas.IsValid() && State.TileAtlas->GetLayout().IsValid())
			{
				const FArcTerrainTileAtlasLayout& Layout = State.TileAtlas->GetLayout();
				const FArcTerrainFaceGrid FaceGrid = ArcTerrain::ResolveFaceGrid(
					Terrain->Config.TexelSizeCm(), Terrain->FaceQuadsShift, Terrain->WorldExtentCm);
				FArcTerrainResidencyRings Rings;
				State.TileAtlas->BuildResidencyRings(State.TierLadder, State.WeightLadder, Rings);
				if (Rings != State.CbtPublishCache.Rings)
				{
					PublishCBTBaseMesh(*Terrain, State, Layout, FaceGrid, Rings);
					const double PreviewBindStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
					const bool bRebound = PreviewManager.EnsureTerrainRender(
						*Terrain, Terrain->Config, *State.TileAtlas, Rings, State.PublishedRenderIdentity);
					ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasPreviewBind,
						PreviewBindStartSeconds);
					if (bRebound)
					{
						ArcTerrainDiag::NoteAtlasPreviewRebind();
					}
				}
			}
			continue;
		}

		const double ResidencyReconcileStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		const FArcTerrainConfig& Config = Terrain->Config;
		if (Config.QuadsPerCell() < 1 || Config.CellSize() <= 0.0)
		{
			InvalidateCBTBaseMesh(State);
			State.CbtPublishCache.bPublished = true;
			Schedule.ConsumedLayoutGeneration = Schedule.LayoutGeneration;
			Schedule.ConsumedResidencyPublicationRevision = Schedule.ResidencyPublicationRevision;
			Schedule.ConsumedDesiredAdmissionRevision = Schedule.DesiredAdmissionRevision;
			Schedule.ConsumedRenderKnobRevision = Schedule.RenderKnobRevision;
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasResidencyReconcile,
				ResidencyReconcileStartSeconds);
			continue;
		}

		const FResolvedResidency Residency = ResolveResidency(*Terrain);
		const FArcTerrainTileAtlasLayout Layout = ResolveTerrainAtlasLayout(*Terrain, State, Residency);

		if (!Layout.IsValid())
		{
			if (State.TileAtlas.IsValid())
			{
				State.TileAtlas->ReleaseAll();
			}
			InvalidateCBTBaseMesh(State);
			State.CbtPublishCache.bPublished = true;
			Schedule.ConsumedLayoutGeneration = Schedule.LayoutGeneration;
			Schedule.ConsumedResidencyPublicationRevision = Schedule.ResidencyPublicationRevision;
			Schedule.ConsumedDesiredAdmissionRevision = Schedule.DesiredAdmissionRevision;
			Schedule.ConsumedRenderKnobRevision = Schedule.RenderKnobRevision;
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasResidencyReconcile,
				ResidencyReconcileStartSeconds);
			continue;
		}

		if (!State.TileAtlas.IsValid())
		{
			State.TileAtlas = MakeShared<FArcTerrainTileAtlas>();
		}
		FArcTerrainTileAtlas& Atlas = *State.TileAtlas;
		if (Atlas.SetLayout(Layout))
		{
			++Schedule.LayoutGeneration;
			InvalidateQueuedAtlasWork(State);
			ReleasePendingAtlasFills(State.CollisionTerrainId);
			PreviewManager.InvalidateBindingSignature(*Terrain);
			ReseedCurrentWindow(State, &Atlas);
		}

		const bool bOnlyPublication =
			Schedule.ResidencyPublicationRevision == Schedule.ConsumedResidencyPublicationRevision
			&& Schedule.DesiredAdmissionRevision == Schedule.ConsumedDesiredAdmissionRevision
			&& Schedule.LayoutGeneration == Schedule.ConsumedLayoutGeneration
			&& State.PendingAtlasAuthorityEvents.Num() == 0
			&& Schedule.WokenHeightAdmissions.Num() == 0
			&& Schedule.WokenWeightAdmissions.Num() == 0
			&& Schedule.DirtyHeightSectors.Num() == 0
			&& Schedule.DirtyWeightGroups.Num() == 0
			&& Schedule.DirtyAtlasCells.Num() == 0
			&& State.PendingRefillCells.Num() == 0;
		if (bOnlyPublication)
		{
			const FArcTerrainFaceGrid FaceGrid = ArcTerrain::ResolveFaceGrid(
				Terrain->Config.TexelSizeCm(), Terrain->FaceQuadsShift, Terrain->WorldExtentCm);
			FArcTerrainResidencyRings Rings;
			Atlas.BuildResidencyRings(State.TierLadder, State.WeightLadder, Rings);
			FlushPendingAtlasFills(*Terrain, State);
			PublishCBTBaseMesh(*Terrain, State, Layout, FaceGrid, Rings);
			const double PreviewBindStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
			const bool bRebound = PreviewManager.EnsureTerrainRender(
				*Terrain, Config, Atlas, Rings, State.PublishedRenderIdentity);
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasPreviewBind,
				PreviewBindStartSeconds);
			if (bRebound)
			{
				ArcTerrainDiag::NoteAtlasPreviewRebind();
			}
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasResidencyReconcile,
				ResidencyReconcileStartSeconds);
			continue;
		}

		FArcTerrainTileGrid HeightGrid;
		ArcTerrainTileAccess::ResolveGrid(*Terrain, EArcTerrainRail::Height, 0, HeightGrid);
		FArcTerrainTileGrid WeightGrid;
		ArcTerrainTileAccess::ResolveGrid(*Terrain, EArcTerrainRail::Weight, 0, WeightGrid);

		// One lattice per tier, resolved once: the readiness gate below asks a coarse sector for its tile,
		// and that key only means anything on that tier's own grid.
		const FArcTerrainTierLadder& Ladder = State.TierLadder;
		FArcTerrainTileGrid TierHeightGrids[ArcTerrain::MaxTiers];
		bool bTierHeightGridValid[ArcTerrain::MaxTiers] = {};
		TierHeightGrids[0] = HeightGrid;
		bTierHeightGridValid[0] = HeightGrid.IsValid();
		for (int32 Tier = 1; Tier < Ladder.Tiers.Num(); ++Tier)
		{
			bTierHeightGridValid[Tier] = ArcTerrainTileAccess::ResolveGrid(*Terrain,
				EArcTerrainRail::Height, Tier, TierHeightGrids[Tier]);
		}

		const FArcTerrainWeightLadder& WeightLadder = State.WeightLadder;
		FArcTerrainTileGrid TierWeightGrids[ArcTerrain::MaxTiers];
		bool bTierWeightGridValid[ArcTerrain::MaxTiers] = {};
		TierWeightGrids[0] = WeightGrid;
		bTierWeightGridValid[0] = WeightGrid.IsValid();
		for (int32 Tier = 1; Tier < WeightLadder.Tiers.Num(); ++Tier)
		{
			bTierWeightGridValid[Tier] = ArcTerrainTileAccess::ResolveGrid(*Terrain,
				EArcTerrainRail::Weight, Tier, TierWeightGrids[Tier]);
		}

		ProcessAtlasAuthorityEvents(*Terrain, State, &Atlas, Layout, Ladder, WeightLadder);

		TSet<FArcTerrainSectorId> DesiredHeightIds;
		DesiredHeightIds.Reserve(State.ResidentSectors.Num());
		for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
		{
			DesiredHeightIds.Add(Sector.Id);
		}
		TSet<FArcTerrainWeightGroupId> DesiredWeightIds;
		DesiredWeightIds.Reserve(State.ResidentWeightGroups.Num());
		for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
		{
			DesiredWeightIds.Add(FArcTerrainWeightGroupId{ Group.Tier, Group.Key });
		}

		const bool bProcessDesiredList =
			Schedule.DesiredAdmissionRevision != Schedule.ConsumedDesiredAdmissionRevision
			|| Schedule.LayoutGeneration != Schedule.ConsumedLayoutGeneration;
		const bool bPublicationUnconsumed =
			Schedule.ResidencyPublicationRevision != Schedule.ConsumedResidencyPublicationRevision;
		TSet<FArcTerrainSectorId> RefusedHeightThisPass;
		TSet<FArcTerrainWeightGroupId> RefusedWeightThisPass;
		if (bProcessDesiredList)
		{
			TArray<FArcTerrainSectorId> Admitted;
			Atlas.GetAdmittedSectors(Admitted);
			for (const FArcTerrainSectorId& SectorId : Admitted)
			{
				if (!DesiredHeightIds.Contains(SectorId))
				{
					OrphanHeightWork(Schedule, SectorId);
					Atlas.EvictSector(SectorId);
					WakeNearestRefusedHeight(State);
				}
			}
			TArray<FArcTerrainWeightGroupId> AdmittedWeights;
			Atlas.GetAdmittedWeightGroups(AdmittedWeights);
			for (const FArcTerrainWeightGroupId& GroupId : AdmittedWeights)
			{
				if (!DesiredWeightIds.Contains(GroupId))
				{
					OrphanWeightWork(Schedule, GroupId);
					Atlas.EvictWeightGroup(GroupId);
					WakeNearestRefusedWeight(State);
				}
			}
			for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
			{
				Schedule.PendingHeightAdmissions.Add(Sector.Id);
				Schedule.WokenHeightAdmissions.Add(Sector.Id);
			}
			for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
			{
				const FArcTerrainWeightGroupId GroupId{ Group.Tier, Group.Key };
				Schedule.PendingWeightAdmissions.Add(GroupId);
				Schedule.WokenWeightAdmissions.Add(GroupId);
			}
		}
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasResidencyReconcile,
			ResidencyReconcileStartSeconds);

		const double AdmissionStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		FArcTerrainAtlasFillJob Job;
		Job.Layout = Layout;

		const int32 CellsPerSectorEdge = 1 << Layout.SectorShift;
		TSet<FArcTerrainSectorId> NewlyAdmittedSectors;
		TSet<FArcTerrainSectorId> AbsentHeightReadySectors;
		int32 NumAlreadyAdmitted = 0;
		int32 NumNotReady = 0;
		int32 NumAdmitRefused = 0;
		int32 NumNewlyAdmitted = 0;
		double NearestNotReadyDistSq = TNumericLimits<double>::Max();

		if (bProcessDesiredList || bPublicationUnconsumed)
		{
			for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
			{
				if (Atlas.IsSectorAdmitted(Sector.Id))
				{
					++NumAlreadyAdmitted;
					Atlas.UpdateSectorPriority(Sector.Id, Sector.DistSq, Sector.bPinned);
				}
			}
		}

		TArray<FArcTerrainSectorId> HeightAdmitQueue;
		if (bProcessDesiredList)
		{
			for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
			{
				if (!Atlas.IsSectorAdmitted(Sector.Id) && !RefusedHeightThisPass.Contains(Sector.Id))
				{
					HeightAdmitQueue.Add(Sector.Id);
				}
			}
		}
		else
		{
			for (const FArcTerrainSectorId& SectorId : Schedule.WokenHeightAdmissions)
			{
				if (DesiredHeightIds.Contains(SectorId) && !Atlas.IsSectorAdmitted(SectorId)
					&& !RefusedHeightThisPass.Contains(SectorId))
				{
					HeightAdmitQueue.Add(SectorId);
				}
			}
		}

		for (const FArcTerrainSectorId& SectorId : HeightAdmitQueue)
		{
			if (RefusedHeightThisPass.Contains(SectorId) || Atlas.IsSectorAdmitted(SectorId))
			{
				continue;
			}
			const FArcTerrainResidentSector* Desired = FindDesiredHeight(State, SectorId);
			if (Desired == nullptr)
			{
				Schedule.PendingHeightAdmissions.Remove(SectorId);
				continue;
			}

			bool bSectorReady = true;
			if (SectorId.Tier == 0)
			{
				for (int32 OffsetY = 0; OffsetY < CellsPerSectorEdge && bSectorReady; ++OffsetY)
				{
					for (int32 OffsetX = 0; OffsetX < CellsPerSectorEdge && bSectorReady; ++OffsetX)
					{
						const FArcTerrainCellKey CellKey(
							(SectorId.Key.X << Layout.SectorShift) + OffsetX,
							(SectorId.Key.Y << Layout.SectorShift) + OffsetY);
						if (const FArcTerrainCellState* Cell = State.Cells.Find(CellKey))
						{
							bSectorReady = IsCellPublishable(*Terrain, CellKey, *Cell, HeightGrid);
						}
					}
				}
			}
			else
			{
				bSectorReady = false;
				if (bTierHeightGridValid[SectorId.Tier] && Ladder.Tiers.IsValidIndex(SectorId.Tier))
				{
					const FIntPoint TileKey = CoarseSectorTileKey(TierHeightGrids[SectorId.Tier],
						SectorId.Key, Ladder.Tiers[SectorId.Tier].CellSectorShift);
					bSectorReady = ArcTerrainTileAccess::FindLoadedTile(*Terrain, EArcTerrainRail::Height,
						SectorId.Tier, TileKey).IsValid();
				}
			}
			if (!bSectorReady)
			{
				++NumNotReady;
				ArcTerrainDiag::NoteAtlasDeferred();
				NearestNotReadyDistSq = FMath::Min(NearestNotReadyDistSq, Desired->DistSq);
				Schedule.PendingHeightAdmissions.Add(SectorId);
				continue;
			}

			const FArcTerrainTileAtlas::FHeightAdmissionResult HeightResult =
				Atlas.AdmitSectorTracked(SectorId, Desired->DistSq, Desired->bPinned);
			if (HeightResult.EvictedId.IsSet())
			{
				const FArcTerrainSectorId EvictedId = HeightResult.EvictedId.GetValue();
				OrphanHeightWork(Schedule, EvictedId);
				if (DesiredHeightIds.Contains(EvictedId))
				{
					Schedule.RefusedHeightSectors.Add(EvictedId);
					RefusedHeightThisPass.Add(EvictedId);
				}
				else
				{
					WakeNearestRefusedHeight(State);
				}
			}
			if (!HeightResult.bAdmitted)
			{
				++NumAdmitRefused;
				ArcTerrainDiag::NoteAtlasRefusal();
				Schedule.RefusedHeightSectors.Add(SectorId);
				Schedule.PendingHeightAdmissions.Remove(SectorId);
				continue;
			}

			++NumNewlyAdmitted;
			ArcTerrainDiag::NoteAtlasAdmission();
			ArcTerrainDiag::NoteSliceGranted(/*bWeight=*/false);
			Job.HeightSlicesToClear.Add(HeightResult.AssignedSlice);
			NewlyAdmittedSectors.Add(SectorId);
			Schedule.DirtyHeightSectors.Add(SectorId);
			Schedule.PendingHeightAdmissions.Remove(SectorId);
			Schedule.RefusedHeightSectors.Remove(SectorId);
			if (SectorId.Tier == 0)
			{
				for (int32 OffsetY = 0; OffsetY < CellsPerSectorEdge; ++OffsetY)
				{
					for (int32 OffsetX = 0; OffsetX < CellsPerSectorEdge; ++OffsetX)
					{
						FArcTerrainCellState* Cell = State.Cells.Find(FArcTerrainCellKey(
							(SectorId.Key.X << Layout.SectorShift) + OffsetX,
							(SectorId.Key.Y << Layout.SectorShift) + OffsetY));
						if (Cell != nullptr)
						{
							Cell->AtlasHeightVersion = 0;
							Cell->AtlasHeightTileSerial = 0;
						}
					}
				}
			}
		}

		if (ArcTerrainDiag::IsOn() && (NumNewlyAdmitted > 0 || NumNotReady > 0 || NumAdmitRefused > 0))
		{
			UE_LOGF(LogArcTerrain, Log,
				"[Residency] admit: resident %d | held %d + new %d | NOTREADY %d (nearest %.2f km) "
				"| REFUSED %d | atlas holds %d",
				State.ResidentSectors.Num(), NumAlreadyAdmitted, NumNewlyAdmitted,
				NumNotReady,
				NumNotReady > 0 ? FMath::Sqrt(NearestNotReadyDistSq) * 0.00001 : -1.0,
				NumAdmitRefused, Atlas.GetAdmittedSectorCount());
		}

		const bool bLogWeightAdmission = ArcTerrainDiag::IsWeightCompositeVerbose();
		const int32 CellsPerWeightGroupEdge = 1 << Layout.WeightSliceShift;
		TSet<FArcTerrainWeightGroupId> NewlyAdmittedWeightGroups;
		TSet<FArcTerrainWeightGroupId> LiveWeightFilledGroups;
		TSet<FArcTerrainWeightGroupId> AbsentWeightReadyGroups;

		if (bProcessDesiredList || bPublicationUnconsumed)
		{
			for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
			{
				const FArcTerrainWeightGroupId GroupId{ Group.Tier, Group.Key };
				if (Atlas.IsWeightGroupAdmitted(GroupId))
				{
					Atlas.UpdateWeightGroupPriority(GroupId, Group.DistSq, Group.bPinned);
				}
			}
		}

		TArray<FArcTerrainWeightGroupId> WeightAdmitQueue;
		if (bProcessDesiredList)
		{
			for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
			{
				const FArcTerrainWeightGroupId GroupId{ Group.Tier, Group.Key };
				if (!Atlas.IsWeightGroupAdmitted(GroupId) && !RefusedWeightThisPass.Contains(GroupId))
				{
					WeightAdmitQueue.Add(GroupId);
				}
			}
		}
		else
		{
			for (const FArcTerrainWeightGroupId& GroupId : Schedule.WokenWeightAdmissions)
			{
				if (DesiredWeightIds.Contains(GroupId) && !Atlas.IsWeightGroupAdmitted(GroupId)
					&& !RefusedWeightThisPass.Contains(GroupId))
				{
					WeightAdmitQueue.Add(GroupId);
				}
			}
		}

		for (const FArcTerrainWeightGroupId& GroupId : WeightAdmitQueue)
		{
			if (RefusedWeightThisPass.Contains(GroupId) || Atlas.IsWeightGroupAdmitted(GroupId))
			{
				continue;
			}
			const FArcTerrainResidentWeightGroup* Desired = FindDesiredWeight(State, GroupId);
			if (Desired == nullptr)
			{
				Schedule.PendingWeightAdmissions.Remove(GroupId);
				continue;
			}

			const FArcTerrainTileAtlas::FWeightAdmissionResult WeightResult =
				Atlas.AdmitWeightGroupTracked(GroupId, Desired->DistSq, Desired->bPinned);
			if (WeightResult.EvictedId.IsSet())
			{
				const FArcTerrainWeightGroupId EvictedId = WeightResult.EvictedId.GetValue();
				OrphanWeightWork(Schedule, EvictedId);
				if (DesiredWeightIds.Contains(EvictedId))
				{
					Schedule.RefusedWeightGroups.Add(EvictedId);
					RefusedWeightThisPass.Add(EvictedId);
				}
				else
				{
					WakeNearestRefusedWeight(State);
				}
			}
			if (!WeightResult.bAdmitted)
			{
				ArcTerrainDiag::NoteAtlasRefusal();
				Schedule.RefusedWeightGroups.Add(GroupId);
				Schedule.PendingWeightAdmissions.Remove(GroupId);
				if (bLogWeightAdmission)
				{
					UE_LOGF(LogArcTerrain, Warning,
						"[WeightAdmit] '%ls' group=(%d,%d) sector=(%d,%d) admission attempted, dist=%.1f m "
						"pinned=%ls -> %ls",
						*Terrain->GetName(), GroupId.Key.X, GroupId.Key.Y,
						Atlas.SectorKeyFromWeightGroup(GroupId.Key).Key.X,
						Atlas.SectorKeyFromWeightGroup(GroupId.Key).Key.Y,
						FMath::Sqrt(Desired->DistSq) * 0.01, Desired->bPinned ? TEXT("yes") : TEXT("no"),
						*Atlas.DescribeWeightAdmission(GroupId, Desired->DistSq));
				}
				continue;
			}

			ArcTerrainDiag::NoteAtlasAdmission();
			ArcTerrainDiag::NoteSliceGranted(/*bWeight=*/true);
			Job.WeightSlicesToClear.Add(WeightResult.AssignedSlice);
			NewlyAdmittedWeightGroups.Add(GroupId);
			Schedule.DirtyWeightGroups.Add(GroupId);
			Schedule.PendingWeightAdmissions.Remove(GroupId);
			Schedule.RefusedWeightGroups.Remove(GroupId);
		}

		Schedule.WokenHeightAdmissions.Reset();
		Schedule.WokenWeightAdmissions.Reset();
		if (bProcessDesiredList)
		{
			Schedule.ConsumedDesiredAdmissionRevision = Schedule.DesiredAdmissionRevision;
			Schedule.ConsumedLayoutGeneration = Schedule.LayoutGeneration;
		}
		if (bProcessDesiredList || bPublicationUnconsumed)
		{
			Schedule.ConsumedResidencyPublicationRevision = Schedule.ResidencyPublicationRevision;
		}
		FlushPendingAtlasFills(*Terrain, State);
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasAdmission, AdmissionStartSeconds);

		if (State.PendingRefillCells.Num() > 0)
		{
			for (const FArcTerrainCellKey& RefillKey : State.PendingRefillCells)
			{
				Schedule.DirtyAtlasCells.Add(RefillKey);
				Schedule.DirtyWeightGroups.Add(FArcTerrainWeightGroupId{
					0, ArcTerrainAtlas::GroupKeyFromCell(RefillKey, Layout.WeightSliceShift) });
			}
			State.PendingRefillCells.Reset();
		}

		const double HeightSnapshotStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		TSet<FArcTerrainSectorId> HeightFillIds = NewlyAdmittedSectors;
		for (const FArcTerrainSectorId& DirtyId : Schedule.DirtyHeightSectors)
		{
			HeightFillIds.Add(DirtyId);
		}
		for (const FArcTerrainSectorId& FillId : HeightFillIds)
		{
			if (!Atlas.IsSectorAdmitted(FillId))
			{
				Schedule.DirtyHeightSectors.Remove(FillId);
				continue;
			}
			FArcTerrainResidentSector Sector;
			Sector.Id = FillId;
			if (const FArcTerrainResidentSector* Desired = FindDesiredHeight(State, FillId))
			{
				Sector = *Desired;
			}
			const bool bNewlyAdmitted = NewlyAdmittedSectors.Contains(Sector.Id);
			bool bKeepDirty = false;
			bool bSawKU = false;
			bool bSawLoaded = false;

			if (Sector.Id.Tier != 0)
			{
				FTileSourceCapture CoarseCapture;
				if (bTierHeightGridValid[Sector.Id.Tier] && Ladder.Tiers.IsValidIndex(Sector.Id.Tier))
				{
					const FIntPoint TileKey = CoarseSectorTileKey(TierHeightGrids[Sector.Id.Tier],
						Sector.Id.Key, Ladder.Tiers[Sector.Id.Tier].CellSectorShift);
					CoarseCapture = ArcTerrainSubsystemPrivate::CaptureTileSource(*Terrain,
						EArcTerrainRail::Height, Sector.Id.Tier, TileKey,
						&TierHeightGrids[Sector.Id.Tier]);
				}
				if (CoarseCapture.State == ETileSourceState::KnownUnloaded)
				{
					bSawKU = true;
				}
				else if (CoarseCapture.State == ETileSourceState::Loaded)
				{
					bSawLoaded = true;
					const bool bSubmitCoarse = bNewlyAdmitted
						|| !Atlas.IsSectorRingReady(Sector.Id)
						|| CoarseCapture.PayloadId != Atlas.GetSectorHeightTileFingerprint(Sector.Id);
					if (bSubmitCoarse)
					{
						ArcTerrainDiag::NoteAtlasCellWalked();
						const FArcTerrainAtlasInFlightKey InFlightKey{
							EArcTerrainAtlasPrepareKind::HeightCoarse, Sector.Id, FArcTerrainWeightGroupId(),
							FArcTerrainCellKey::ZeroValue };
						if (Schedule.InFlight.Contains(InFlightKey))
						{
							bKeepDirty = true;
						}
						else
						{
							FArcTerrainAtlasPrepareInput Input;
							Input.OwnerId = GetRuntimeOwnerId();
							Input.TerrainId = State.CollisionTerrainId;
							Input.LayoutGeneration = Schedule.LayoutGeneration;
							Input.Kind = EArcTerrainAtlasPrepareKind::HeightCoarse;
							Input.SectorId = Sector.Id;
							Input.Slice = Atlas.FindHeightSlice(Sector.Id);
							Input.SliceGeneration = Atlas.GetHeightSliceGeneration(Sector.Id);
							Input.ExpectedPayloadId = CoarseCapture.PayloadId;
							Input.Layout = Layout;
							Input.ExpectedResolution = Layout.GetHeightSliceSide();
							Input.Source = CoarseCapture;
							if (SubmitAtlasPrepare(State, MoveTemp(Input)))
							{
								Schedule.InFlight.Add(InFlightKey);
							}
							else
							{
								bKeepDirty = true;
							}
						}
					}
				}
				Atlas.SetSectorAwaitingTile(Sector.Id, bSawKU);
				if (!bKeepDirty)
				{
					Schedule.DirtyHeightSectors.Remove(Sector.Id);
				}
				if (!bKeepDirty && !bSawKU && !bSawLoaded)
				{
					AbsentHeightReadySectors.Add(Sector.Id);
				}
				continue;
			}

			FIntPoint MemoTileKey = FIntPoint::ZeroValue;
			FTileSourceCapture MemoCapture;
			bool bHasMemo = false;
			for (int32 OffsetY = 0; OffsetY < CellsPerSectorEdge; ++OffsetY)
			{
				for (int32 OffsetX = 0; OffsetX < CellsPerSectorEdge; ++OffsetX)
				{
					const FArcTerrainCellKey CellKey(
						(Sector.Id.Key.X << Layout.SectorShift) + OffsetX,
						(Sector.Id.Key.Y << Layout.SectorShift) + OffsetY);
					FTileSourceCapture HeightCapture;
					if (HeightGrid.IsValid())
					{
						const FIntPoint HeightTileKey = ArcTerrainTileAccess::TileKeyFromCell(HeightGrid,
							CellKey);
						if (!bHasMemo || HeightTileKey != MemoTileKey)
						{
							MemoTileKey = HeightTileKey;
							bHasMemo = true;
							MemoCapture = ArcTerrainSubsystemPrivate::CaptureTileSource(*Terrain,
								EArcTerrainRail::Height, 0, HeightTileKey, &HeightGrid);
						}
						HeightCapture = MemoCapture;
					}

					FArcTerrainCellState* Cell = State.Cells.Find(CellKey);
					if (Cell != nullptr)
					{
						Cell->HeightTileSerial = HeightCapture.Payload.IsValid()
							? HeightCapture.Payload->PayloadId : 0;
					}

					const bool bWithheld = Cell != nullptr && Cell->AtlasHeightVersion != 0
						&& Cell->HeightTileSerial <= Cell->AtlasHeightTileSerial;
					if (bWithheld)
					{
						ArcTerrainDiag::NoteAtlasCellWalked();
						ArcTerrainDiag::NoteAtlasHeightMiss(ArcTerrainDiag::EAtlasHeightMiss::Withheld);
						if (HeightCapture.State == ETileSourceState::Loaded)
						{
							bSawLoaded = true;
						}
						continue;
					}

					if (HeightCapture.State == ETileSourceState::KnownUnloaded)
					{
						bSawKU = true;
						continue;
					}
					if (HeightCapture.State != ETileSourceState::Loaded)
					{
						continue;
					}

					bSawLoaded = true;
					if (Atlas.IsSectorRingReady(Sector.Id)
						&& HeightCapture.PayloadId == Atlas.GetSectorHeightTileFingerprint(Sector.Id)
						&& !bWithheld)
					{
						continue;
					}
					if (HeightCapture.PayloadId != 0
						&& Cell != nullptr
						&& Cell->AtlasHeightTileSerial == HeightCapture.PayloadId
						&& !bWithheld)
					{
						continue;
					}

					ArcTerrainDiag::NoteAtlasCellWalked();
					const FArcTerrainAtlasInFlightKey InFlightKey{
						EArcTerrainAtlasPrepareKind::HeightCell, Sector.Id, FArcTerrainWeightGroupId(),
						CellKey };
					if (Schedule.InFlight.Contains(InFlightKey))
					{
						bKeepDirty = true;
						continue;
					}

					FArcTerrainAtlasPrepareInput Input;
					Input.OwnerId = GetRuntimeOwnerId();
					Input.TerrainId = State.CollisionTerrainId;
					Input.LayoutGeneration = Schedule.LayoutGeneration;
					Input.Kind = EArcTerrainAtlasPrepareKind::HeightCell;
					Input.SectorId = Sector.Id;
					Input.CellKey = CellKey;
					Input.Slice = Atlas.FindHeightSlice(Sector.Id);
					Input.SliceGeneration = Atlas.GetHeightSliceGeneration(Sector.Id);
					Input.CellPublicationVersion = Cell != nullptr ? Cell->AtlasHeightVersion : 0;
					Input.ExpectedPayloadId = HeightCapture.PayloadId;
					Input.Grid = HeightGrid;
					Input.Layout = Layout;
					Input.ExpectedResolution = Layout.HeightQuads + 3;
					Input.Source = HeightCapture;
					if (SubmitAtlasPrepare(State, MoveTemp(Input)))
					{
						Schedule.InFlight.Add(InFlightKey);
					}
					else
					{
						bKeepDirty = true;
					}
				}
			}

			Atlas.SetSectorAwaitingTile(Sector.Id, bSawKU);
			if (!bKeepDirty)
			{
				Schedule.DirtyHeightSectors.Remove(Sector.Id);
			}
			if (!bKeepDirty && !bSawKU && !bSawLoaded)
			{
				AbsentHeightReadySectors.Add(Sector.Id);
			}
		}
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasHeightSnapshot,
			HeightSnapshotStartSeconds);

		const double WeightSnapshotStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		TSet<FArcTerrainWeightGroupId> WeightFillIds = NewlyAdmittedWeightGroups;
		for (const FArcTerrainWeightGroupId& DirtyId : Schedule.DirtyWeightGroups)
		{
			WeightFillIds.Add(DirtyId);
		}
		for (const FArcTerrainCellKey& DirtyCell : Schedule.DirtyAtlasCells)
		{
			WeightFillIds.Add(FArcTerrainWeightGroupId{
				0, ArcTerrainAtlas::GroupKeyFromCell(DirtyCell, Layout.WeightSliceShift) });
		}
		for (const FArcTerrainWeightGroupId& FillId : WeightFillIds)
		{
			const FArcTerrainWeightGroupId GroupId = FillId;
			if (!Atlas.IsWeightGroupAdmitted(GroupId))
			{
				Schedule.DirtyWeightGroups.Remove(GroupId);
				DropDirtyAtlasCellsForGroup(Schedule, GroupId, Layout.WeightSliceShift);
				continue;
			}
			FArcTerrainResidentWeightGroup Group;
			Group.Tier = GroupId.Tier;
			Group.Key = GroupId.Key;
			if (const FArcTerrainResidentWeightGroup* Desired = FindDesiredWeight(State, GroupId))
			{
				Group = *Desired;
			}
			const bool bNewlyAdmitted = NewlyAdmittedWeightGroups.Contains(GroupId);
			bool bKeepDirty = false;
			bool bSawKU = false;
			bool bSawLoaded = false;

			if (Group.Tier != 0)
			{
				FTileSourceCapture CoarseCapture;
				int32 GroupQuads = 0;
				int32 GroupsPerTileEdge = 0;
				if (bTierWeightGridValid[Group.Tier] && WeightLadder.Tiers.IsValidIndex(Group.Tier))
				{
					const FArcTerrainTileGrid& TierGrid = TierWeightGrids[Group.Tier];
					const FResolvedWeightTier& WeightTier = WeightLadder.Tiers[Group.Tier];
					GroupQuads = WeightTier.CellWeightQuads << WeightTier.SliceShift;
					GroupsPerTileEdge = TierGrid.CellsPerTileEdge >> WeightTier.SliceShift;
					const FIntPoint TileKey = CoarseWeightGroupTileKey(TierGrid, Group.Key,
						WeightTier.SliceShift);
					CoarseCapture = ArcTerrainSubsystemPrivate::CaptureTileSource(*Terrain,
						EArcTerrainRail::Weight, Group.Tier, TileKey, &TierGrid);
				}
				if (CoarseCapture.State == ETileSourceState::KnownUnloaded)
				{
					bSawKU = true;
				}
				else if (CoarseCapture.State == ETileSourceState::Loaded)
				{
					bSawLoaded = true;
					const bool bSubmitCoarse = bNewlyAdmitted
						|| !Atlas.IsWeightGroupRingReady(GroupId)
						|| CoarseCapture.PayloadId != Atlas.GetWeightGroupTileFingerprint(GroupId);
					if (bSubmitCoarse)
					{
						ArcTerrainDiag::NoteAtlasCellWalked();
						const FArcTerrainAtlasInFlightKey InFlightKey{
							EArcTerrainAtlasPrepareKind::WeightCoarse, FArcTerrainSectorId(), GroupId,
							FArcTerrainCellKey::ZeroValue };
						if (Schedule.InFlight.Contains(InFlightKey))
						{
							bKeepDirty = true;
						}
						else if (Terrain->Config.ChannelSet != nullptr)
						{
							FArcTerrainAtlasPrepareInput Input;
							Input.OwnerId = GetRuntimeOwnerId();
							Input.TerrainId = State.CollisionTerrainId;
							Input.LayoutGeneration = Schedule.LayoutGeneration;
							Input.Kind = EArcTerrainAtlasPrepareKind::WeightCoarse;
							Input.GroupId = GroupId;
							Input.Slice = Atlas.FindWeightSlice(GroupId);
							Input.SliceGeneration = Atlas.GetWeightSliceGeneration(GroupId);
							Input.ExpectedPayloadId = CoarseCapture.PayloadId;
							Input.Grid = TierWeightGrids[Group.Tier];
							Input.Layout = Layout;
							Input.ExpectedResolution = Layout.GetWeightSliceSide();
							Input.GroupQuads = GroupQuads;
							Input.GroupsPerTileEdge = GroupsPerTileEdge;
							Input.Source = CoarseCapture;
							ArcTerrainAtlasPrepare::CopyChannelHashIndexMap(*Terrain->Config.ChannelSet,
								Input.ChannelMap);
							if (SubmitAtlasPrepare(State, MoveTemp(Input)))
							{
								Schedule.InFlight.Add(InFlightKey);
							}
							else
							{
								bKeepDirty = true;
							}
						}
					}
				}
				Atlas.SetWeightGroupAwaitingTile(GroupId, bSawKU);
				if (!bKeepDirty)
				{
					Schedule.DirtyWeightGroups.Remove(GroupId);
				}
				if (!bKeepDirty && !bSawKU && !bSawLoaded)
				{
					AbsentWeightReadyGroups.Add(GroupId);
				}
				continue;
			}

			FIntPoint MemoTileKey = FIntPoint::ZeroValue;
			FTileSourceCapture MemoCapture;
			bool bHasMemo = false;
			for (int32 OffsetY = 0; OffsetY < CellsPerWeightGroupEdge; ++OffsetY)
			{
				for (int32 OffsetX = 0; OffsetX < CellsPerWeightGroupEdge; ++OffsetX)
				{
					const FArcTerrainCellKey CellKey(
						(Group.Key.X << Layout.WeightSliceShift) + OffsetX,
						(Group.Key.Y << Layout.WeightSliceShift) + OffsetY);
					FTileSourceCapture WeightCapture;
					if (WeightGrid.IsValid())
					{
						const FIntPoint WeightTileKey = ArcTerrainTileAccess::TileKeyFromCell(WeightGrid,
							CellKey);
						if (!bHasMemo || WeightTileKey != MemoTileKey)
						{
							MemoTileKey = WeightTileKey;
							bHasMemo = true;
							MemoCapture = ArcTerrainSubsystemPrivate::CaptureTileSource(*Terrain,
								EArcTerrainRail::Weight, 0, WeightTileKey, &WeightGrid);
						}
						WeightCapture = MemoCapture;
					}

					FArcTerrainCellState* Cell = State.Cells.Find(CellKey);
					if (bNewlyAdmitted && Cell != nullptr)
					{
						Cell->AtlasWeightTileSerial = 0;
					}
					const bool bMarked = Cell != nullptr
						? Cell->bAtlasRefillPending
						: Schedule.DirtyAtlasCells.Contains(CellKey);
					if (WeightCapture.State == ETileSourceState::Loaded
						&& Atlas.IsWeightGroupRingReady(GroupId)
						&& WeightCapture.PayloadId == Atlas.GetWeightGroupTileFingerprint(GroupId)
						&& !bMarked)
					{
						bSawLoaded = true;
						continue;
					}
					if (WeightCapture.State == ETileSourceState::Loaded
						&& WeightCapture.PayloadId != 0
						&& Cell != nullptr
						&& Cell->AtlasWeightTileSerial == WeightCapture.PayloadId
						&& !bMarked)
					{
						bSawLoaded = true;
						continue;
					}
					if (!bNewlyAdmitted && bMarked)
					{
						ArcTerrainDiag::NoteAtlasRefill();
					}
					ArcTerrainDiag::NoteAtlasCellWalked();

					FArcTerrainAtlasCellFill LiveFill;
					bool bWeightUnresolvable = false;
					const bool bHasLiveWeights = Cell != nullptr && !Cell->TileSet.WeightRTs.IsEmpty();
					if (bHasLiveWeights)
					{
						const bool bFilled = BuildCellWeightFill(CellKey, Cell, Terrain->Config.ChannelSet,
							WeightGrid, WeightCapture.Payload, Layout, Atlas, LiveFill, bWeightUnresolvable);
						if (bFilled && !LiveFill.LiveWeightResources.IsEmpty())
						{
							ArcTerrainDiag::NoteAtlasFill(ArcTerrainDiag::EAtlasFillSource::NoHeight);
							ArcTerrainDiag::NoteAtlasReady();
							Job.Cells.Add(MoveTemp(LiveFill));
							LiveWeightFilledGroups.Add(GroupId);
							Schedule.DirtyAtlasCells.Remove(CellKey);
						}
					}
					else if (WeightCapture.State == ETileSourceState::KnownUnloaded)
					{
						bSawKU = true;
					}
					else if (WeightCapture.State == ETileSourceState::Loaded)
					{
						bSawLoaded = true;
						const FArcTerrainAtlasInFlightKey InFlightKey{
							EArcTerrainAtlasPrepareKind::WeightCell, FArcTerrainSectorId(), GroupId, CellKey };
						if (Schedule.InFlight.Contains(InFlightKey))
						{
							bKeepDirty = true;
							Schedule.DirtyAtlasCells.Add(CellKey);
						}
						else if (Terrain->Config.ChannelSet != nullptr)
						{
							FArcTerrainAtlasPrepareInput Input;
							Input.OwnerId = GetRuntimeOwnerId();
							Input.TerrainId = State.CollisionTerrainId;
							Input.LayoutGeneration = Schedule.LayoutGeneration;
							Input.Kind = EArcTerrainAtlasPrepareKind::WeightCell;
							Input.GroupId = GroupId;
							Input.CellKey = CellKey;
							Input.Slice = Atlas.FindWeightSlice(GroupId);
							Input.SliceGeneration = Atlas.GetWeightSliceGeneration(GroupId);
							Input.ExpectedPayloadId = WeightCapture.PayloadId;
							Input.Grid = WeightGrid;
							Input.Layout = Layout;
							Input.ExpectedResolution = Layout.WeightQuads + 3;
							Input.Source = WeightCapture;
							ArcTerrainAtlasPrepare::CopyChannelHashIndexMap(*Terrain->Config.ChannelSet,
								Input.ChannelMap);
							if (SubmitAtlasPrepare(State, MoveTemp(Input)))
							{
								Schedule.InFlight.Add(InFlightKey);
								Schedule.DirtyAtlasCells.Remove(CellKey);
							}
							else
							{
								bKeepDirty = true;
								Schedule.DirtyAtlasCells.Add(CellKey);
							}
						}
						else
						{
							bWeightUnresolvable = true;
						}
					}
					if (Cell != nullptr)
					{
						const bool bWeightSourceExists =
							!Cell->TileSet.WeightRTs.IsEmpty() || WeightCapture.Payload.IsValid();
						Cell->bAtlasRefillPending = Atlas.FindWeightSlice(GroupId) != INDEX_NONE
							&& bWeightSourceExists && !bHasLiveWeights && !bWeightUnresolvable
							&& !Schedule.InFlight.Contains(FArcTerrainAtlasInFlightKey{
								EArcTerrainAtlasPrepareKind::WeightCell, FArcTerrainSectorId(), GroupId,
								CellKey });
					}
				}
			}

			Atlas.SetWeightGroupAwaitingTile(GroupId, bSawKU);
			if (!bKeepDirty)
			{
				Schedule.DirtyWeightGroups.Remove(GroupId);
				DropDirtyAtlasCellsForGroup(Schedule, GroupId, Layout.WeightSliceShift);
			}
			if (!bKeepDirty && !bSawKU && !bSawLoaded)
			{
				AbsentWeightReadyGroups.Add(GroupId);
			}
		}
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasWeightSnapshot,
			WeightSnapshotStartSeconds);

		const bool bHasFillWork = !Job.IsEmpty() || !Atlas.GetResources().IsValid();
		if (bHasFillWork)
		{
			Atlas.EnqueueFill(MoveTemp(Job));
			ArcTerrainDiag::NoteAtlasInFlight();
		}
		for (const FArcTerrainSectorId& SectorId : AbsentHeightReadySectors)
		{
			TryMarkHeightRingReady(Atlas, Schedule, SectorId);
		}
		for (const FArcTerrainWeightGroupId& GroupId : AbsentWeightReadyGroups)
		{
			TryMarkWeightRingReady(Atlas, Schedule, GroupId);
		}
		for (const FArcTerrainWeightGroupId& GroupId : LiveWeightFilledGroups)
		{
			TryMarkWeightRingReady(Atlas, Schedule, GroupId);
		}

		const FArcTerrainFaceGrid FaceGrid = ArcTerrain::ResolveFaceGrid(
			Terrain->Config.TexelSizeCm(), Terrain->FaceQuadsShift, Terrain->WorldExtentCm);
		FArcTerrainResidencyRings Rings;
		Atlas.BuildResidencyRings(Ladder, State.WeightLadder, Rings);

		const uint64 TopologySignature = MakeCbtTopologySignature(FaceGrid, Layout);
		const bool bNeedCbt = !State.CbtPublishCache.bPublished
			|| bTransformChanged
			|| Schedule.RenderKnobRevision != Schedule.ConsumedRenderKnobRevision
			|| TopologySignature != State.CbtPublishCache.TopologySignature
			|| Atlas.GetResourceGeneration() != State.CbtPublishCache.ResourceGeneration
			|| Rings != State.CbtPublishCache.Rings;
		if (bNeedCbt)
		{
			PublishCBTBaseMesh(*Terrain, State, Layout, FaceGrid, Rings);
		}

		const bool bNeedPreview = bNeedCbt || !PreviewManager.HasAdmittedRender(*Terrain);
		if (bNeedPreview)
		{
			const double PreviewBindStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
			const bool bRebound = PreviewManager.EnsureTerrainRender(
				*Terrain, Config, Atlas, Rings, State.PublishedRenderIdentity);
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasPreviewBind,
				PreviewBindStartSeconds);
			if (bRebound)
			{
				ArcTerrainDiag::NoteAtlasPreviewRebind();
			}
		}
	}
}

void UArcTerrainSubsystem::InvalidateCBTBaseMesh(FArcTerrainState& State)
{
	State.CbtTopologyCache = FArcTerrainCBTTopologyCache();
	State.CbtPublishCache = FArcTerrainCBTPublishCache();
	if (!State.PublishedRenderIdentity.IsValid())
	{
		return;
	}

	const double PublishStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
	GArcTerrainCBTRendererExtension.UnpublishCandidate_GameThread(State.PublishedRenderIdentity);
	ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasCbtPublish, PublishStartSeconds);
	ArcTerrainDiag::NoteAtlasPublication();

	State.PublishedRenderIdentity = FArcTerrainRenderIdentity();
}

void UArcTerrainSubsystem::PublishCBTBaseMesh(const AArcTerrain& Terrain, FArcTerrainState& State,
	const FArcTerrainTileAtlasLayout& Layout, const FArcTerrainFaceGrid& Grid,
	const FArcTerrainResidencyRings& Rings)
{
	using namespace ArcTerrainAtlasScheduleLocal_9c31;

	const int32 NumFaces = Grid.IsValid() ? Grid.NumFaces() : 0;
	if (NumFaces <= 0)
	{
		InvalidateCBTBaseMesh(State);
		State.CbtPublishCache.bPublished = true;
		return;
	}

	const uint64 TopologySignature = MakeCbtTopologySignature(Grid, Layout);
	if (State.CbtTopologyCache.Signature != TopologySignature
		|| State.CbtTopologyCache.NumFaces != NumFaces
		|| State.CbtTopologyCache.Neighbors.Num() != NumFaces * 4
		|| State.CbtTopologyCache.FaceTexelOrigins.Num() != NumFaces)
	{
		FArcTerrainAtlasSchedule& Schedule = State.AtlasSchedule;
		if (!Schedule.bCbtTopologyInFlight
			|| Schedule.PendingCbtTopologySignature != TopologySignature
			|| Schedule.PendingCbtLayoutGeneration != Schedule.LayoutGeneration)
		{
			FArcTerrainCBTTopologyInput TopologyInput;
			TopologyInput.FaceGrid = Grid;
			TopologyInput.TopologySignature = TopologySignature;
			TopologyInput.LayoutGeneration = Schedule.LayoutGeneration;
			if (SubmitCbtTopology(State, TopologyInput))
			{
				Schedule.bCbtTopologyInFlight = true;
				Schedule.PendingCbtTopologySignature = TopologySignature;
				Schedule.PendingCbtLayoutGeneration = Schedule.LayoutGeneration;
			}
		}
		return;
	}

	FArcTerrainCBTBaseMesh BaseMesh;
	const UWorld* World = Terrain.GetWorld();
	++State.ResourceRevision;
	BaseMesh.Identity.Owner = State.RenderOwner;
	BaseMesh.Identity.ResourceRevision = State.ResourceRevision;
	BaseMesh.Identity.RhiEpoch = GArcTerrainCBTRendererExtension.GetRhiEpoch();
	BaseMesh.Identity.WorldType = World != nullptr
		? static_cast<EWorldType::Type>(World->WorldType)
		: EWorldType::None;
	BaseMesh.Identity.SceneKey = World != nullptr ? World->Scene : nullptr;
	BaseMesh.NumFaces = NumFaces;
	BaseMesh.Neighbors = State.CbtTopologyCache.Neighbors;
	BaseMesh.FaceTexelOrigins = State.CbtTopologyCache.FaceTexelOrigins;
	BaseMesh.AtlasResources = State.TileAtlas->GetResources();
	BaseMesh.SectorQuads = Layout.HeightQuads << Layout.SectorShift;
	BaseMesh.FaceQuads = Grid.FaceQuads;
	BaseMesh.SectorQuadsShift = Layout.GetSectorQuadsShift();
	BaseMesh.TexelToLocalCm = ArcTerrainCellMath::CellTexelToLocal(Terrain.Config.CellSize(), Layout.HeightQuads);
	BaseMesh.HeightRings = Rings.HeightTiers;
	BaseMesh.LocalToWorld = Terrain.GetActorTransform().ToMatrixWithScale();
	BaseMesh.PoolDepth = Terrain.CBTPoolDepth;
	BaseMesh.bEnableDistanceCulling = Terrain.bEnableDistanceCulling;
	BaseMesh.DistanceCullRadius = Terrain.DistanceCullRadius;
	BaseMesh.ConservativeMinHeightCm = Terrain.ConservativeMinHeightCm;
	BaseMesh.ConservativeMaxHeightCm = Terrain.ConservativeMaxHeightCm;
	BaseMesh.TargetTrianglePixels = Terrain.TargetTrianglePixels;
	BaseMesh.MaxTriangleEdgePixels = Terrain.MaxTriangleEdgePixels;
	BaseMesh.MergeThresholdFraction = Terrain.MergeThresholdFraction;
	BaseMesh.TargetErrorPixels = Terrain.TargetErrorPixels;
	BaseMesh.FlatAreaRelief = Terrain.FlatAreaRelief;
	BaseMesh.FlatReliefSagCmPerMeterSq = Terrain.FlatReliefSagCmPerMeterSq;
	BaseMesh.FrustumCullMargin = Terrain.FrustumCullMargin;
	BaseMesh.TessFrustumCullMargin = Terrain.TessFrustumCullMargin;

	State.PublishedRenderIdentity = BaseMesh.Identity;
	State.LastObservedRhiEpoch = BaseMesh.Identity.RhiEpoch;
	State.CbtPublishCache.TopologySignature = TopologySignature;
	State.CbtPublishCache.ResourceGeneration =
		State.TileAtlas.IsValid() ? State.TileAtlas->GetResourceGeneration() : 0;
	State.CbtPublishCache.RenderKnobRevision = State.AtlasSchedule.RenderKnobRevision;
	State.AtlasSchedule.ConsumedRenderKnobRevision = State.AtlasSchedule.RenderKnobRevision;
	State.CbtPublishCache.LocalToWorld = BaseMesh.LocalToWorld;
	State.CbtPublishCache.Rings = Rings;
	State.CbtPublishCache.bPublished = true;

	const double PublishStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
	GArcTerrainCBTRendererExtension.PublishCandidate_GameThread(MoveTemp(BaseMesh));
	ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasCbtPublish, PublishStartSeconds);
	ArcTerrainDiag::NoteAtlasPublication();
}

namespace ArcTerrainAtlasRuntimeLocal_e3a1
{
	TSet<uint32> LoggedUnknownPlaneIds;

	FArcTerrainAtlasInFlightKey MakeInFlightKey(const FArcTerrainAtlasPrepareInput& Input)
	{
		FArcTerrainAtlasInFlightKey Key;
		Key.Kind = Input.Kind;
		Key.SectorId = Input.SectorId;
		Key.GroupId = Input.GroupId;
		Key.CellKey = Input.CellKey;
		return Key;
	}

	void EmitPrepareDiagnostics(const FArcTerrainAtlasPrepareResult& Prepared)
	{
		for (const uint32 PlaneId : Prepared.Status.UnknownPlaneIds)
		{
			bool bAlreadyLogged = false;
			LoggedUnknownPlaneIds.Add(PlaneId, &bAlreadyLogged);
			if (!bAlreadyLogged)
			{
				UE_LOGF(LogArcTerrain, Warning,
					"[Weight] Tile (%d,%d) carries plane id 0x%08X, which names no channel of the "
					"terrain's channel set — the plane is dropped from the atlas. Logged once per id.",
					Prepared.Status.PayloadTileKey.X, Prepared.Status.PayloadTileKey.Y, PlaneId);
			}
		}
		if (Prepared.Status.DroppedChannels.Num() > 0)
		{
			UE_LOGF(LogArcTerrain, Warning,
				"[Weight] Cut key (%d,%d): weight planes exceed the %d-slice atlas budget — kept top-%d, "
				"dropped %d (channels %ls).",
				Prepared.Identity.CellKey.X, Prepared.Identity.CellKey.Y,
				ArcTerrainWeights::GArcTerrainWeightMaxSlices, Prepared.Status.KeptTopK,
				Prepared.Status.DroppedChannels.Num(),
				*FString::JoinBy(Prepared.Status.DroppedChannels, TEXT(","),
					[](int32 Channel) { return FString::FromInt(Channel); }));
		}
		if (Prepared.Status.bNoTile)
		{
			ArcTerrainDiag::NoteAtlasHeightMiss(ArcTerrainDiag::EAtlasHeightMiss::NoTile);
		}
		else if (Prepared.Status.bResolutionMismatch)
		{
			ArcTerrainDiag::NoteAtlasHeightMiss(ArcTerrainDiag::EAtlasHeightMiss::CoarseResolutionMismatch);
			ArcTerrainDiag::NoteCoarseResolutionMismatch(Prepared.Identity.SectorId.Key,
				Prepared.Identity.SectorId.Tier, Prepared.Status.ObservedResolution,
				Prepared.Identity.ExpectedResolution);
		}
		else if (Prepared.Status.bCutFailed)
		{
			ArcTerrainDiag::NoteAtlasHeightMiss(ArcTerrainDiag::EAtlasHeightMiss::CutFailed);
			ArcTerrainDiag::NoteAtlasCutFailure(Prepared.Identity.CellKey, Prepared.Status.PayloadTileKey,
				Prepared.Status.PayloadRail, Prepared.Status.ObservedResolution,
				Prepared.Status.PayloadFloats, Prepared.Identity.Grid,
				Prepared.Identity.ExpectedResolution);
		}
	}

	void Redirty(FArcTerrainAtlasSchedule& Schedule, const FArcTerrainAtlasPrepareInput& Identity)
	{
		if (Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCell
			|| Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCoarse)
		{
			Schedule.DirtyHeightSectors.Add(Identity.SectorId);
		}
		else
		{
			Schedule.DirtyWeightGroups.Add(Identity.GroupId);
			if (Identity.Kind == EArcTerrainAtlasPrepareKind::WeightCell)
			{
				Schedule.DirtyAtlasCells.Add(Identity.CellKey);
			}
		}
	}

	void TouchAtlasIdentity(const FArcTerrainAtlasPrepareInput& Identity,
		TSet<FArcTerrainSectorId>& TouchedHeightSectors, TSet<FArcTerrainWeightGroupId>& TouchedWeightGroups)
	{
		if (Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCell
			|| Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCoarse)
		{
			TouchedHeightSectors.Add(Identity.SectorId);
		}
		else if (Identity.Kind == EArcTerrainAtlasPrepareKind::WeightCell
			|| Identity.Kind == EArcTerrainAtlasPrepareKind::WeightCoarse)
		{
			TouchedWeightGroups.Add(Identity.GroupId);
		}
	}

	uint32 LiveAtlasPayloadId(const AArcTerrain& Terrain, const FArcTerrainState& State,
		const FArcTerrainAtlasPrepareInput& Identity)
	{
		const bool bHeight = Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCell
			|| Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCoarse;
		const EArcTerrainRail Rail = bHeight ? EArcTerrainRail::Height : EArcTerrainRail::Weight;
		int32 Tier = 0;
		FArcTerrainTileGrid Grid = Identity.Grid;
		FIntPoint TileKey = FIntPoint::ZeroValue;

		if (Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCell
			|| Identity.Kind == EArcTerrainAtlasPrepareKind::WeightCell)
		{
			if (!Grid.IsValid() && !ArcTerrainTileAccess::ResolveGrid(Terrain, Rail, 0, Grid))
			{
				if (Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCell)
				{
					if (const FArcTerrainCellState* Cell = State.Cells.Find(Identity.CellKey))
					{
						return static_cast<uint32>(Cell->HeightTileSerial);
					}
				}
				return 0;
			}
			if (!Grid.IsValid())
			{
				return 0;
			}
			TileKey = ArcTerrainTileAccess::TileKeyFromCell(Grid, Identity.CellKey);
		}
		else if (Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCoarse)
		{
			Tier = Identity.SectorId.Tier;
			if (!ArcTerrainTileAccess::ResolveGrid(Terrain, Rail, Tier, Grid)
				|| !State.TierLadder.Tiers.IsValidIndex(Tier))
			{
				return 0;
			}
			TileKey = ArcTerrainSubsystemPrivate::CoarseSectorTileKey(Grid, Identity.SectorId.Key,
				State.TierLadder.Tiers[Tier].CellSectorShift);
		}
		else
		{
			Tier = Identity.GroupId.Tier;
			if (!Grid.IsValid() && !ArcTerrainTileAccess::ResolveGrid(Terrain, Rail, Tier, Grid))
			{
				return 0;
			}
			if (!Grid.IsValid())
			{
				return 0;
			}
			int32 SliceShift = 0;
			if (State.WeightLadder.Tiers.IsValidIndex(Tier))
			{
				SliceShift = State.WeightLadder.Tiers[Tier].SliceShift;
			}
			TileKey = ArcTerrainSubsystemPrivate::CoarseWeightGroupTileKey(Grid, Identity.GroupId.Key,
				SliceShift);
		}

		const FArcTerrainTilePayloadPtr Tile =
			ArcTerrainTileAccess::FindLoadedTile(Terrain, Rail, Tier, TileKey);
		return Tile.IsValid() ? Tile->PayloadId : 0;
	}
}

bool UArcTerrainSubsystem::SubmitAtlasPrepare(FArcTerrainState& State, FArcTerrainAtlasPrepareInput&& Input)
{
	FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet();
	if (Coordinator == nullptr)
	{
		return false;
	}

	FArcTerrainRuntimeReservationRequest Request;
	Request.OwnerId = Input.OwnerId;
	Request.TerrainId = Input.TerrainId;
	Request.Lane = EArcTerrainRuntimeLane::Atlas;
	Request.ExactOwnedInputBytes = static_cast<uint64>(Input.ChannelMap.GetAllocatedSize())
		+ sizeof(FArcTerrainAtlasPrepareInput);
	if (Input.Kind == EArcTerrainAtlasPrepareKind::HeightCell
		|| Input.Kind == EArcTerrainAtlasPrepareKind::HeightCoarse)
	{
		Request.PreReservedResultBytes =
			ArcTerrainAtlasPrepare::EstimateHeightStagingBytes(Input.ExpectedResolution);
	}
	else
	{
		Request.PreReservedResultBytes = ArcTerrainAtlasPrepare::EstimateWeightStagingBytes(
			Input.ExpectedResolution, ArcTerrainWeights::GArcTerrainWeightMaxSlices);
	}
	if (Input.Source.Payload.IsValid())
	{
		FArcTerrainRetainedPayload Retain;
		Retain.PayloadId = Input.Source.PayloadId;
		Retain.Bytes = Input.Source.Payload->RetainedBytes();
		Retain.Payload = Input.Source.Payload;
		Request.Payloads.Add(Retain);
	}

	FArcTerrainRuntimeReservationToken Token;
	if (!Coordinator->TryReserveSubmission(Request, Token))
	{
		return false;
	}

	Coordinator->Submit(MoveTemp(Token), UE::Tasks::ETaskPriority::Normal,
		[Input = MoveTemp(Input)](FArcTerrainRuntimeWorkContext& Context) mutable
		{
			TSharedPtr<FArcTerrainAtlasLanePayload, ESPMode::ThreadSafe> LanePayload =
				MakeShared<FArcTerrainAtlasLanePayload, ESPMode::ThreadSafe>();
			LanePayload->Prepare = MakeShared<FArcTerrainAtlasPrepareResult, ESPMode::ThreadSafe>();
			LanePayload->Prepare->Identity = Input;
			*LanePayload->Prepare = ArcTerrainAtlasPrepare::PrepareAtlasWork(Input, Context);
			FArcTerrainRuntimeResult Result;
			if (Context.IsAbandoned())
			{
				Result.Status = EArcTerrainRuntimeWorkStatus::Cancelled;
			}
			else if (LanePayload->Prepare->Status.bBudgetExceeded)
			{
				Result.Status = EArcTerrainRuntimeWorkStatus::BudgetExceeded;
			}
			else
			{
				Result.Status = EArcTerrainRuntimeWorkStatus::Succeeded;
			}
			Result.Payload = LanePayload;
			return Result;
		});
	ArcTerrainDiag::NoteAtlasInFlight();
	return true;
}

bool UArcTerrainSubsystem::SubmitCbtTopology(FArcTerrainState& State, const FArcTerrainCBTTopologyInput& Input)
{
	FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet();
	if (Coordinator == nullptr)
	{
		return false;
	}

	FArcTerrainRuntimeReservationRequest Request;
	Request.OwnerId = GetRuntimeOwnerId();
	Request.TerrainId = State.CollisionTerrainId;
	Request.Lane = EArcTerrainRuntimeLane::Atlas;
	Request.ExactOwnedInputBytes = sizeof(FArcTerrainCBTTopologyInput);
	Request.PreReservedResultBytes = ArcTerrainAtlasPrepare::EstimateCbtTopologyBytes(
		Input.FaceGrid.IsValid() ? Input.FaceGrid.NumFaces() : 0);

	FArcTerrainRuntimeReservationToken Token;
	if (!Coordinator->TryReserveSubmission(Request, Token))
	{
		return false;
	}

	Coordinator->Submit(MoveTemp(Token), UE::Tasks::ETaskPriority::BackgroundNormal,
		[Input](FArcTerrainRuntimeWorkContext& Context)
		{
			TSharedPtr<FArcTerrainAtlasLanePayload, ESPMode::ThreadSafe> LanePayload =
				MakeShared<FArcTerrainAtlasLanePayload, ESPMode::ThreadSafe>();
			LanePayload->Topology = MakeShared<FArcTerrainCBTTopologyResult, ESPMode::ThreadSafe>();
			LanePayload->Topology->TopologySignature = Input.TopologySignature;
			LanePayload->Topology->LayoutGeneration = Input.LayoutGeneration;
			*LanePayload->Topology = ArcTerrainAtlasPrepare::BuildCbtTopology(Input, Context);
			FArcTerrainRuntimeResult Result;
			if (Context.IsAbandoned())
			{
				Result.Status = EArcTerrainRuntimeWorkStatus::Cancelled;
			}
			else
			{
				Result.Status = LanePayload->Topology->Status;
			}
			Result.Payload = LanePayload;
			return Result;
		});
	return true;
}

void UArcTerrainSubsystem::ApplyAtlasRuntimeCompletion(FArcTerrainRuntimeCompletion&& Completion)
{
	using namespace ArcTerrainAtlasRuntimeLocal_e3a1;

	TSharedPtr<FArcTerrainAtlasLanePayload, ESPMode::ThreadSafe> LanePayload =
		StaticCastSharedPtr<FArcTerrainAtlasLanePayload>(Completion.Payload);
	if (!LanePayload.IsValid())
	{
		Completion.Reservation.Release();
		return;
	}

	if (LanePayload->Topology.IsValid())
	{
		const FArcTerrainCBTTopologyResult& Topology = *LanePayload->Topology;
		for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
		{
			FArcTerrainState& State = TerrainPair.Value;
			if (State.CollisionTerrainId != Completion.TerrainId)
			{
				continue;
			}
			FArcTerrainAtlasSchedule& Schedule = State.AtlasSchedule;
			if (Schedule.bCbtTopologyInFlight
				&& Schedule.PendingCbtTopologySignature == Topology.TopologySignature
				&& Schedule.PendingCbtLayoutGeneration == Topology.LayoutGeneration
				&& Schedule.LayoutGeneration == Topology.LayoutGeneration
				&& Completion.Status == EArcTerrainRuntimeWorkStatus::Succeeded
				&& Topology.Status == EArcTerrainRuntimeWorkStatus::Succeeded)
			{
				const double TopologyStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
				State.CbtTopologyCache.Signature = Topology.TopologySignature;
				State.CbtTopologyCache.NumFaces = Topology.NumFaces;
				State.CbtTopologyCache.FaceQuads = Topology.FaceQuads;
				State.CbtTopologyCache.Neighbors = Topology.Neighbors;
				State.CbtTopologyCache.FaceTexelOrigins = Topology.FaceTexelOrigins;
				ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::AtlasCbtTopology,
					TopologyStartSeconds);
				ArcTerrainDiag::NoteAtlasTopologyBuild();
			}
			Schedule.bCbtTopologyInFlight = false;
			break;
		}
		Completion.Reservation.Release();
		return;
	}

	if (!LanePayload->Prepare.IsValid())
	{
		Completion.Reservation.Release();
		return;
	}

	FArcTerrainState* MatchingState = nullptr;
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		if (TerrainPair.Value.CollisionTerrainId == Completion.TerrainId)
		{
			MatchingState = &TerrainPair.Value;
			break;
		}
	}
	if (MatchingState == nullptr)
	{
		Completion.Reservation.Release();
		return;
	}

	if (Completion.Status == EArcTerrainRuntimeWorkStatus::Cancelled)
	{
		const FArcTerrainAtlasPrepareResult& Prepared = *LanePayload->Prepare;
		MatchingState->AtlasSchedule.InFlight.Remove(MakeInFlightKey(Prepared.Identity));
		Redirty(MatchingState->AtlasSchedule, Prepared.Identity);
		Completion.Reservation.Release();
		return;
	}

	FPendingAtlasFill Pending;
	Pending.TerrainId = Completion.TerrainId;
	Pending.Prepared = LanePayload->Prepare;
	Pending.Token = MoveTemp(Completion.Reservation);
	Pending.Status = Completion.Status;
	PendingAtlasFills.Add(MoveTemp(Pending));
}

void UArcTerrainSubsystem::ReleasePendingAtlasFills(uint64 TerrainId)
{
	using namespace ArcTerrainAtlasRuntimeLocal_e3a1;
	TArray<int32> RemoveIndices;
	for (int32 Index = 0; Index < PendingAtlasFills.Num(); ++Index)
	{
		if (TerrainId == 0 || PendingAtlasFills[Index].TerrainId == TerrainId)
		{
			RemoveIndices.Add(Index);
		}
	}
	for (int32 Reverse = RemoveIndices.Num() - 1; Reverse >= 0; --Reverse)
	{
		FPendingAtlasFill& Item = PendingAtlasFills[RemoveIndices[Reverse]];
		if (Item.Prepared.IsValid())
		{
			for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
			{
				if (TerrainPair.Value.CollisionTerrainId == Item.TerrainId)
				{
					TerrainPair.Value.AtlasSchedule.InFlight.Remove(MakeInFlightKey(Item.Prepared->Identity));
					break;
				}
			}
		}
		Item.Token.Release();
		PendingAtlasFills.RemoveAt(RemoveIndices[Reverse], EAllowShrinking::No);
	}
}

bool UArcTerrainSubsystem::FlushPendingAtlasFills(AArcTerrain& Terrain, FArcTerrainState& State)
{
	using namespace ArcTerrainAtlasRuntimeLocal_e3a1;
	using namespace ArcTerrainAtlasScheduleLocal_9c31;
	(void)Terrain;

	TArray<FPendingAtlasFill> Pending;
	TArray<int32> RemoveIndices;
	for (int32 Index = 0; Index < PendingAtlasFills.Num(); ++Index)
	{
		if (PendingAtlasFills[Index].TerrainId == State.CollisionTerrainId)
		{
			RemoveIndices.Add(Index);
		}
	}
	for (int32 Reverse = RemoveIndices.Num() - 1; Reverse >= 0; --Reverse)
	{
		Pending.Add(MoveTemp(PendingAtlasFills[RemoveIndices[Reverse]]));
		PendingAtlasFills.RemoveAt(RemoveIndices[Reverse], EAllowShrinking::No);
	}

	FArcTerrainAtlasSchedule& Schedule = State.AtlasSchedule;
	if (!State.TileAtlas.IsValid())
	{
		for (FPendingAtlasFill& Item : Pending)
		{
			if (Item.Prepared.IsValid())
			{
				Schedule.InFlight.Remove(MakeInFlightKey(Item.Prepared->Identity));
			}
			Item.Token.Release();
		}
		return false;
	}

	FArcTerrainTileAtlas& Atlas = *State.TileAtlas;
	TSet<FArcTerrainSectorId> TouchedHeightSectors;
	TSet<FArcTerrainWeightGroupId> TouchedWeightGroups;
	for (FPendingAtlasFill& Item : Pending)
	{
		if (!Item.Prepared.IsValid())
		{
			Item.Token.Release();
			continue;
		}
		FArcTerrainAtlasPrepareResult& Prepared = *Item.Prepared;
		Schedule.InFlight.Remove(MakeInFlightKey(Prepared.Identity));
		EmitPrepareDiagnostics(Prepared);

		if (Item.Status == EArcTerrainRuntimeWorkStatus::BudgetExceeded
			|| Prepared.Status.bBudgetExceeded)
		{
			Redirty(Schedule, Prepared.Identity);
			Item.Token.Release();
			continue;
		}

		const bool bLayoutOk = Schedule.LayoutGeneration == Prepared.Identity.LayoutGeneration;
		const bool bSliceOk = Prepared.Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCell
			|| Prepared.Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCoarse
			? Atlas.FindHeightSlice(Prepared.Identity.SectorId) == Prepared.Identity.Slice
				&& Atlas.GetHeightSliceGeneration(Prepared.Identity.SectorId)
					== Prepared.Identity.SliceGeneration
			: Atlas.FindWeightSlice(Prepared.Identity.GroupId) == Prepared.Identity.Slice
				&& Atlas.GetWeightSliceGeneration(Prepared.Identity.GroupId)
					== Prepared.Identity.SliceGeneration;
		uint64 CurrentPublication = 0;
		if (Prepared.Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCell)
		{
			if (const FArcTerrainCellState* Cell = State.Cells.Find(Prepared.Identity.CellKey))
			{
				CurrentPublication = Cell->AtlasHeightVersion;
			}
		}
		const bool bPublicationOk = CurrentPublication == Prepared.Identity.CellPublicationVersion;
		const uint32 LivePayloadId = LiveAtlasPayloadId(Terrain, State, Prepared.Identity);
		const bool bPayloadOk = LivePayloadId == Prepared.Identity.ExpectedPayloadId;
		if (!bLayoutOk || !bSliceOk || !bPublicationOk || !bPayloadOk)
		{
			Redirty(Schedule, Prepared.Identity);
			Item.Token.Release();
			continue;
		}

		const bool bHasStaging = !Prepared.Fill.HeightStagingBytes.IsEmpty()
			|| !Prepared.Fill.WeightStagingPlanes.IsEmpty();
		if (!bHasStaging)
		{
			// Disk-absent is valid ready; HasTile/Loaded empty staging is not.
			if (Prepared.Identity.Source.State == ETileSourceState::Loaded
				|| Prepared.Identity.ExpectedPayloadId != 0)
			{
				Redirty(Schedule, Prepared.Identity);
			}
			else if (Prepared.Identity.Source.State != ETileSourceState::KnownUnloaded)
			{
				TouchAtlasIdentity(Prepared.Identity, TouchedHeightSectors, TouchedWeightGroups);
			}
			Item.Token.Release();
			continue;
		}

		if (Prepared.Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCell)
		{
			if (FArcTerrainCellState* Cell = State.Cells.Find(Prepared.Identity.CellKey))
			{
				Cell->AtlasHeightTileSerial = Prepared.Identity.ExpectedPayloadId;
			}
			ArcTerrainDiag::NoteAtlasFill(ArcTerrainDiag::EAtlasFillSource::Tile);
		}
		else
		{
			if (Prepared.Identity.Kind == EArcTerrainAtlasPrepareKind::WeightCell)
			{
				if (FArcTerrainCellState* Cell = State.Cells.Find(Prepared.Identity.CellKey))
				{
					Cell->AtlasWeightTileSerial = Prepared.Identity.ExpectedPayloadId;
				}
			}
			ArcTerrainDiag::NoteAtlasFill(ArcTerrainDiag::EAtlasFillSource::NoHeight);
		}
		ArcTerrainDiag::NoteAtlasReady();

		FArcTerrainAtlasFillJob FillJob;
		FillJob.Layout = Atlas.GetLayout();
		FillJob.Cells.Add(MoveTemp(Prepared.Fill));
		FillJob.ReservationTokens.Add(MoveTemp(Item.Token));
		Atlas.EnqueueFill(MoveTemp(FillJob));
		if (Prepared.Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCell
			|| Prepared.Identity.Kind == EArcTerrainAtlasPrepareKind::HeightCoarse)
		{
			Atlas.SetSectorHeightTileFingerprint(Prepared.Identity.SectorId,
				Prepared.Identity.ExpectedPayloadId);
		}
		else
		{
			Atlas.SetWeightGroupTileFingerprint(Prepared.Identity.GroupId,
				Prepared.Identity.ExpectedPayloadId);
		}
		TouchAtlasIdentity(Prepared.Identity, TouchedHeightSectors, TouchedWeightGroups);
		ArcTerrainDiag::NoteAtlasInFlight();
	}

	bool bAnyRingReady = false;
	for (const FArcTerrainSectorId& SectorId : TouchedHeightSectors)
	{
		if (TryMarkHeightRingReady(Atlas, Schedule, SectorId))
		{
			bAnyRingReady = true;
		}
	}
	for (const FArcTerrainWeightGroupId& GroupId : TouchedWeightGroups)
	{
		if (TryMarkWeightRingReady(Atlas, Schedule, GroupId))
		{
			bAnyRingReady = true;
		}
	}
	return bAnyRingReady;
}
