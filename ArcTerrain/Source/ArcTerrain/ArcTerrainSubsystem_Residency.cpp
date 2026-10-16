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
#include "Render/ArcTerrainCBTRenderer.h"
#include "Render/ArcTerrainCellMath.h"
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

void FArcTerrainPipelineTelemetry::IncrementAuthorityProduced(EArcTerrainTileAuthorityReason Reason)
{
	const int32 Index = static_cast<int32>(Reason);
	if (Index >= 0 && Index < static_cast<int32>(EArcTerrainTileAuthorityReason::Count))
	{
		++Interval.AuthorityProduced[Index];
		++Lifetime.AuthorityProduced[Index];
	}
}

void FArcTerrainPipelineTelemetry::IncrementAtlasAuthorityConsumed(EArcTerrainTileAuthorityReason Reason)
{
	const int32 Index = static_cast<int32>(Reason);
	if (Index >= 0 && Index < static_cast<int32>(EArcTerrainTileAuthorityReason::Count))
	{
		++Interval.AtlasAuthorityConsumed[Index];
		++Lifetime.AtlasAuthorityConsumed[Index];
	}
}

void FArcTerrainPipelineTelemetry::IncrementAtlasEffect(EArcTerrainPipelineAtlasEffect Effect)
{
	const int32 Index = static_cast<int32>(Effect);
	if (Index >= 0 && Index < static_cast<int32>(EArcTerrainPipelineAtlasEffect::Count))
	{
		++Interval.AtlasEffects[Index];
		++Lifetime.AtlasEffects[Index];
	}
}

void FArcTerrainPipelineTelemetry::IncrementAtlasIgnored(EArcTerrainPipelineAtlasIgnore Reason)
{
	const int32 Index = static_cast<int32>(Reason);
	if (Index >= 0 && Index < static_cast<int32>(EArcTerrainPipelineAtlasIgnore::Count))
	{
		++Interval.AtlasIgnored[Index];
		++Lifetime.AtlasIgnored[Index];
	}
}

void FArcTerrainPipelineTelemetry::IncrementCollisionEnqueue(EArcTerrainPipelineCollisionEnqueue Outcome)
{
	const int32 Index = static_cast<int32>(Outcome);
	if (Index >= 0 && Index < static_cast<int32>(EArcTerrainPipelineCollisionEnqueue::Count))
	{
		++Interval.CollisionEnqueue[Index];
		++Lifetime.CollisionEnqueue[Index];
	}
}

void PublishLocalTileAuthorityEvent(FArcTerrainState& State, const FArcTerrainTileAuthorityEvent& Event)
{
	State.PipelineTelemetry.IncrementAuthorityProduced(Event.Reason);
	++State.LocalTileAuthorityRevision;
	State.PendingTileAuthorityEvents.Add(Event);
	State.PendingAtlasAuthorityEvents.Add(Event);
	State.PendingLightingAuthorityEvents.Add(Event);
	State.PendingFarAuthorityEvents.Add(Event);
	if (Event.Reason == EArcTerrainTileAuthorityReason::TileArrived
		|| Event.Reason == EArcTerrainTileAuthorityReason::TileReleased
		|| Event.Reason == EArcTerrainTileAuthorityReason::PendingHeightChanged
		|| Event.Reason == EArcTerrainTileAuthorityReason::PendingHeightReset
		|| Event.Reason == EArcTerrainTileAuthorityReason::LayoutChanged)
	{
		State.PendingCollisionAuthorityEvents.Add(Event);
	}
}

namespace ArcTerrainResidencyIndex_c4e8
{
	FIntPoint SectorKeyFromCell(FIntPoint Cell, int32 Shift)
	{
		const int64 Span = static_cast<int64>(1) << Shift;
		return FIntPoint(
			static_cast<int32>(ArcTerrain::FloorDiv(static_cast<int64>(Cell.X), Span)),
			static_cast<int32>(ArcTerrain::FloorDiv(static_cast<int64>(Cell.Y), Span)));
	}

	void RecomputeWeightMax(FArcTerrainWeightResidencyRecord& Record)
	{
		int32 NewMax = 0;
		for (const TPair<int32, int32>& Pair : Record.SourceDimCounts)
		{
			if (Pair.Value > 0)
			{
				NewMax = FMath::Max(NewMax, Pair.Key);
			}
		}
		Record.MaxSourceDim = NewMax;
	}

	void AddWeightDim(FArcTerrainState& State, int32 Tier, FIntPoint GroupKey, int32 SourceDim)
	{
		if (SourceDim <= 0)
		{
			return;
		}
		FArcTerrainWeightResidencyRecord& Record =
			State.WeightResidencyIndex.FindOrAdd(FArcTerrainWeightResidencyKey{ Tier, GroupKey });
		int32& Count = Record.SourceDimCounts.FindOrAdd(SourceDim);
		++Count;
		if (SourceDim > Record.MaxSourceDim)
		{
			Record.MaxSourceDim = SourceDim;
		}
	}

	void RemoveWeightDim(FArcTerrainState& State, int32 Tier, FIntPoint GroupKey, int32 SourceDim)
	{
		if (SourceDim <= 0)
		{
			return;
		}
		const FArcTerrainWeightResidencyKey Key{ Tier, GroupKey };
		FArcTerrainWeightResidencyRecord* Record = State.WeightResidencyIndex.Find(Key);
		if (Record == nullptr)
		{
			return;
		}
		int32* Count = Record->SourceDimCounts.Find(SourceDim);
		if (Count == nullptr)
		{
			return;
		}
		--(*Count);
		if (*Count <= 0)
		{
			Record->SourceDimCounts.Remove(SourceDim);
		}
		if (Record->SourceDimCounts.IsEmpty())
		{
			State.WeightResidencyIndex.Remove(Key);
			return;
		}
		if (SourceDim == Record->MaxSourceDim)
		{
			RecomputeWeightMax(*Record);
		}
	}

	void CollectWeightGroupsFromSpans(TConstArrayView<FArcTerrainResidencyCoverageSpan> Spans, int32 Shift,
		TSet<FIntPoint>& OutGroups)
	{
		for (const FArcTerrainResidencyCoverageSpan& Span : Spans)
		{
			const FIntPoint MinGroup = SectorKeyFromCell(Span.MinCell, Shift);
			const FIntPoint MaxGroup = SectorKeyFromCell(Span.MaxCell, Shift);
			for (int32 GroupY = MinGroup.Y; GroupY <= MaxGroup.Y; ++GroupY)
			{
				for (int32 GroupX = MinGroup.X; GroupX <= MaxGroup.X; ++GroupX)
				{
					OutGroups.Add(FIntPoint(GroupX, GroupY));
				}
			}
		}
	}

	void RemoveStampWeightCoverage(FArcTerrainState& State, const TArray<FArcTerrainResidencyCoverageSpan>& Spans,
		int32 SourceDim)
	{
		for (int32 Tier = 0; Tier < State.NumIndexedWeightTiers; ++Tier)
		{
			TSet<FIntPoint> Groups;
			CollectWeightGroupsFromSpans(Spans, State.IndexedWeightShifts[Tier], Groups);
			for (const FIntPoint& GroupKey : Groups)
			{
				RemoveWeightDim(State, Tier, GroupKey, SourceDim);
			}
		}
	}

	void AddStampWeightCoverage(FArcTerrainState& State, const TArray<FArcTerrainResidencyCoverageSpan>& Spans,
		int32 SourceDim)
	{
		for (int32 Tier = 0; Tier < State.NumIndexedWeightTiers; ++Tier)
		{
			TSet<FIntPoint> Groups;
			CollectWeightGroupsFromSpans(Spans, State.IndexedWeightShifts[Tier], Groups);
			for (const FIntPoint& GroupKey : Groups)
			{
				AddWeightDim(State, Tier, GroupKey, SourceDim);
			}
		}
	}

	void CaptureIndexShifts(const AArcTerrain& Terrain, const FArcTerrainState& State, int32& OutSectorShift,
		int32 OutWeightShifts[ArcTerrain::MaxTiers], int32& OutNumWeightTiers)
	{
		OutSectorShift = State.ResidencySelectionCache.SectorShift;
		if (OutSectorShift == INDEX_NONE)
		{
			OutSectorShift = Terrain.SectorShift;
		}
		OutNumWeightTiers = 0;
		if (!State.WeightLadder.Tiers.IsEmpty())
		{
			for (int32 Tier = 0; Tier < State.WeightLadder.Tiers.Num() && Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				OutWeightShifts[Tier] = State.WeightLadder.Tiers[Tier].SliceShift;
				++OutNumWeightTiers;
			}
			return;
		}
		OutWeightShifts[0] = Terrain.WeightSliceShift;
		OutNumWeightTiers = 1;
	}

	void RemoveStampCoverage(FArcTerrainState& State, FArcTerrainStampEntry& Entry)
	{
		RemoveStampWeightCoverage(State, Entry.ResidencyCoverage, Entry.CachedMaxWeightSourceDim);
		for (const FArcTerrainResidencyCoverageSpan& Span : Entry.ResidencyCoverage)
		{
			FArcTerrainSectorContentRecord* Record = State.SectorContentIndex.Find(Span.SectorKey);
			if (Record == nullptr)
			{
				continue;
			}
			const int32 SpanIndex = Record->Spans.IndexOfByPredicate(
				[&Span](const FArcTerrainResidencyCoverageSpan& Existing)
				{
					return Existing.SectorKey == Span.SectorKey
						&& Existing.MinCell == Span.MinCell
						&& Existing.MaxCell == Span.MaxCell;
				});
			if (SpanIndex != INDEX_NONE)
			{
				Record->Spans.RemoveAtSwap(SpanIndex, EAllowShrinking::No);
			}
			--Record->RefCount;
			if (Record->RefCount <= 0)
			{
				State.SectorContentIndex.Remove(Span.SectorKey);
			}
		}
		Entry.ResidencyCoverage.Reset();
		++State.ContentIndexRevision;
	}

	void AddStampCoverage(const AArcTerrain& Terrain, FArcTerrainState& State, FArcTerrainStampEntry& Entry)
	{
		const UArcTerrainStampComponent* Stamp = Entry.Stamp.Get();
		if (Stamp == nullptr || Stamp->bIsDisabled)
		{
			return;
		}
		const FBox Bounds = Entry.Bounds.IsValid ? Entry.Bounds : Stamp->ComputeBounds();
		FIntPoint MinCell = FIntPoint::ZeroValue;
		FIntPoint MaxCell = FIntPoint::ZeroValue;
		if (!ArcTerrain::CellRangeIntersectingLocalBounds(Bounds, Terrain.Config, MinCell, MaxCell))
		{
			return;
		}

		int32 SectorShift = State.IndexedSectorShift;
		if (SectorShift == INDEX_NONE)
		{
			CaptureIndexShifts(Terrain, State, State.IndexedSectorShift, State.IndexedWeightShifts,
				State.NumIndexedWeightTiers);
			SectorShift = State.IndexedSectorShift;
		}
		if (SectorShift < 0)
		{
			return;
		}

		const FIntPoint MinSector = SectorKeyFromCell(MinCell, SectorShift);
		const FIntPoint MaxSector = SectorKeyFromCell(MaxCell, SectorShift);
		const int32 SectorCellSpan = 1 << SectorShift;
		for (int32 SectorY = MinSector.Y; SectorY <= MaxSector.Y; ++SectorY)
		{
			for (int32 SectorX = MinSector.X; SectorX <= MaxSector.X; ++SectorX)
			{
				const FIntPoint SectorKey(SectorX, SectorY);
				const int32 SectorMinCellX = SectorX * SectorCellSpan;
				const int32 SectorMinCellY = SectorY * SectorCellSpan;
				const FIntPoint ClipMin(
					FMath::Max(MinCell.X, SectorMinCellX),
					FMath::Max(MinCell.Y, SectorMinCellY));
				const FIntPoint ClipMax(
					FMath::Min(MaxCell.X, SectorMinCellX + SectorCellSpan - 1),
					FMath::Min(MaxCell.Y, SectorMinCellY + SectorCellSpan - 1));
				if (ClipMin.X > ClipMax.X || ClipMin.Y > ClipMax.Y)
				{
					continue;
				}
				FArcTerrainResidencyCoverageSpan Span;
				Span.SectorKey = SectorKey;
				Span.MinCell = ClipMin;
				Span.MaxCell = ClipMax;
				Entry.ResidencyCoverage.Add(Span);
				FArcTerrainSectorContentRecord& Record = State.SectorContentIndex.FindOrAdd(SectorKey);
				Record.Spans.Add(Span);
				++Record.RefCount;
			}
		}
		AddStampWeightCoverage(State, Entry.ResidencyCoverage, Entry.CachedMaxWeightSourceDim);
		++State.ContentIndexRevision;
	}

	void RebuildIndexes(const AArcTerrain& Terrain, FArcTerrainState& State)
	{
		State.SectorContentIndex.Reset();
		State.WeightResidencyIndex.Reset();
		CaptureIndexShifts(Terrain, State, State.IndexedSectorShift, State.IndexedWeightShifts,
			State.NumIndexedWeightTiers);
		for (FArcTerrainStampEntry& Entry : State.Stamps)
		{
			Entry.ResidencyCoverage.Reset();
			AddStampCoverage(Terrain, State, Entry);
		}
		++State.ContentIndexRevision;
	}

	void EnqueueCompositeReady(FArcTerrainState& State, FArcTerrainCellKey Key)
	{
		State.CompositeReadyCells.Add(Key);
	}

	void NotePendingHeightChanged(FArcTerrainState& State, FIntPoint TileKey)
	{
		FArcTerrainTileAuthorityEvent Event;
		Event.Rail = EArcTerrainRail::Height;
		Event.Tier = 0;
		Event.TileKey = TileKey;
		Event.Reason = EArcTerrainTileAuthorityReason::PendingHeightChanged;
		PublishLocalTileAuthorityEvent(State, Event);
	}

	void NotePendingHeightReset(FArcTerrainState& State)
	{
		FArcTerrainTileAuthorityEvent Event;
		Event.Rail = EArcTerrainRail::Height;
		Event.Tier = 0;
		Event.Reason = EArcTerrainTileAuthorityReason::PendingHeightReset;
		PublishLocalTileAuthorityEvent(State, Event);
	}
}

FArcTerrainCellState& FArcTerrainState::FindOrAddCell(FArcTerrainCellKey Key)
{
	FArcTerrainCellState* Existing = Cells.Find(Key);
	if (Existing != nullptr)
	{
		return *Existing;
	}
	FArcTerrainCellState& Cell = Cells.Add(Key);
	const int32 HotIndex = HotCells.Num();
	Cell.HotIndex = HotIndex;
	HotCells.AddDefaulted_GetRef().Key = Key;
	if (SectorCellsShift != INDEX_NONE)
	{
		SectorCells.FindOrAdd(ArcTerrainAtlas::GroupKeyFromCell(Key, SectorCellsShift)).Add(HotIndex);
	}
	return Cell;
}

void FArcTerrainState::RemoveCell(FArcTerrainCellKey Key)
{
	const FArcTerrainCellState* Cell = Cells.Find(Key);
	if (Cell == nullptr)
	{
		return;
	}
	const int32 Index = Cell->HotIndex;
	const int32 LastIndex = HotCells.Num() - 1;

	// The sector list is patched around the RemoveAtSwap: the removed cell's entry goes, and the entry
	// naming the record that moves down into its slot is re-pointed. Missing either one hands a later
	// phase an index that names a different cell.
	if (SectorCellsShift != INDEX_NONE)
	{
		const FIntPoint SectorKey = ArcTerrainAtlas::GroupKeyFromCell(Key, SectorCellsShift);
		TArray<int32>* SectorIndices = SectorCells.Find(SectorKey);
		if (SectorIndices != nullptr)
		{
			SectorIndices->RemoveSingleSwap(Index, EAllowShrinking::No);
			if (SectorIndices->IsEmpty())
			{
				SectorCells.Remove(SectorKey);
			}
		}
	}

	Cells.Remove(Key);
	HotCells.RemoveAtSwap(Index, EAllowShrinking::No);
	if (HotCells.IsValidIndex(Index))
	{
		// The record swapped down into Index belongs to another cell, whose back-links must follow it.
		const FArcTerrainCellKey MovedKey = HotCells[Index].Key;
		Cells.FindChecked(MovedKey).HotIndex = Index;
		if (SectorCellsShift != INDEX_NONE)
		{
			TArray<int32>* MovedIndices =
				SectorCells.Find(ArcTerrainAtlas::GroupKeyFromCell(MovedKey, SectorCellsShift));
			if (MovedIndices != nullptr)
			{
				const int32 Slot = MovedIndices->Find(LastIndex);
				if (Slot != INDEX_NONE)
				{
					(*MovedIndices)[Slot] = Index;
				}
			}
		}
	}
	PinnedCells.Remove(Key);
}

void FArcTerrainState::EmptyCells()
{
	Cells.Empty();
	HotCells.Empty();
	PinnedCells.Empty();
	SectorCells.Reset();
	DirtyQueue.Reset();
	ResidentCells.Reset();
	ResidentWeightGroups.Reset();
	PendingRefillCells.Reset();
	DeferredResidencyEvictions.Reset();
	ReadyResidencyEvictions.Reset();
	PendingResidencyAdmissions.Reset();
	CompositeReadyCells.Reset();
	CompositeRetryCells.Reset();
	PendingAtlasAuthorityEvents.Reset();
	AtlasSchedule.PendingHeightAdmissions.Reset();
	AtlasSchedule.DirtyHeightSectors.Reset();
	AtlasSchedule.RefusedHeightSectors.Reset();
	AtlasSchedule.WokenHeightAdmissions.Reset();
	AtlasSchedule.PendingWeightAdmissions.Reset();
	AtlasSchedule.DirtyWeightGroups.Reset();
	AtlasSchedule.RefusedWeightGroups.Reset();
	AtlasSchedule.WokenWeightAdmissions.Reset();
	AtlasSchedule.DirtyAtlasCells.Reset();
	++AtlasSchedule.LayoutGeneration;
	NextPinExpiry = 0.0;
	NextCompositeRetry = 0.0;
	bPinMembershipDirty = false;
}

#if WITH_EDITOR
TSet<FIntPoint>& FArcTerrainState::PendingTiles(EArcTerrainRail Rail, int32 Tier)
{
	check(ArcTerrainTileAccess::IsValidTier(Tier));

	switch (Rail)
	{
	case EArcTerrainRail::Weight:
		return PendingWeightTiles[Tier];
	case EArcTerrainRail::Data:
		return PendingDataTiles;
	default:
		return PendingHeightTiles[Tier];
	}
}
#endif

void FArcTerrainState::EnqueueCell(FArcTerrainCellKey Key)
{
	const FArcTerrainCellState* Cell = Cells.Find(Key);
	if (Cell == nullptr)
	{
		return; // the queue is a work list, never a creation rail
	}
	FArcTerrainCellHot& HotCell = Hot(*Cell);
	if (HotCell.bQueued == 0)
	{
		HotCell.bQueued = 1;
		DirtyQueue.Add(Key);
	}
}

void UArcTerrainSubsystem::DrainCellDirtyQueues()
{
	for (TSet<TWeakObjectPtr<AArcTerrain>>::TIterator It(TerrainsWithDirtyCells); It; ++It)
	{
		AArcTerrain* Terrain = It->Get();
		FArcTerrainState* State = Terrain != nullptr ? Terrains.Find(Terrain) : nullptr;
		if (State == nullptr)
		{
			It.RemoveCurrent();
			continue;
		}
		for (const FArcTerrainCellKey& QueuedKey : State->DirtyQueue)
		{
			const FArcTerrainCellState* QueuedCell = State->Cells.Find(QueuedKey);
			if (QueuedCell == nullptr)
			{
				continue;
			}
			State->Hot(*QueuedCell).bQueued = 0;
			if (State->DeferredResidencyEvictions.Remove(QueuedKey) > 0)
			{
				State->ReadyResidencyEvictions.Add(QueuedKey);
			}
		}
		State->DirtyQueue.Reset();
		It.RemoveCurrent();
	}
}

// FILE-SCOPED and unique (CLAUDE.md C++ Code Style) — nothing here may gain external linkage.
namespace ArcTerrainCellCreationLocal_71a3
{
	// "Is this ground already drawn from bytes" for a run of cells, memoised on the tile key: a tile spans
	// many cells (sixteen at the defaults) and FindLoadedTile takes the tile store's lock on every call,
	// while this answers per cell on two of the residency phase's walks.
	struct FHeightTileProbe
	{
		FHeightTileProbe(const AArcTerrain& InTerrain, const FArcTerrainTileGrid& InGrid)
			: Terrain(&InTerrain)
			, Grid(InGrid)
			, bGridValid(InGrid.IsValid())
		{
		}

#if WITH_EDITOR
		void SetRebakeQueue(const TSet<FIntPoint>& InPendingTiles)
		{
			PendingTiles = &InPendingTiles;
		}
#endif

		/** True when the loaded tile IS the published answer for this cell: bytes in memory that no edit has
		 *  queued for a rebake. A tile in the bake queue holds PRE-EDIT ground — it must not take the cell's
		 *  place, or a sector re-admitted between the edit and the bake landing (the slice clear zeroes
		 *  AtlasHeightVersion, the fill then takes the stale tile) drops the edit off the screen until the
		 *  bake lands. */
		bool IsTileAuthoritativeForCell(FArcTerrainCellKey CellKey)
		{
			if (!bGridValid)
			{
				return false;
			}
			const FIntPoint TileKey = ArcTerrainTileAccess::TileKeyFromCell(Grid, CellKey);
			if (!bHasMemo || TileKey != MemoTileKey)
			{
				MemoTileKey = TileKey;
				bHasMemo = true;
				bMemoAuthoritative = ArcTerrainTileAccess::FindLoadedTile(*Terrain, EArcTerrainRail::Height,
					0, TileKey).IsValid();
#if WITH_EDITOR
				bMemoAuthoritative = bMemoAuthoritative
					&& (PendingTiles == nullptr || !PendingTiles->Contains(TileKey));
#endif
			}
			return bMemoAuthoritative;
		}

	private:
		const AArcTerrain* Terrain = nullptr;
		FArcTerrainTileGrid Grid;
#if WITH_EDITOR
		const TSet<FIntPoint>* PendingTiles = nullptr;
#endif
		FIntPoint MemoTileKey = FIntPoint::ZeroValue;
		bool bGridValid = false;
		bool bHasMemo = false;
		bool bMemoAuthoritative = false;
	};
}

void UArcTerrainSubsystem::TickEditPins(double Now)
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	// ---- Phase 0 (editor worlds): while a stamp component (or its owner actor) is SELECTED, keep
	// its whole footprint edit-pinned on a rolling window — an artist working a stamp bigger than
	// the preview ring sees every affected cell for as long as they hold the selection. Cheap poll:
	// per registered stamp, two selection-set membership checks; runs BEFORE phase 1 so pins take
	// effect the same tick.
	if (GEditor != nullptr && World != nullptr && World->WorldType == EWorldType::Editor)
	{
		USelection* SelectedComponents = GEditor->GetSelectedComponents();
		USelection* SelectedActors = GEditor->GetSelectedActors();
		for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
		{
			AArcTerrain* Terrain = TerrainPair.Key.Get();
			if (Terrain == nullptr)
			{
				continue; // dead-terrain reclaim is phase 1's job
			}
			const double EditPinSeconds = static_cast<double>(Terrain->EditPinSeconds);
			if (EditPinSeconds <= 0.0)
			{
				continue;
			}
			const double SelectionPinnedUntil = Now + EditPinSeconds;
			for (const FArcTerrainStampEntry& Entry : TerrainPair.Value.Stamps)
			{
				UArcTerrainStampComponent* Stamp = Entry.Stamp.Get();
				if (Stamp == nullptr)
				{
					continue;
				}
				const AActor* Owner = Stamp->GetOwner();
				const bool bStampSelected =
					(SelectedComponents != nullptr && SelectedComponents->IsSelected(Stamp))
					|| (Owner != nullptr && SelectedActors != nullptr && SelectedActors->IsSelected(Owner));
				if (bStampSelected)
				{
					PinCellsInBounds(TerrainPair.Value, Terrain->Config, Stamp->ComputeBounds(),
						SelectionPinnedUntil);
				}
			}
		}
	}
#endif
}

namespace ArcTerrainResidencyReactive_a91c
{
	bool WindowContains(const ArcTerrainSubsystemPrivate::FSectorWindow& Window, FIntPoint Key)
	{
		return Key.X >= Window.MinX && Key.X <= Window.MaxX && Key.Y >= Window.MinY && Key.Y <= Window.MaxY;
	}

	bool InAnyInvokerWindow(TConstArrayView<FArcTerrainCachedInvokerWindow> Invokers, FIntPoint Key, bool bOuter)
	{
		for (const FArcTerrainCachedInvokerWindow& Invoker : Invokers)
		{
			if (WindowContains(bOuter ? Invoker.OuterWindow : Invoker.InnerWindow, Key))
			{
				return true;
			}
		}
		return false;
	}

	FIntPoint IntegerCentreOf(const FVector& WorldInvoker, int32 SectorShift, double CellSize,
		const FTransform& TerrainTransform)
	{
		const double SectorSize = CellSize * static_cast<double>(static_cast<int64>(1) << SectorShift);
		if (SectorSize <= 0.0)
		{
			return FIntPoint::ZeroValue;
		}
		const FVector Local = TerrainTransform.InverseTransformPosition(WorldInvoker);
		return FIntPoint(
			FMath::FloorToInt32(Local.X / SectorSize),
			FMath::FloorToInt32(Local.Y / SectorSize));
	}

	bool CentresMatch(TConstArrayView<FIntPoint> A, TConstArrayView<FIntPoint> B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index] != B[Index])
			{
				return false;
			}
		}
		return true;
	}

	int32 Chebyshev(FIntPoint A, FIntPoint B)
	{
		return FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y));
	}

	void AppendWindowKeys(const ArcTerrainSubsystemPrivate::FSectorWindow& Window, TSet<FIntPoint>& OutKeys)
	{
		if (Window.MaxX < Window.MinX || Window.MaxY < Window.MinY)
		{
			return;
		}
		for (int32 KeyY = Window.MinY; KeyY <= Window.MaxY; ++KeyY)
		{
			for (int32 KeyX = Window.MinX; KeyX <= Window.MaxX; ++KeyX)
			{
				OutKeys.Add(FIntPoint(KeyX, KeyY));
			}
		}
	}

	void DiffWindows(const ArcTerrainSubsystemPrivate::FSectorWindow& OldWindow,
		const ArcTerrainSubsystemPrivate::FSectorWindow& NewWindow, TArray<FIntPoint>& OutEnter,
		TArray<FIntPoint>& OutLeave)
	{
		TSet<FIntPoint> OldKeys;
		TSet<FIntPoint> NewKeys;
		AppendWindowKeys(OldWindow, OldKeys);
		AppendWindowKeys(NewWindow, NewKeys);
		for (const FIntPoint& Key : NewKeys)
		{
			if (!OldKeys.Contains(Key))
			{
				OutEnter.Add(Key);
			}
		}
		for (const FIntPoint& Key : OldKeys)
		{
			if (!NewKeys.Contains(Key))
			{
				OutLeave.Add(Key);
			}
		}
	}

	double SpanDistSq(const FArcTerrainResidencyCoverageSpan& Span, double CellSize,
		const FTransform& TerrainTransform, TConstArrayView<FVector> Invokers)
	{
		return ArcTerrainSubsystemPrivate::RectInvokerDistSq(
			static_cast<double>(Span.MinCell.X) * CellSize,
			static_cast<double>(Span.MinCell.Y) * CellSize,
			static_cast<double>(Span.MaxCell.X + 1) * CellSize,
			static_cast<double>(Span.MaxCell.Y + 1) * CellSize,
			TerrainTransform, Invokers);
	}

	double SectorContentDistSq(const FArcTerrainState& State, FIntPoint SectorKey, double CellSize,
		const FTransform& TerrainTransform, TConstArrayView<FVector> Invokers)
	{
		const FArcTerrainSectorContentRecord* Record = State.SectorContentIndex.Find(SectorKey);
		if (Record == nullptr)
		{
			return TNumericLimits<double>::Max();
		}
		double Best = TNumericLimits<double>::Max();
		for (const FArcTerrainResidencyCoverageSpan& Span : Record->Spans)
		{
			Best = FMath::Min(Best, SpanDistSq(Span, CellSize, TerrainTransform, Invokers));
		}
		return Best;
	}

	bool EvaluateTier0Sector(const AArcTerrain& Terrain, FArcTerrainState& State, FIntPoint SectorKey,
		int32 SectorShift, const FArcTerrainTileGrid& BaseHeightGrid, double CellSize,
		const FTransform& TerrainTransform, TConstArrayView<FVector> Invokers, double& OutDistSq)
	{
		bool bHasAuthority = State.SectorContentIndex.Contains(SectorKey);
		OutDistSq = SectorContentDistSq(State, SectorKey, CellSize, TerrainTransform, Invokers);
		const TArray<int32>* SectorCellIndices = State.SectorCells.Find(SectorKey);
		if (SectorCellIndices != nullptr)
		{
			bHasAuthority = true;
			OutDistSq = FMath::Min(OutDistSq, ArcTerrainSubsystemPrivate::UpdateCellDistances(
				State.HotCells, *SectorCellIndices, CellSize, TerrainTransform, Invokers));
		}
		if (!BaseHeightGrid.IsValid() || SectorShift < 0 || SectorShift > 31)
		{
			return bHasAuthority;
		}

		const int64 SectorCellSpan = static_cast<int64>(1) << SectorShift;
		const int64 SectorMinCellX = static_cast<int64>(SectorKey.X) * SectorCellSpan;
		const int64 SectorMinCellY = static_cast<int64>(SectorKey.Y) * SectorCellSpan;
		const int64 SectorMaxCellX = SectorMinCellX + SectorCellSpan - 1;
		const int64 SectorMaxCellY = SectorMinCellY + SectorCellSpan - 1;
		if (SectorMinCellX < static_cast<int64>(MIN_int32)
			|| SectorMinCellY < static_cast<int64>(MIN_int32)
			|| SectorMaxCellX > static_cast<int64>(MAX_int32)
			|| SectorMaxCellY > static_cast<int64>(MAX_int32))
		{
			return bHasAuthority;
		}

		const FIntPoint MinTileKey = ArcTerrainTileAccess::TileKeyFromCell(BaseHeightGrid,
			FIntPoint(static_cast<int32>(SectorMinCellX), static_cast<int32>(SectorMinCellY)));
		const FIntPoint MaxTileKey = ArcTerrainTileAccess::TileKeyFromCell(BaseHeightGrid,
			FIntPoint(static_cast<int32>(SectorMaxCellX), static_cast<int32>(SectorMaxCellY)));
		const int64 TileCellSpan = static_cast<int64>(BaseHeightGrid.CellsPerTileEdge);
		for (int64 TileY = static_cast<int64>(MinTileKey.Y); TileY <= static_cast<int64>(MaxTileKey.Y); ++TileY)
		{
			for (int64 TileX = static_cast<int64>(MinTileKey.X); TileX <= static_cast<int64>(MaxTileKey.X); ++TileX)
			{
				const FIntPoint TileKey(static_cast<int32>(TileX), static_cast<int32>(TileY));
				if (!ArcTerrainTileAccess::HasTile(Terrain, EArcTerrainRail::Height, 0, TileKey, &BaseHeightGrid))
				{
					continue;
				}
				const int64 TileMinCellX = TileX * TileCellSpan;
				const int64 TileMinCellY = TileY * TileCellSpan;
				if (TileMinCellX > TNumericLimits<int64>::Max() - (TileCellSpan - 1)
					|| TileMinCellY > TNumericLimits<int64>::Max() - (TileCellSpan - 1))
				{
					continue;
				}
				const int64 TileMaxCellX = TileMinCellX + TileCellSpan - 1;
				const int64 TileMaxCellY = TileMinCellY + TileCellSpan - 1;
				const int64 ClipMinCellX = FMath::Max(TileMinCellX, SectorMinCellX);
				const int64 ClipMinCellY = FMath::Max(TileMinCellY, SectorMinCellY);
				const int64 ClipMaxCellX = FMath::Min(TileMaxCellX, SectorMaxCellX);
				const int64 ClipMaxCellY = FMath::Min(TileMaxCellY, SectorMaxCellY);
				if (ClipMinCellX > ClipMaxCellX || ClipMinCellY > ClipMaxCellY)
				{
					continue;
				}
				bHasAuthority = true;
				OutDistSq = FMath::Min(OutDistSq, ArcTerrainSubsystemPrivate::RectInvokerDistSq(
					static_cast<double>(ClipMinCellX) * CellSize,
					static_cast<double>(ClipMinCellY) * CellSize,
					static_cast<double>(ClipMaxCellX + 1) * CellSize,
					static_cast<double>(ClipMaxCellY + 1) * CellSize,
					TerrainTransform, Invokers));
			}
		}
		return bHasAuthority;
	}

	ArcTerrainSubsystemPrivate::FWantedSector& FindOrAddCachedWanted(
		FArcTerrainState& State, FArcTerrainSectorId Id)
	{
		return ArcTerrainSubsystemPrivate::FindOrAddWantedSector(
			State.CachedWantedSectors, State.CachedWantedSectorIndex, Id);
	}

	void RemoveCachedWanted(FArcTerrainState& State, FArcTerrainSectorId Id)
	{
		const int32* Existing = State.CachedWantedSectorIndex.Find(Id);
		if (Existing == nullptr)
		{
			return;
		}
		const int32 Index = *Existing;
		State.CachedWantedSectors.RemoveAtSwap(Index, EAllowShrinking::No);
		State.CachedWantedSectorIndex.Remove(Id);
		if (State.CachedWantedSectors.IsValidIndex(Index))
		{
			State.CachedWantedSectorIndex.Add(State.CachedWantedSectors[Index].Id, Index);
		}
	}

	ArcTerrainSubsystemPrivate::FWantedWeightGroup& FindOrAddCachedWantedWeight(
		FArcTerrainState& State, FArcTerrainWeightGroupId Id)
	{
		const int32* Existing = State.CachedWantedWeightIndex.Find(Id);
		if (Existing != nullptr)
		{
			return State.CachedWantedWeightGroups[*Existing];
		}
		ArcTerrainSubsystemPrivate::FWantedWeightGroup& Group =
			State.CachedWantedWeightGroups[State.CachedWantedWeightIndex.Add(Id,
				State.CachedWantedWeightGroups.AddDefaulted())];
		Group.Id = Id;
		return Group;
	}

	void RemoveCachedWantedWeight(FArcTerrainState& State, FArcTerrainWeightGroupId Id)
	{
		const int32* Existing = State.CachedWantedWeightIndex.Find(Id);
		if (Existing == nullptr)
		{
			return;
		}
		const int32 Index = *Existing;
		State.CachedWantedWeightGroups.RemoveAtSwap(Index, EAllowShrinking::No);
		State.CachedWantedWeightIndex.Remove(Id);
		if (State.CachedWantedWeightGroups.IsValidIndex(Index))
		{
			State.CachedWantedWeightIndex.Add(State.CachedWantedWeightGroups[Index].Id, Index);
		}
	}

	double WeightGroupContentDistSq(const FArcTerrainState& State, FIntPoint GroupKey, int32 GroupShift,
		double CellSize, const FTransform& TerrainTransform, TConstArrayView<FVector> Invokers)
	{
		const int32 GroupSpan = 1 << GroupShift;
		const FIntPoint GroupMinCell(GroupKey.X * GroupSpan, GroupKey.Y * GroupSpan);
		const FIntPoint GroupMaxCell(GroupMinCell.X + GroupSpan - 1, GroupMinCell.Y + GroupSpan - 1);
		const int32 SectorShift = State.IndexedSectorShift;
		if (SectorShift < 0)
		{
			return TNumericLimits<double>::Max();
		}
		const FIntPoint MinSector = ArcTerrainResidencyIndex_c4e8::SectorKeyFromCell(GroupMinCell, SectorShift);
		const FIntPoint MaxSector = ArcTerrainResidencyIndex_c4e8::SectorKeyFromCell(GroupMaxCell, SectorShift);
		double Best = TNumericLimits<double>::Max();
		for (int32 SectorY = MinSector.Y; SectorY <= MaxSector.Y; ++SectorY)
		{
			for (int32 SectorX = MinSector.X; SectorX <= MaxSector.X; ++SectorX)
			{
				const FArcTerrainSectorContentRecord* Record =
					State.SectorContentIndex.Find(FIntPoint(SectorX, SectorY));
				if (Record == nullptr)
				{
					continue;
				}
				for (const FArcTerrainResidencyCoverageSpan& Span : Record->Spans)
				{
					const FIntPoint ClipMin(FMath::Max(Span.MinCell.X, GroupMinCell.X),
						FMath::Max(Span.MinCell.Y, GroupMinCell.Y));
					const FIntPoint ClipMax(FMath::Min(Span.MaxCell.X, GroupMaxCell.X),
						FMath::Min(Span.MaxCell.Y, GroupMaxCell.Y));
					if (ClipMin.X > ClipMax.X || ClipMin.Y > ClipMax.Y)
					{
						continue;
					}
					FArcTerrainResidencyCoverageSpan Clipped = Span;
					Clipped.MinCell = ClipMin;
					Clipped.MaxCell = ClipMax;
					Best = FMath::Min(Best, SpanDistSq(Clipped, CellSize, TerrainTransform, Invokers));
				}
			}
		}
		return Best;
	}

	bool IsSectorGranted(const TArray<FArcTerrainResidentSector>& ResidentSectors, FArcTerrainSectorId Id)
	{
		for (const FArcTerrainResidentSector& Sector : ResidentSectors)
		{
			if (Sector.Id == Id)
			{
				return true;
			}
		}
		return false;
	}

	void ConsiderTier0Sector(const AArcTerrain& Terrain, FArcTerrainState& State, FIntPoint SectorKey,
		int32 SectorShift, const FArcTerrainTileGrid& BaseHeightGrid, double CellSize,
		const FTransform& TerrainTransform, TConstArrayView<FVector> Invokers,
		double PreviewRadiusSq, double PreviewUnloadRadiusSq)
	{
		double DistSq = TNumericLimits<double>::Max();
		if (!EvaluateTier0Sector(Terrain, State, SectorKey, SectorShift, BaseHeightGrid, CellSize,
			TerrainTransform, Invokers, DistSq))
		{
			return;
		}
		const FArcTerrainSectorId SectorId{ 0, SectorKey };
		const bool bWasResident = IsSectorGranted(State.ResidentSectors, SectorId);
		if (DistSq > (bWasResident ? PreviewUnloadRadiusSq : PreviewRadiusSq))
		{
			return;
		}
		FindOrAddCachedWanted(State, SectorId).DistSq = DistSq;
	}

	void DropTier0SectorIfUnused(FArcTerrainState& State, FIntPoint SectorKey,
		TConstArrayView<FArcTerrainCachedInvokerWindow> Remaining)
	{
		if (InAnyInvokerWindow(Remaining, SectorKey, true))
		{
			return;
		}
		const FArcTerrainSectorId SectorId{ 0, SectorKey };
		const int32* Existing = State.CachedWantedSectorIndex.Find(SectorId);
		if (Existing == nullptr)
		{
			return;
		}
		if (State.CachedWantedSectors[*Existing].bPinned)
		{
			return;
		}
		RemoveCachedWanted(State, SectorId);
	}

	void ApplyPinnedSectors(const AArcTerrain& Terrain, FArcTerrainState& State, int32 SectorShift,
		const FArcTerrainTileGrid& BaseHeightGrid, double CellSize, const FTransform& TerrainTransform,
		TConstArrayView<FVector> Invokers, double Now)
	{
		for (const FArcTerrainCellKey& PinnedKey : State.PinnedCells)
		{
			const FArcTerrainCellState* PinnedCell = State.Cells.Find(PinnedKey);
			if (PinnedCell == nullptr || State.Hot(*PinnedCell).PinnedUntil <= Now)
			{
				continue;
			}
			const FIntPoint PinnedSectorKey = ArcTerrainAtlas::GroupKeyFromCell(PinnedKey, SectorShift);
			const FArcTerrainSectorId PinnedSectorId{ 0, PinnedSectorKey };
			const bool bFirstReach = !State.CachedWantedSectorIndex.Contains(PinnedSectorId);
			ArcTerrainSubsystemPrivate::FWantedSector& Sector = FindOrAddCachedWanted(State, PinnedSectorId);
			Sector.bPinned = true;
			if (bFirstReach)
			{
				double DistSq = TNumericLimits<double>::Max();
				EvaluateTier0Sector(Terrain, State, PinnedSectorKey, SectorShift, BaseHeightGrid, CellSize,
					TerrainTransform, Invokers, DistSq);
				Sector.DistSq = DistSq;
			}
		}
	}

	void ApplyPinnedWeightGroups(FArcTerrainState& State, int32 GroupShift, double CellSize,
		const FTransform& TerrainTransform, TConstArrayView<FVector> Invokers, double Now)
	{
		const int32 GroupCellSpan = 1 << GroupShift;
		for (const FArcTerrainCellKey& PinnedKey : State.PinnedCells)
		{
			const FArcTerrainCellState* PinnedCell = State.Cells.Find(PinnedKey);
			if (PinnedCell == nullptr || State.Hot(*PinnedCell).PinnedUntil <= Now)
			{
				continue;
			}
			const FIntPoint PinnedGroupKey = ArcTerrainAtlas::GroupKeyFromCell(PinnedKey, GroupShift);
			const FArcTerrainWeightGroupId PinnedGroupId{ 0, PinnedGroupKey };
			const bool bFirstReach = !State.CachedWantedWeightIndex.Contains(PinnedGroupId);
			ArcTerrainSubsystemPrivate::FWantedWeightGroup& Group =
				FindOrAddCachedWantedWeight(State, PinnedGroupId);
			Group.bPinned = true;
			if (bFirstReach)
			{
				const double GroupMinX = static_cast<double>(PinnedGroupKey.X * GroupCellSpan) * CellSize;
				const double GroupMinY = static_cast<double>(PinnedGroupKey.Y * GroupCellSpan) * CellSize;
				Group.DistSq = ArcTerrainSubsystemPrivate::RectInvokerDistSq(
					GroupMinX, GroupMinY,
					GroupMinX + static_cast<double>(GroupCellSpan) * CellSize,
					GroupMinY + static_cast<double>(GroupCellSpan) * CellSize,
					TerrainTransform, Invokers);
			}
		}
	}

	bool WeightGroupHasContent(const FArcTerrainState& State, int32 Tier, FIntPoint GroupKey)
	{
		const FArcTerrainWeightResidencyRecord* Record =
			State.WeightResidencyIndex.Find(FArcTerrainWeightResidencyKey{ Tier, GroupKey });
		return Record != nullptr && Record->MaxSourceDim > 0;
	}

	void ConsiderWeightGroup(FArcTerrainState& State, FIntPoint GroupKey, int32 GroupShift, double CellSize,
		const FTransform& TerrainTransform, TConstArrayView<FVector> Invokers,
		double ResidencyRadiusSq, double UnloadRadiusSq)
	{
		if (!WeightGroupHasContent(State, 0, GroupKey))
		{
			return;
		}
		const double DistSq = WeightGroupContentDistSq(State, GroupKey, GroupShift, CellSize,
			TerrainTransform, Invokers);
		if (DistSq == TNumericLimits<double>::Max())
		{
			return;
		}
		const FArcTerrainWeightGroupId GroupId{ 0, GroupKey };
		bool bWasResident = false;
		for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
		{
			if (Group.Tier == 0 && Group.Key == GroupKey)
			{
				bWasResident = true;
				break;
			}
		}
		if (DistSq > (bWasResident ? UnloadRadiusSq : ResidencyRadiusSq))
		{
			return;
		}
		FindOrAddCachedWantedWeight(State, GroupId).DistSq = DistSq;
	}

	void AddCoarseWindow(AArcTerrain& Terrain, FArcTerrainState& State, const FArcTerrainTierLadder& Ladder,
		const FArcTerrainConfig& Config, const FTransform& TerrainTransform,
		TConstArrayView<FVector> Invokers, const FVector& Invoker)
	{
		using namespace ArcTerrainSubsystemPrivate;
		for (int32 Tier = 1; Tier < Ladder.Tiers.Num(); ++Tier)
		{
			const FResolvedTier& TierEntry = Ladder.Tiers[Tier];
			const bool bLastTier = Tier == Ladder.Tiers.Num() - 1;
			const double OuterRadius = TierEntry.BandOuterRadiusCm;
			const double OuterUnloadRadius = bLastTier ? TierEntry.BandUnloadRadiusCm : OuterRadius;
			const double OuterRadiusSq = OuterRadius * OuterRadius;
			const double OuterUnloadRadiusSq = OuterUnloadRadius * OuterUnloadRadius;
			const FArcTerrainTileGrid TierGrid =
				ArcTerrainTileFormat::HeightTileGrid(Config, TierEntry.CumulativeShift);
			if (!TierGrid.IsValid())
			{
				continue;
			}
			const int32 CellSectorShift = TierEntry.CellSectorShift;
			const double SectorSize =
				Config.CellSize() * static_cast<double>(static_cast<int64>(1) << CellSectorShift);
			const FSectorWindow Window = ComputeSectorWindow(Invoker, OuterUnloadRadius, CellSectorShift,
				Config.CellSize(), TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
			for (int32 SectorY = Window.MinY; SectorY <= Window.MaxY; ++SectorY)
			{
				for (int32 SectorX = Window.MinX; SectorX <= Window.MaxX; ++SectorX)
				{
					const FArcTerrainSectorId SectorId{ Tier, FIntPoint(SectorX, SectorY) };
					if (State.CachedWantedSectorIndex.Contains(SectorId))
					{
						continue;
					}
					const FIntPoint TileKey = CoarseSectorTileKey(TierGrid, SectorId.Key, CellSectorShift);
					if (!ArcTerrainTileAccess::HasTile(Terrain, EArcTerrainRail::Height, Tier, TileKey, &TierGrid))
					{
						continue;
					}
					const double MinX = static_cast<double>(SectorX) * SectorSize;
					const double MinY = static_cast<double>(SectorY) * SectorSize;
					const double DistSq = RectInvokerDistSq(MinX, MinY, MinX + SectorSize, MinY + SectorSize,
						TerrainTransform, Invokers);
					bool bWasResident = false;
					for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
					{
						if (Sector.Id == SectorId)
						{
							bWasResident = true;
							break;
						}
					}
					if (DistSq > (bWasResident ? OuterUnloadRadiusSq : OuterRadiusSq))
					{
						continue;
					}
					FindOrAddCachedWanted(State, SectorId).DistSq = DistSq;
				}
			}
		}
	}

	void AddCoarseWeightWindow(AArcTerrain& Terrain, FArcTerrainState& State,
		const FArcTerrainWeightLadder& WeightLadder, const FArcTerrainConfig& Config,
		const FTransform& TerrainTransform, TConstArrayView<FVector> Invokers, const FVector& Invoker)
	{
		using namespace ArcTerrainSubsystemPrivate;
		for (int32 Tier = 1; Tier < WeightLadder.Tiers.Num(); ++Tier)
		{
			const FResolvedWeightTier& TierEntry = WeightLadder.Tiers[Tier];
			const bool bLastTier = Tier == WeightLadder.Tiers.Num() - 1;
			const double OuterRadius = TierEntry.BandOuterRadiusCm;
			const double OuterUnloadRadius = bLastTier ? TierEntry.BandUnloadRadiusCm : OuterRadius;
			const double OuterRadiusSq = OuterRadius * OuterRadius;
			const double OuterUnloadRadiusSq = OuterUnloadRadius * OuterUnloadRadius;
			const FArcTerrainTileGrid TierGrid = ArcTerrainTileFormat::WeightTileGrid(Config,
				TierEntry.CellWeightQuads, TierEntry.CumulativeShift);
			if (!TierGrid.IsValid())
			{
				continue;
			}
			const int32 SliceShift = TierEntry.SliceShift;
			const double GroupSize = Config.CellSize() * static_cast<double>(static_cast<int64>(1) << SliceShift);
			const FSectorWindow Window = ComputeSectorWindow(Invoker, OuterUnloadRadius, SliceShift,
				Config.CellSize(), TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
			for (int32 GroupY = Window.MinY; GroupY <= Window.MaxY; ++GroupY)
			{
				for (int32 GroupX = Window.MinX; GroupX <= Window.MaxX; ++GroupX)
				{
					const FArcTerrainWeightGroupId GroupId{ Tier, FIntPoint(GroupX, GroupY) };
					if (State.CachedWantedWeightIndex.Contains(GroupId))
					{
						continue;
					}
					const FIntPoint TileKey = CoarseWeightGroupTileKey(TierGrid, GroupId.Key, SliceShift);
					if (!ArcTerrainTileAccess::HasTile(Terrain, EArcTerrainRail::Weight, Tier, TileKey, &TierGrid))
					{
						continue;
					}
					const double MinX = static_cast<double>(GroupX) * GroupSize;
					const double MinY = static_cast<double>(GroupY) * GroupSize;
					const double DistSq = RectInvokerDistSq(MinX, MinY, MinX + GroupSize, MinY + GroupSize,
						TerrainTransform, Invokers);
					bool bWasResident = false;
					for (const FArcTerrainResidentWeightGroup& Group : State.ResidentWeightGroups)
					{
						if (Group.Tier == Tier && Group.Key == GroupId.Key)
						{
							bWasResident = true;
							break;
						}
					}
					if (DistSq > (bWasResident ? OuterUnloadRadiusSq : OuterRadiusSq))
					{
						continue;
					}
					FindOrAddCachedWantedWeight(State, GroupId).DistSq = DistSq;
				}
			}
		}
	}

	bool CoarseKeyCovered(TConstArrayView<FVector> RemainingPositions, FIntPoint Key, int32 CellSectorShift,
		double OuterUnloadRadius, const FArcTerrainConfig& Config, const FTransform& TerrainTransform)
	{
		using namespace ArcTerrainSubsystemPrivate;
		for (const FVector& Position : RemainingPositions)
		{
			const FSectorWindow Window = ComputeSectorWindow(Position, OuterUnloadRadius, CellSectorShift,
				Config.CellSize(), TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
			if (WindowContains(Window, Key))
			{
				return true;
			}
		}
		return false;
	}

	void DropWeightGroupIfUnused(FArcTerrainState& State, FIntPoint GroupKey, int32 GroupShift,
		double WeightUnloadRadius, const FArcTerrainConfig& Config, const FTransform& TerrainTransform,
		TConstArrayView<FVector> RemainingPositions)
	{
		if (CoarseKeyCovered(RemainingPositions, GroupKey, GroupShift, WeightUnloadRadius, Config,
			TerrainTransform))
		{
			return;
		}
		const FArcTerrainWeightGroupId GroupId{ 0, GroupKey };
		const int32* Existing = State.CachedWantedWeightIndex.Find(GroupId);
		if (Existing == nullptr)
		{
			return;
		}
		if (State.CachedWantedWeightGroups[*Existing].bPinned)
		{
			return;
		}
		RemoveCachedWantedWeight(State, GroupId);
	}

	void RemoveInvokerCoarse(FArcTerrainState& State, const FArcTerrainTierLadder& Ladder,
		const FArcTerrainWeightLadder& WeightLadder, const FArcTerrainConfig& Config,
		const FTransform& TerrainTransform, const FVector& OldPosition,
		TConstArrayView<FVector> RemainingPositions)
	{
		using namespace ArcTerrainSubsystemPrivate;
		for (int32 Tier = 1; Tier < Ladder.Tiers.Num(); ++Tier)
		{
			const FResolvedTier& TierEntry = Ladder.Tiers[Tier];
			const bool bLastTier = Tier == Ladder.Tiers.Num() - 1;
			const double OuterUnloadRadius = bLastTier ? TierEntry.BandUnloadRadiusCm : TierEntry.BandOuterRadiusCm;
			const FSectorWindow Window = ComputeSectorWindow(OldPosition, OuterUnloadRadius,
				TierEntry.CellSectorShift, Config.CellSize(), TerrainTransform,
				static_cast<double>(GMaxSectorWindowSpan));
			for (int32 SectorY = Window.MinY; SectorY <= Window.MaxY; ++SectorY)
			{
				for (int32 SectorX = Window.MinX; SectorX <= Window.MaxX; ++SectorX)
				{
					const FIntPoint Key(SectorX, SectorY);
					if (CoarseKeyCovered(RemainingPositions, Key, TierEntry.CellSectorShift, OuterUnloadRadius,
						Config, TerrainTransform))
					{
						continue;
					}
					RemoveCachedWanted(State, FArcTerrainSectorId{ Tier, Key });
				}
			}
		}
		for (int32 Tier = 1; Tier < WeightLadder.Tiers.Num(); ++Tier)
		{
			const FResolvedWeightTier& TierEntry = WeightLadder.Tiers[Tier];
			const bool bLastTier = Tier == WeightLadder.Tiers.Num() - 1;
			const double OuterUnloadRadius = bLastTier ? TierEntry.BandUnloadRadiusCm : TierEntry.BandOuterRadiusCm;
			const FSectorWindow Window = ComputeSectorWindow(OldPosition, OuterUnloadRadius, TierEntry.SliceShift,
				Config.CellSize(), TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
			for (int32 GroupY = Window.MinY; GroupY <= Window.MaxY; ++GroupY)
			{
				for (int32 GroupX = Window.MinX; GroupX <= Window.MaxX; ++GroupX)
				{
					const FIntPoint Key(GroupX, GroupY);
					if (CoarseKeyCovered(RemainingPositions, Key, TierEntry.SliceShift, OuterUnloadRadius,
						Config, TerrainTransform))
					{
						continue;
					}
					RemoveCachedWantedWeight(State, FArcTerrainWeightGroupId{ Tier, Key });
				}
			}
		}
	}

	void CapWantedSets(FArcTerrainState& State, const FArcTerrainTierLadder& Ladder,
		const FArcTerrainWeightLadder& WeightLadder, int32 MaxResidentSectors, int32 MaxResidentWeightCells,
		int32& OutNumPinnedSectors, int32& OutNumPinnedWeightGroups, int32& OutNumWantedBeforeCap,
		int32& OutNumWantedWeightBeforeCap)
	{
		using namespace ArcTerrainSubsystemPrivate;
		TArray<FWantedSector> WantedByTier[ArcTerrain::MaxTiers];
		for (const FWantedSector& Sector : State.CachedWantedSectors)
		{
			WantedByTier[Sector.Id.Tier].Add(Sector);
		}
		TArray<FWantedSector>& WantedTier0 = WantedByTier[0];
		OutNumPinnedSectors = 0;
		for (const FWantedSector& Sector : WantedTier0)
		{
			OutNumPinnedSectors += Sector.bPinned ? 1 : 0;
		}
		SortPinnedExemptWanted(WantedTier0);
		OutNumWantedBeforeCap = WantedTier0.Num();
		TrimPinnedExemptWantedSet(WantedTier0, MaxResidentSectors, OutNumPinnedSectors);
		for (int32 Tier = 1; Tier < ArcTerrain::MaxTiers; ++Tier)
		{
			const int32 TierBudget = Ladder.Tiers.IsValidIndex(Tier)
				? FMath::Max(Ladder.Tiers[Tier].MaxResidentSectors, 0)
				: 0;
			TrimCoarseWantedSet(WantedByTier[Tier], TierBudget);
		}
		State.CachedWantedSectors.Reset();
		State.CachedWantedSectorIndex.Reset();
		for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
		{
			for (const FWantedSector& Sector : WantedByTier[Tier])
			{
				FindOrAddCachedWanted(State, Sector.Id) = Sector;
			}
		}

		TArray<FWantedWeightGroup> WantedWeightByTier[ArcTerrain::MaxTiers];
		for (const FWantedWeightGroup& Group : State.CachedWantedWeightGroups)
		{
			WantedWeightByTier[Group.Id.Tier].Add(Group);
		}
		TArray<FWantedWeightGroup>& WantedWeightTier0 = WantedWeightByTier[0];
		OutNumPinnedWeightGroups = 0;
		for (const FWantedWeightGroup& Group : WantedWeightTier0)
		{
			OutNumPinnedWeightGroups += Group.bPinned ? 1 : 0;
		}
		SortPinnedExemptWanted(WantedWeightTier0);
		OutNumWantedWeightBeforeCap = State.CachedWantedWeightGroups.Num();
		TrimPinnedExemptWantedSet(WantedWeightTier0, MaxResidentWeightCells, OutNumPinnedWeightGroups);
		for (int32 Tier = 1; Tier < ArcTerrain::MaxTiers; ++Tier)
		{
			const int32 TierBudget = WeightLadder.Tiers.IsValidIndex(Tier)
				? FMath::Max(WeightLadder.Tiers[Tier].MaxResidentWeightGroups, 0)
				: 0;
			TrimCoarseWantedSet(WantedWeightByTier[Tier], TierBudget);
		}
		State.CachedWantedWeightGroups.Reset();
		State.CachedWantedWeightIndex.Reset();
		for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
		{
			for (const FWantedWeightGroup& Group : WantedWeightByTier[Tier])
			{
				FindOrAddCachedWantedWeight(State, Group.Id) = Group;
			}
		}
	}

	void RebuildInvokerCache(TConstArrayView<FVector> InvokerPositions, int32 SectorShift, double CellSize,
		const FTransform& TerrainTransform, double PreviewRadius, double PreviewUnloadRadius,
		FArcTerrainResidencySelectionCache& Cache)
	{
		using namespace ArcTerrainSubsystemPrivate;
		Cache.Invokers.Reset();
		Cache.QuantizedPositions.Reset();
		Cache.Invokers.Reserve(InvokerPositions.Num());
		Cache.QuantizedPositions.Reserve(InvokerPositions.Num());
		TArray<TPair<FIntPoint, int32>> Sorted;
		Sorted.Reserve(InvokerPositions.Num());
		for (int32 Index = 0; Index < InvokerPositions.Num(); ++Index)
		{
			Sorted.Emplace(IntegerCentreOf(InvokerPositions[Index], SectorShift, CellSize, TerrainTransform),
				Index);
		}
		Sorted.Sort([](const TPair<FIntPoint, int32>& A, const TPair<FIntPoint, int32>& B)
		{
			if (A.Key.X != B.Key.X)
			{
				return A.Key.X < B.Key.X;
			}
			return A.Key.Y < B.Key.Y;
		});
		for (const TPair<FIntPoint, int32>& Pair : Sorted)
		{
			FArcTerrainCachedInvokerWindow Window;
			Window.IntegerCentre = Pair.Key;
			Window.InnerWindow = ComputeSectorWindow(InvokerPositions[Pair.Value], PreviewRadius, SectorShift,
				CellSize, TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
			Window.OuterWindow = ComputeSectorWindow(InvokerPositions[Pair.Value], PreviewUnloadRadius,
				SectorShift, CellSize, TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
			Cache.Invokers.Add(Window);
			Cache.QuantizedPositions.Add(InvokerPositions[Pair.Value]);
		}
	}

	void AddInvokerWindows(AArcTerrain& Terrain, FArcTerrainState& State,
		const FArcTerrainCachedInvokerWindow& Window, const FVector& Position,
		const FArcTerrainTierLadder& Ladder, const FArcTerrainWeightLadder& WeightLadder,
		const FArcTerrainConfig& Config, int32 SectorShift, const FArcTerrainTileGrid& BaseHeightGrid,
		const FTransform& TerrainTransform,
		TConstArrayView<FVector> Invokers, double PreviewRadiusSq, double PreviewUnloadRadiusSq,
		double WeightRadiusSq, double WeightUnloadRadiusSq, int32 WeightShift, double WeightUnloadRadius)
	{
		using namespace ArcTerrainSubsystemPrivate;
		TSet<FIntPoint> Keys;
		AppendWindowKeys(Window.OuterWindow, Keys);
		for (const FIntPoint& Key : Keys)
		{
			ConsiderTier0Sector(Terrain, State, Key, SectorShift, BaseHeightGrid, Config.CellSize(),
				TerrainTransform, Invokers, PreviewRadiusSq, PreviewUnloadRadiusSq);
		}
		const FSectorWindow WeightWindow = ComputeSectorWindow(Position, WeightUnloadRadius, WeightShift,
			Config.CellSize(), TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
		TSet<FIntPoint> WeightKeys;
		AppendWindowKeys(WeightWindow, WeightKeys);
		for (const FIntPoint& Key : WeightKeys)
		{
			ConsiderWeightGroup(State, Key, WeightShift, Config.CellSize(), TerrainTransform, Invokers,
				WeightRadiusSq, WeightUnloadRadiusSq);
		}
		AddCoarseWindow(Terrain, State, Ladder, Config, TerrainTransform, Invokers, Position);
		AddCoarseWeightWindow(Terrain, State, WeightLadder, Config, TerrainTransform, Invokers, Position);
	}

	bool ReevaluateCachedContentWindows(const AArcTerrain& Terrain, FArcTerrainState& State,
		const FArcTerrainResidencySelectionCache& Cache, const FArcTerrainConfig& Config,
		int32 SectorShift, const FArcTerrainTileGrid& BaseHeightGrid, const FTransform& TerrainTransform,
		double PreviewRadiusSq, double PreviewUnloadRadiusSq, double WeightRadiusSq,
		double WeightUnloadRadiusSq, int32 WeightShift, double WeightUnloadRadius)
	{
		using namespace ArcTerrainSubsystemPrivate;
		bool bChanged = false;
		for (int32 InvokerIndex = 0; InvokerIndex < Cache.Invokers.Num(); ++InvokerIndex)
		{
			TSet<FIntPoint> SectorKeys;
			AppendWindowKeys(Cache.Invokers[InvokerIndex].OuterWindow, SectorKeys);
			for (const FIntPoint& Key : SectorKeys)
			{
				const FArcTerrainSectorId SectorId{ 0, Key };
				const int32* Existing = State.CachedWantedSectorIndex.Find(SectorId);
				const bool bWasWanted = Existing != nullptr;
				const double PreviousDistSq = bWasWanted
					? State.CachedWantedSectors[*Existing].DistSq
					: TNumericLimits<double>::Max();
				double DistSq = TNumericLimits<double>::Max();
				const bool bHasAuthority = EvaluateTier0Sector(Terrain, State, Key, SectorShift,
					BaseHeightGrid, Config.CellSize(), TerrainTransform, Cache.QuantizedPositions, DistSq);
				const bool bWasResident = IsSectorGranted(State.ResidentSectors, SectorId);
				const bool bShouldWant = bHasAuthority
					&& DistSq <= (bWasResident ? PreviewUnloadRadiusSq : PreviewRadiusSq);
				if (bShouldWant)
				{
					FindOrAddCachedWanted(State, SectorId).DistSq = DistSq;
					bChanged = bChanged || !bWasWanted || PreviousDistSq != DistSq;
					continue;
				}
				if (Existing != nullptr && !State.CachedWantedSectors[*Existing].bPinned)
				{
					RemoveCachedWanted(State, SectorId);
					bChanged = true;
				}
			}
			const FSectorWindow WeightWindow = ComputeSectorWindow(
				Cache.QuantizedPositions[InvokerIndex], WeightUnloadRadius, WeightShift,
				Config.CellSize(), TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
			TSet<FIntPoint> WeightKeys;
			AppendWindowKeys(WeightWindow, WeightKeys);
			for (const FIntPoint& Key : WeightKeys)
			{
				if (WeightGroupHasContent(State, 0, Key))
				{
					ConsiderWeightGroup(State, Key, WeightShift, Config.CellSize(), TerrainTransform,
						Cache.QuantizedPositions, WeightRadiusSq, WeightUnloadRadiusSq);
					continue;
				}
				const FArcTerrainWeightGroupId GroupId{ 0, Key };
				const int32* Existing = State.CachedWantedWeightIndex.Find(GroupId);
				if (Existing != nullptr && !State.CachedWantedWeightGroups[*Existing].bPinned)
				{
					RemoveCachedWantedWeight(State, GroupId);
				}
			}
		}
		return bChanged;
	}

	void RerankWantedInWindows(const AArcTerrain& Terrain, FArcTerrainState& State,
		const ArcTerrainSubsystemPrivate::FSectorWindow& SectorWindow,
		const ArcTerrainSubsystemPrivate::FSectorWindow& WeightWindow, int32 SectorShift,
		const FArcTerrainTileGrid& BaseHeightGrid, int32 WeightShift, double CellSize,
		const FTransform& TerrainTransform, TConstArrayView<FVector> Invokers)
	{
		using namespace ArcTerrainSubsystemPrivate;
		TSet<FIntPoint> SectorKeys;
		AppendWindowKeys(SectorWindow, SectorKeys);
		for (const FIntPoint& Key : SectorKeys)
		{
			const int32* Existing = State.CachedWantedSectorIndex.Find(FArcTerrainSectorId{ 0, Key });
			if (Existing != nullptr)
			{
				double DistSq = TNumericLimits<double>::Max();
				EvaluateTier0Sector(Terrain, State, Key, SectorShift, BaseHeightGrid, CellSize,
					TerrainTransform, Invokers, DistSq);
				State.CachedWantedSectors[*Existing].DistSq = DistSq;
			}
		}
		TSet<FIntPoint> WeightKeys;
		AppendWindowKeys(WeightWindow, WeightKeys);
		for (const FIntPoint& Key : WeightKeys)
		{
			const int32* Existing = State.CachedWantedWeightIndex.Find(FArcTerrainWeightGroupId{ 0, Key });
			if (Existing != nullptr)
			{
				State.CachedWantedWeightGroups[*Existing].DistSq =
					WeightGroupContentDistSq(State, Key, WeightShift, CellSize, TerrainTransform, Invokers);
			}
		}
	}

	void ExpirePins(FArcTerrainState& State, double Now)
	{
		TArray<FArcTerrainCellKey> Expired;
		double NextExpiry = 0.0;
		for (const FArcTerrainCellKey& PinnedKey : State.PinnedCells)
		{
			const FArcTerrainCellState* Cell = State.Cells.Find(PinnedKey);
			if (Cell == nullptr || State.Hot(*Cell).PinnedUntil <= Now)
			{
				Expired.Add(PinnedKey);
				continue;
			}
			const double Until = State.Hot(*Cell).PinnedUntil;
			if (NextExpiry <= 0.0 || Until < NextExpiry)
			{
				NextExpiry = Until;
			}
		}
		for (const FArcTerrainCellKey& Key : Expired)
		{
			State.PinnedCells.Remove(Key);
			if (State.DeferredResidencyEvictions.Remove(Key) > 0)
			{
				State.ReadyResidencyEvictions.Add(Key);
			}
			State.bPinMembershipDirty = true;
		}
		State.NextPinExpiry = NextExpiry;
	}

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

	void EnqueueResidentCellAuthority(FArcTerrainState& State, FArcTerrainCellKey CellKey, bool bAdmit)
	{
		ArcTerrainResidencyIndex_c4e8::EnqueueCompositeReady(State, CellKey);
		if (bAdmit)
		{
			State.PendingResidencyAdmissions.Add(CellKey);
		}
	}

	void ProcessTileAuthorityEvents(AArcTerrain& Terrain, FArcTerrainState& State, int32 SectorShift,
		const FArcTerrainTileGrid& BaseHeightGrid)
	{
		TArray<FArcTerrainCellKey> TileCells;
		for (const FArcTerrainTileAuthorityEvent& Event : State.PendingTileAuthorityEvents)
		{
			if (Event.Reason == EArcTerrainTileAuthorityReason::LayoutChanged
				|| Event.Reason == EArcTerrainTileAuthorityReason::StampBoundsChanged)
			{
				continue;
			}
			if (Event.Reason == EArcTerrainTileAuthorityReason::PendingHeightReset)
			{
				for (const int32 HotIndex : State.ResidentCells)
				{
					EnqueueResidentCellAuthority(State, State.HotCells[HotIndex].Key, true);
				}
				continue;
			}
			FArcTerrainTileGrid Grid;
			if (Event.Rail == EArcTerrainRail::Height && Event.Tier == 0)
			{
				Grid = BaseHeightGrid;
				if (!Grid.IsValid())
				{
					continue;
				}
			}
			else if (!ArcTerrainTileAccess::ResolveGrid(Terrain, Event.Rail, Event.Tier, Grid))
			{
				continue;
			}
			MapTileToCells(Grid, Event.TileKey, TileCells);
			for (const FArcTerrainCellKey& CellKey : TileCells)
			{
				const FIntPoint SectorKey = ArcTerrainAtlas::GroupKeyFromCell(CellKey, SectorShift);
				if (IsSectorGranted(State.ResidentSectors, FArcTerrainSectorId{ 0, SectorKey })
					|| State.Cells.Contains(CellKey))
				{
					const bool bAdmit = Event.Reason == EArcTerrainTileAuthorityReason::TileReleased
						|| Event.Reason == EArcTerrainTileAuthorityReason::PendingHeightChanged;
					EnqueueResidentCellAuthority(State, CellKey, bAdmit);
				}
			}
		}
		State.PendingTileAuthorityEvents.Reset();
	}

	void AdmitSectorContentCells(FArcTerrainState& State, FIntPoint SectorKey,
		ArcTerrainCellCreationLocal_71a3::FHeightTileProbe& HeightTileProbe)
	{
		const FArcTerrainSectorContentRecord* Record = State.SectorContentIndex.Find(SectorKey);
		if (Record == nullptr)
		{
			return;
		}
		for (const FArcTerrainResidencyCoverageSpan& Span : Record->Spans)
		{
			for (int32 CellY = Span.MinCell.Y; CellY <= Span.MaxCell.Y; ++CellY)
			{
				for (int32 CellX = Span.MinCell.X; CellX <= Span.MaxCell.X; ++CellX)
				{
					const FArcTerrainCellKey CellKey(CellX, CellY);
					if (State.Cells.Contains(CellKey))
					{
						ArcTerrainResidencyIndex_c4e8::EnqueueCompositeReady(State, CellKey);
						continue;
					}
					if (HeightTileProbe.IsTileAuthoritativeForCell(CellKey))
					{
						continue;
					}
					State.FindOrAddCell(CellKey);
					ArcTerrainResidencyIndex_c4e8::EnqueueCompositeReady(State, CellKey);
					ArcTerrainDiag::NoteCellCreated(ArcTerrainDiag::ECellRail::Content, CellKey);
				}
			}
		}
	}

	bool ReconcileEpochWindows(AArcTerrain& Terrain, FArcTerrainState& State,
		const FArcTerrainTierLadder& Ladder, const FArcTerrainWeightLadder& WeightLadder,
		const FArcTerrainConfig& Config, int32 SectorShift, const FArcTerrainTileGrid& BaseHeightGrid,
		const FTransform& TerrainTransform, TConstArrayView<FVector> Invokers,
		double PreviewRadiusSq, double PreviewUnloadRadiusSq, double WeightRadiusSq,
		double WeightUnloadRadiusSq, int32 WeightShift, double WeightUnloadRadius)
	{
		const int32 WantedSectorsBefore = State.CachedWantedSectors.Num();
		const int32 WantedWeightsBefore = State.CachedWantedWeightGroups.Num();
		const bool bTier0Changed = ReevaluateCachedContentWindows(Terrain, State,
			State.ResidencySelectionCache, Config, SectorShift, BaseHeightGrid, TerrainTransform,
			PreviewRadiusSq, PreviewUnloadRadiusSq, WeightRadiusSq, WeightUnloadRadiusSq,
			WeightShift, WeightUnloadRadius);
		for (int32 Index = 0; Index < State.ResidencySelectionCache.QuantizedPositions.Num(); ++Index)
		{
			AddCoarseWindow(Terrain, State, Ladder, Config, TerrainTransform, Invokers,
				State.ResidencySelectionCache.QuantizedPositions[Index]);
			AddCoarseWeightWindow(Terrain, State, WeightLadder, Config, TerrainTransform, Invokers,
				State.ResidencySelectionCache.QuantizedPositions[Index]);
		}
		return bTier0Changed || State.CachedWantedSectors.Num() != WantedSectorsBefore
			|| State.CachedWantedWeightGroups.Num() != WantedWeightsBefore;
	}
}

void UArcTerrainSubsystem::TickResidency(
	TConstArrayView<FVector> InvokerPositions,
	double Now,
	const UArcTerrainSettings& Settings,
	TArray<ArcTerrainSubsystemPrivate::FCompositeCandidate>& OutCandidates,
	int32& OutNumSectorCells,
	int32& OutNumGrantedSectors,
	int32& OutNumEvictCandidates,
	int32& OutNumEvicted,
	int32& OutNumHeldByPin,
	int32& OutNumHeldByQueue,
	int32& OutNumCompositeCandidates)
{
	using namespace ArcTerrainSubsystemPrivate;
	using namespace ArcTerrainResidencyReactive_a91c;

#if WITH_EDITORONLY_DATA
	const int32 MaxCompositesPerFrame = FMath::Max(Settings.MaxCompositesPerFrame, 1);
	TArray<FCompositeCandidate>& Candidates = OutCandidates;
	Candidates.Reset();
	Candidates.Reserve(MaxCompositesPerFrame);
#endif
	OutNumSectorCells = 0;
	OutNumGrantedSectors = 0;
	OutNumEvictCandidates = 0;
	OutNumEvicted = 0;
	OutNumHeldByPin = 0;
	OutNumHeldByQueue = 0;
	OutNumCompositeCandidates = 0;

	for (TMap<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>::TIterator TerrainIt(Terrains); TerrainIt; ++TerrainIt)
	{
		AArcTerrain* Terrain = TerrainIt.Key().Get();
		if (Terrain == nullptr)
		{
			GArcTerrainCBTRendererExtension.RetireTerrain_GameThread(TerrainIt.Value().RenderOwner);
			if (FArcTerrainRuntimeWorkCoordinator* Coordinator = FArcTerrainRuntimeWorkCoordinator::TryGet())
			{
				Coordinator->AbandonTerrain(GetRuntimeOwnerId(), TerrainIt.Value().CollisionTerrainId);
			}
			ReleasePendingAtlasFills(TerrainIt.Value().CollisionTerrainId);
			PreviewManager.ReleaseTerrainRender(TerrainIt.Key());
			for (TPair<FArcTerrainCellKey, FArcTerrainCellState>& CellPair : TerrainIt.Value().Cells)
			{
				CellPair.Value.TileSet.Release(RenderTargetPool);
				ResetCellAtlasPublication(CellPair.Value);
			}
			TerrainIt.Value().TileAtlas.Reset();
			TerrainIt.RemoveCurrent();
			continue;
		}

		const FArcTerrainConfig& Config = Terrain->Config;
		FArcTerrainState& State = TerrainIt.Value();
		++State.PipelineTelemetry.StateAgeTicks;
		const FTransform TerrainTransform = Terrain->GetTerrainTransform();

		const FResolvedResidency Residency = ResolveResidency(*Terrain);
		const double PreviewRadius = Residency.PreviewRadius;
		// BOTH knobs come from the resolved layout, not from the raw settings: ResolveTileAtlasLayout may
		// lower SectorShift (device texture limit) AND MaxResidentSectors (array-layer ceiling). Granting
		// more sectors than the atlas has slots makes eviction pick a victim NEARER than the sector being
		// admitted, and the victim — still resident — is re-granted next tick: a backwards LRU that
		// re-clears and refills a slice every frame.
		const FArcTerrainTileAtlasLayout AtlasLayout =
			ResolveTerrainAtlasLayout(*Terrain, State, Residency);
		const FArcTerrainTierLadder Ladder =
			ArcTerrain::ResolveTierLadder(*Terrain, AtlasLayout);
		const FArcTerrainWeightLadder WeightLadder =
			ArcTerrain::ResolveWeightTierLadder(*Terrain, AtlasLayout, AtlasLayout.WeightQuads);
		// Resolved here, not in ResolveResidency: whether tier 0 is the LAST tier is a property of the
		// ladder the atlas actually funds, and the ladder cannot be resolved without the residency it takes.
		const double PreviewUnloadRadius =
			Ladder.Tiers.Num() > 1 ? PreviewRadius : Residency.PreviewUnloadRadius;
		// Tier 0's SHARE of the pool, not the ladder's sum: leaving this on the sum lets tier 0 eat every
		// slot and starve every coarse tier through the atlas's refusal rule.
		const int32 MaxResidentSectors = FMath::Max(Ladder.Tiers[0].MaxResidentSectors, 1);
		const int32 MaxResidentWeightCells = WeightLadder.Tiers.IsEmpty()
			? 1
			: FMath::Max(WeightLadder.Tiers[0].MaxResidentWeightGroups, 1);
		const int32 SectorShift = AtlasLayout.SectorShift;
		FArcTerrainTileGrid BaseHeightGrid;
		ArcTerrainTileAccess::ResolveGrid(*Terrain, EArcTerrainRail::Height, 0, BaseHeightGrid);
		const double PreviewRadiusSq = PreviewRadius * PreviewRadius;
		const double PreviewUnloadRadiusSq = PreviewUnloadRadius * PreviewUnloadRadius;
		const int32 WeightShift = AtlasLayout.WeightSliceShift;
		const double WeightRadius = WeightLadder.Tiers.IsEmpty() ? PreviewRadius : WeightLadder.Tiers[0].BandOuterRadiusCm;
		const double WeightUnloadRadius = WeightLadder.Tiers.Num() > 1
			? WeightRadius
			: (WeightLadder.Tiers.IsEmpty() ? PreviewUnloadRadius : WeightLadder.Tiers[0].BandUnloadRadiusCm);
		const double WeightRadiusSq = WeightRadius * WeightRadius;
		const double WeightUnloadRadiusSq = WeightUnloadRadius * WeightUnloadRadius;

		const bool bIndexNeedsRebuild = State.IndexedSectorShift != SectorShift
			|| State.NumIndexedWeightTiers != WeightLadder.Tiers.Num()
			|| (State.NumIndexedWeightTiers > 0 && State.IndexedWeightShifts[0] != WeightShift);
		if (bIndexNeedsRebuild)
		{
			State.ResidencySelectionCache.SectorShift = SectorShift;
			ArcTerrainResidencyIndex_c4e8::RebuildIndexes(*Terrain, State);
			FArcTerrainTileAuthorityEvent LayoutEvent;
			LayoutEvent.Reason = EArcTerrainTileAuthorityReason::LayoutChanged;
			PublishLocalTileAuthorityEvent(State, LayoutEvent);
		}

		if (State.SectorCellsShift != SectorShift)
		{
			State.SectorCells.Reset();
			for (int32 HotIndex = 0; HotIndex < State.HotCells.Num(); ++HotIndex)
			{
				State.SectorCells
					.FindOrAdd(ArcTerrainAtlas::GroupKeyFromCell(State.HotCells[HotIndex].Key, SectorShift))
					.Add(HotIndex);
			}
			State.SectorCellsShift = SectorShift;
			State.ResidentSectors.Reset();
			State.CachedWantedSectors.Reset();
			State.CachedWantedSectorIndex.Reset();
		}

		TArray<FIntPoint> NewCentres;
		NewCentres.Reserve(InvokerPositions.Num());
		for (const FVector& Invoker : InvokerPositions)
		{
			NewCentres.Add(IntegerCentreOf(Invoker, SectorShift, Config.CellSize(), TerrainTransform));
		}
		NewCentres.Sort([](const FIntPoint& A, const FIntPoint& B)
		{
			if (A.X != B.X)
			{
				return A.X < B.X;
			}
			return A.Y < B.Y;
		});

		TArray<FIntPoint> OldCentres;
		OldCentres.Reserve(State.ResidencySelectionCache.Invokers.Num());
		for (const FArcTerrainCachedInvokerWindow& Cached : State.ResidencySelectionCache.Invokers)
		{
			OldCentres.Add(Cached.IntegerCentre);
		}

		const uint32 ManifestEpoch = ArcTerrainTileAccess::GetManifestCoverageEpoch();
		const uint32 LoadedEpoch = ArcTerrainTileAccess::GetLoadedCoverageEpoch();
		const uint64 LoadedMutationSerial = ArcTerrainTileAccess::GetLoadedMutationSerial(*Terrain);
		const bool bCentresUnchanged = CentresMatch(OldCentres, NewCentres)
			&& State.ResidencySelectionCache.SectorShift == SectorShift
			&& State.ResidencySelectionCache.PreviewRadius == PreviewRadius
			&& State.ResidencySelectionCache.PreviewUnloadRadius == PreviewUnloadRadius;
		const bool bFingerprintUnchanged = bCentresUnchanged
			&& State.ResidencySelectionCache.ConfigRevision == State.ResidencyConfigRevision
			&& State.ResidencySelectionCache.ContentIndexRevision == State.ContentIndexRevision;
		const bool bPinDue = State.NextPinExpiry > 0.0 && Now >= State.NextPinExpiry;
		const bool bRetryDue = State.NextCompositeRetry > 0.0 && Now >= State.NextCompositeRetry;
		const bool bEpochChanged = ManifestEpoch != State.ConsumedResidencyManifestEpoch
			|| LoadedEpoch != State.ConsumedResidencyLoadedEpoch;
		const bool bLocalEpochChanged =
			LoadedMutationSerial != State.PipelineTelemetry.ConsumedLoadedMutationSerial;
		if (bEpochChanged)
		{
			State.PipelineTelemetry.Increment(
				&FArcTerrainPipelineCounters::ResidencyProcessEpochObservations);
		}
		if (bLocalEpochChanged)
		{
			State.PipelineTelemetry.Increment(
				&FArcTerrainPipelineCounters::ResidencyLocalEpochObservations);
		}
		const bool bAuthorityPending = State.PendingTileAuthorityEvents.Num() > 0
			|| State.ResidencySelectionCache.LocalTileAuthorityRevision != State.LocalTileAuthorityRevision;
		const bool bHasInternalWork = State.PendingResidencyAdmissions.Num() > 0
			|| State.CompositeReadyCells.Num() > 0
			|| State.ReadyResidencyEvictions.Num() > 0
			|| State.bPinMembershipDirty;
		const bool bCanSleep = bFingerprintUnchanged && !bPinDue && !bRetryDue && !bAuthorityPending
			&& !bHasInternalWork;

		if (bCanSleep && !bEpochChanged)
		{
			OutNumSectorCells += State.SectorCells.Num();
			OutNumGrantedSectors += State.ResidentSectors.Num();
			continue;
		}

		if (bPinDue)
		{
			ExpirePins(State, Now);
		}
		if (bRetryDue)
		{
			double NextRetry = 0.0;
			TArray<FArcTerrainCellKey> DueRetries;
			for (const FArcTerrainCellKey& RetryKey : State.CompositeRetryCells)
			{
				const FArcTerrainCellState* Cell = State.Cells.Find(RetryKey);
				if (Cell == nullptr)
				{
					DueRetries.Add(RetryKey);
					continue;
				}
				if (Cell->bCompositeResolveRetryPending && Now >= Cell->CompositeResolveRetryAfter)
				{
					ArcTerrainResidencyIndex_c4e8::EnqueueCompositeReady(State, RetryKey);
					DueRetries.Add(RetryKey);
					continue;
				}
				if (Cell->bCompositeResolveRetryPending)
				{
					if (NextRetry <= 0.0 || Cell->CompositeResolveRetryAfter < NextRetry)
					{
						NextRetry = Cell->CompositeResolveRetryAfter;
					}
				}
			}
			for (const FArcTerrainCellKey& DueKey : DueRetries)
			{
				State.CompositeRetryCells.Remove(DueKey);
			}
			State.NextCompositeRetry = NextRetry;
		}

		bool bWantedDirty = bIndexNeedsRebuild || !bFingerprintUnchanged || State.bPinMembershipDirty
			|| State.ResidencySelectionCache.ContentIndexRevision != State.ContentIndexRevision
			|| State.ResidencySelectionCache.ConfigRevision != State.ResidencyConfigRevision;

		FArcTerrainResidencySelectionCache PreviousCache = State.ResidencySelectionCache;
		if (!bCentresUnchanged)
		{
			RebuildInvokerCache(InvokerPositions, SectorShift, Config.CellSize(), TerrainTransform,
				PreviewRadius, PreviewUnloadRadius, State.ResidencySelectionCache);
		}
		State.ResidencySelectionCache.SectorShift = SectorShift;
		State.ResidencySelectionCache.PreviewRadius = PreviewRadius;
		State.ResidencySelectionCache.PreviewUnloadRadius = PreviewUnloadRadius;
		State.ResidencySelectionCache.ConfigRevision = State.ResidencyConfigRevision;
		State.ResidencySelectionCache.ContentIndexRevision = State.ContentIndexRevision;

		const bool bFullWindowRebuild = PreviousCache.Invokers.IsEmpty() || bIndexNeedsRebuild
			|| PreviousCache.ConfigRevision != State.ResidencyConfigRevision;
		if (bFullWindowRebuild)
		{
			State.CachedWantedSectors.Reset();
			State.CachedWantedSectorIndex.Reset();
			State.CachedWantedWeightGroups.Reset();
			State.CachedWantedWeightIndex.Reset();
			for (int32 InvokerIndex = 0; InvokerIndex < State.ResidencySelectionCache.Invokers.Num(); ++InvokerIndex)
			{
				AddInvokerWindows(*Terrain, State, State.ResidencySelectionCache.Invokers[InvokerIndex],
					State.ResidencySelectionCache.QuantizedPositions[InvokerIndex], Ladder, WeightLadder,
					Config, SectorShift, BaseHeightGrid, TerrainTransform,
					State.ResidencySelectionCache.QuantizedPositions,
					PreviewRadiusSq, PreviewUnloadRadiusSq, WeightRadiusSq, WeightUnloadRadiusSq,
					WeightShift, WeightUnloadRadius);
			}
			bWantedDirty = true;
		}
		else if (!bCentresUnchanged)
		{
			TArray<int32> OldUsed;
			OldUsed.Init(0, PreviousCache.Invokers.Num());
			TArray<int32> NewUsed;
			NewUsed.Init(0, State.ResidencySelectionCache.Invokers.Num());
			for (int32 NewIndex = 0; NewIndex < State.ResidencySelectionCache.Invokers.Num(); ++NewIndex)
			{
				for (int32 OldIndex = 0; OldIndex < PreviousCache.Invokers.Num(); ++OldIndex)
				{
					if (OldUsed[OldIndex] != 0)
					{
						continue;
					}
					if (PreviousCache.Invokers[OldIndex].IntegerCentre
						== State.ResidencySelectionCache.Invokers[NewIndex].IntegerCentre)
					{
						OldUsed[OldIndex] = 1;
						NewUsed[NewIndex] = 1;
						break;
					}
				}
			}
			for (int32 NewIndex = 0; NewIndex < State.ResidencySelectionCache.Invokers.Num(); ++NewIndex)
			{
				if (NewUsed[NewIndex] != 0)
				{
					continue;
				}
				int32 AdjacentOld = INDEX_NONE;
				int32 TeleportOld = INDEX_NONE;
				for (int32 OldIndex = 0; OldIndex < PreviousCache.Invokers.Num(); ++OldIndex)
				{
					if (OldUsed[OldIndex] != 0)
					{
						continue;
					}
					const int32 Distance = Chebyshev(PreviousCache.Invokers[OldIndex].IntegerCentre,
						State.ResidencySelectionCache.Invokers[NewIndex].IntegerCentre);
					if (Distance == 1 && AdjacentOld == INDEX_NONE)
					{
						AdjacentOld = OldIndex;
					}
					else if (Distance > 1 && TeleportOld == INDEX_NONE)
					{
						TeleportOld = OldIndex;
					}
				}
				if (AdjacentOld != INDEX_NONE)
				{
					OldUsed[AdjacentOld] = 1;
					NewUsed[NewIndex] = 1;
					TArray<FIntPoint> EnterKeys;
					TArray<FIntPoint> LeaveKeys;
					DiffWindows(PreviousCache.Invokers[AdjacentOld].OuterWindow,
						State.ResidencySelectionCache.Invokers[NewIndex].OuterWindow, EnterKeys, LeaveKeys);
					for (const FIntPoint& Key : EnterKeys)
					{
						ConsiderTier0Sector(*Terrain, State, Key, SectorShift, BaseHeightGrid,
							Config.CellSize(), TerrainTransform, State.ResidencySelectionCache.QuantizedPositions,
							PreviewRadiusSq, PreviewUnloadRadiusSq);
					}
					for (const FIntPoint& Key : LeaveKeys)
					{
						DropTier0SectorIfUnused(State, Key, State.ResidencySelectionCache.Invokers);
					}
					const FSectorWindow OldWeightWindow = ComputeSectorWindow(
						PreviousCache.QuantizedPositions[AdjacentOld], WeightUnloadRadius, WeightShift,
						Config.CellSize(), TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
					const FSectorWindow NewWeightWindow = ComputeSectorWindow(
						State.ResidencySelectionCache.QuantizedPositions[NewIndex], WeightUnloadRadius,
						WeightShift, Config.CellSize(), TerrainTransform,
						static_cast<double>(GMaxSectorWindowSpan));
					TArray<FIntPoint> WeightEnterKeys;
					TArray<FIntPoint> WeightLeaveKeys;
					DiffWindows(OldWeightWindow, NewWeightWindow, WeightEnterKeys, WeightLeaveKeys);
					for (const FIntPoint& Key : WeightEnterKeys)
					{
						ConsiderWeightGroup(State, Key, WeightShift, Config.CellSize(), TerrainTransform,
							State.ResidencySelectionCache.QuantizedPositions, WeightRadiusSq,
							WeightUnloadRadiusSq);
					}
					for (const FIntPoint& Key : WeightLeaveKeys)
					{
						DropWeightGroupIfUnused(State, Key, WeightShift, WeightUnloadRadius, Config,
							TerrainTransform, State.ResidencySelectionCache.QuantizedPositions);
					}
					RemoveInvokerCoarse(State, Ladder, WeightLadder, Config, TerrainTransform,
						PreviousCache.QuantizedPositions[AdjacentOld],
						State.ResidencySelectionCache.QuantizedPositions);
					AddCoarseWindow(*Terrain, State, Ladder, Config, TerrainTransform,
						State.ResidencySelectionCache.QuantizedPositions,
						State.ResidencySelectionCache.QuantizedPositions[NewIndex]);
					AddCoarseWeightWindow(*Terrain, State, WeightLadder, Config, TerrainTransform,
						State.ResidencySelectionCache.QuantizedPositions,
						State.ResidencySelectionCache.QuantizedPositions[NewIndex]);
					RerankWantedInWindows(*Terrain, State,
						State.ResidencySelectionCache.Invokers[NewIndex].OuterWindow,
						NewWeightWindow, SectorShift, BaseHeightGrid, WeightShift, Config.CellSize(), TerrainTransform,
						State.ResidencySelectionCache.QuantizedPositions);
					bWantedDirty = true;
				}
				else if (TeleportOld != INDEX_NONE)
				{
					OldUsed[TeleportOld] = 1;
					NewUsed[NewIndex] = 1;
					TArray<FVector> Remaining = State.ResidencySelectionCache.QuantizedPositions;
					RemoveInvokerCoarse(State, Ladder, WeightLadder, Config, TerrainTransform,
						PreviousCache.QuantizedPositions[TeleportOld], Remaining);
					TSet<FIntPoint> LeaveKeys;
					AppendWindowKeys(PreviousCache.Invokers[TeleportOld].OuterWindow, LeaveKeys);
					for (const FIntPoint& Key : LeaveKeys)
					{
						DropTier0SectorIfUnused(State, Key, State.ResidencySelectionCache.Invokers);
					}
					const FSectorWindow OldTeleportWeight = ComputeSectorWindow(
						PreviousCache.QuantizedPositions[TeleportOld], WeightUnloadRadius, WeightShift,
						Config.CellSize(), TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
					TSet<FIntPoint> WeightLeaveKeys;
					AppendWindowKeys(OldTeleportWeight, WeightLeaveKeys);
					for (const FIntPoint& Key : WeightLeaveKeys)
					{
						DropWeightGroupIfUnused(State, Key, WeightShift, WeightUnloadRadius, Config,
							TerrainTransform, State.ResidencySelectionCache.QuantizedPositions);
					}
					AddInvokerWindows(*Terrain, State, State.ResidencySelectionCache.Invokers[NewIndex],
						State.ResidencySelectionCache.QuantizedPositions[NewIndex], Ladder, WeightLadder,
						Config, SectorShift, BaseHeightGrid, TerrainTransform,
						State.ResidencySelectionCache.QuantizedPositions,
						PreviewRadiusSq, PreviewUnloadRadiusSq, WeightRadiusSq, WeightUnloadRadiusSq,
						WeightShift, WeightUnloadRadius);
					const FSectorWindow TeleportWeightWindow = ComputeSectorWindow(
						State.ResidencySelectionCache.QuantizedPositions[NewIndex], WeightUnloadRadius,
						WeightShift, Config.CellSize(), TerrainTransform,
						static_cast<double>(GMaxSectorWindowSpan));
					RerankWantedInWindows(*Terrain, State,
						State.ResidencySelectionCache.Invokers[NewIndex].OuterWindow,
						TeleportWeightWindow, SectorShift, BaseHeightGrid, WeightShift, Config.CellSize(),
						TerrainTransform,
						State.ResidencySelectionCache.QuantizedPositions);
					bWantedDirty = true;
				}
				else
				{
					NewUsed[NewIndex] = 1;
					AddInvokerWindows(*Terrain, State, State.ResidencySelectionCache.Invokers[NewIndex],
						State.ResidencySelectionCache.QuantizedPositions[NewIndex], Ladder, WeightLadder,
						Config, SectorShift, BaseHeightGrid, TerrainTransform,
						State.ResidencySelectionCache.QuantizedPositions,
						PreviewRadiusSq, PreviewUnloadRadiusSq, WeightRadiusSq, WeightUnloadRadiusSq,
						WeightShift, WeightUnloadRadius);
					bWantedDirty = true;
				}
			}
			for (int32 OldIndex = 0; OldIndex < PreviousCache.Invokers.Num(); ++OldIndex)
			{
				if (OldUsed[OldIndex] != 0)
				{
					continue;
				}
				TSet<FIntPoint> LeaveKeys;
				AppendWindowKeys(PreviousCache.Invokers[OldIndex].OuterWindow, LeaveKeys);
				for (const FIntPoint& Key : LeaveKeys)
				{
					DropTier0SectorIfUnused(State, Key, State.ResidencySelectionCache.Invokers);
				}
				const FSectorWindow OldDisappearWeight = ComputeSectorWindow(
					PreviousCache.QuantizedPositions[OldIndex], WeightUnloadRadius, WeightShift,
					Config.CellSize(), TerrainTransform, static_cast<double>(GMaxSectorWindowSpan));
				TSet<FIntPoint> WeightLeaveKeys;
				AppendWindowKeys(OldDisappearWeight, WeightLeaveKeys);
				for (const FIntPoint& Key : WeightLeaveKeys)
				{
					DropWeightGroupIfUnused(State, Key, WeightShift, WeightUnloadRadius, Config,
						TerrainTransform, State.ResidencySelectionCache.QuantizedPositions);
				}
				RemoveInvokerCoarse(State, Ladder, WeightLadder, Config, TerrainTransform,
					PreviousCache.QuantizedPositions[OldIndex],
					State.ResidencySelectionCache.QuantizedPositions);
				bWantedDirty = true;
			}
		}

		if (!bFullWindowRebuild
			&& PreviousCache.ContentIndexRevision != State.ContentIndexRevision)
		{
			bWantedDirty = ReevaluateCachedContentWindows(*Terrain, State, State.ResidencySelectionCache,
				Config, SectorShift, BaseHeightGrid, TerrainTransform, PreviewRadiusSq, PreviewUnloadRadiusSq,
				WeightRadiusSq, WeightUnloadRadiusSq, WeightShift, WeightUnloadRadius) || bWantedDirty;
		}

		if (State.bPinMembershipDirty || bWantedDirty)
		{
			ApplyPinnedSectors(*Terrain, State, SectorShift, BaseHeightGrid, Config.CellSize(), TerrainTransform,
				State.ResidencySelectionCache.QuantizedPositions, Now);
			ApplyPinnedWeightGroups(State, WeightShift, Config.CellSize(), TerrainTransform,
				State.ResidencySelectionCache.QuantizedPositions, Now);
			State.bPinMembershipDirty = false;
			bWantedDirty = true;
		}

		if (bEpochChanged)
		{
			if (ReconcileEpochWindows(*Terrain, State, Ladder, WeightLadder, Config, SectorShift,
				BaseHeightGrid, TerrainTransform, State.ResidencySelectionCache.QuantizedPositions,
				PreviewRadiusSq, PreviewUnloadRadiusSq, WeightRadiusSq, WeightUnloadRadiusSq,
				WeightShift, WeightUnloadRadius))
			{
				State.PipelineTelemetry.Increment(
					&FArcTerrainPipelineCounters::ResidencyReconcileChanged);
				bWantedDirty = true;
			}
			else
			{
				State.PipelineTelemetry.Increment(
					&FArcTerrainPipelineCounters::ResidencyReconcileUnchanged);
			}
			State.ConsumedResidencyManifestEpoch = ManifestEpoch;
			State.ConsumedResidencyLoadedEpoch = LoadedEpoch;
			State.PipelineTelemetry.ConsumedLoadedMutationSerial = LoadedMutationSerial;
		}

		if (bCanSleep && !bWantedDirty)
		{
			OutNumSectorCells += State.SectorCells.Num();
			OutNumGrantedSectors += State.ResidentSectors.Num();
			continue;
		}

		ProcessTileAuthorityEvents(*Terrain, State, SectorShift, BaseHeightGrid);
		State.ResidencySelectionCache.LocalTileAuthorityRevision = State.LocalTileAuthorityRevision;

		int32 NumPinnedSectors = 0;
		int32 NumPinnedWeightGroups = 0;
		int32 NumWantedBeforeCap = State.CachedWantedSectors.Num();
		int32 NumWantedWeightBeforeCap = State.CachedWantedWeightGroups.Num();
		if (bWantedDirty)
		{
			CapWantedSets(State, Ladder, WeightLadder, MaxResidentSectors, MaxResidentWeightCells,
				NumPinnedSectors, NumPinnedWeightGroups, NumWantedBeforeCap, NumWantedWeightBeforeCap);
		}

		if (ArcTerrainDiag::IsOn())
		{
			FString InvokerText;
			for (const FVector& Invoker : InvokerPositions)
			{
				InvokerText += FString::Printf(TEXT(" (%.0f,%.0f,%.0f)"), Invoker.X, Invoker.Y, Invoker.Z);
			}
			UE_LOGF(LogArcTerrain, Log,"[Residency] invokers %d%ls | radius %.0f m unload %.0f m | wanted %d (cap %d, pinned %d)",
				InvokerPositions.Num(), *InvokerText,
				PreviewRadius * 0.01, PreviewUnloadRadius * 0.01,
				State.CachedWantedSectors.Num(), MaxResidentSectors, NumPinnedSectors);
		}

		if (NumPinnedSectors > MaxResidentSectors && !bWarnedPinOverBudget)
		{
			bWarnedPinOverBudget = true;
			UE_LOGF(LogArcTerrain, Warning,
				"ArcTerrain: %d edit-pinned sectors exceed the terrain's MaxResidentSectors (%d). "
				"Pinned sectors bypass the eviction cap by design (stamp editing), but each holds "
				"pooled tile render targets and an atlas slice — a stamp footprint this large costs "
				"proportionally more VRAM until the pin window (EditPinSeconds) expires, "
				"and sectors past the atlas slot count are refused admission and do not draw.",
				NumPinnedSectors, MaxResidentSectors);
		}
		if (NumPinnedWeightGroups > MaxResidentWeightCells && !bWarnedWeightPinOverBudget)
		{
			bWarnedWeightPinOverBudget = true;
			UE_LOGF(LogArcTerrain, Warning,
				"ArcTerrain: %d edit-pinned weight groups exceed tier 0's share of the weight slice "
				"budget (%d). Pins bypass the cap by design, but each weight slice costs as much VRAM as a "
				"whole sector of height; groups past the atlas slot count are refused and shade with "
				"the base layer.",
				NumPinnedWeightGroups, MaxResidentWeightCells);
		}

		const bool bMembershipWork = bWantedDirty
			|| State.PendingResidencyAdmissions.Num() > 0
			|| State.ReadyResidencyEvictions.Num() > 0;
		TSet<FArcTerrainSectorId> PreviousResidentSectors;
		PreviousResidentSectors.Reserve(State.ResidentSectors.Num());
		for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
		{
			PreviousResidentSectors.Add(Sector.Id);
		}
		if (!bMembershipWork)
		{
			OutNumSectorCells += State.SectorCells.Num();
			OutNumGrantedSectors += State.ResidentSectors.Num();
		}
		else
		{

		TArray<FArcTerrainResidentSector> NewResidentSectors;
		NewResidentSectors.Reserve(State.CachedWantedSectors.Num());
		TSet<FIntPoint> GrantedSectors;
		GrantedSectors.Reserve(State.CachedWantedSectors.Num());
		for (const FWantedSector& Sector : State.CachedWantedSectors)
		{
			NewResidentSectors.Add({ Sector.Id, Sector.DistSq, Sector.bPinned });
			if (Sector.Id.Tier == 0)
			{
				GrantedSectors.Add(Sector.Id.Key);
			}
		}

		TArray<FArcTerrainResidentWeightGroup> NewResidentWeightGroups;
		NewResidentWeightGroups.Reserve(State.CachedWantedWeightGroups.Num());
		for (const FWantedWeightGroup& Group : State.CachedWantedWeightGroups)
		{
			NewResidentWeightGroups.Add({ Group.Id.Tier, Group.Id.Key, Group.DistSq, Group.bPinned });
		}

		for (const int32 HotIndex : State.ResidentCells)
		{
			State.HotCells[HotIndex].bResident = 0;
		}

		TArray<FArcTerrainCellKey> ToEvict;
		OutNumSectorCells += State.SectorCells.Num();
		OutNumGrantedSectors += GrantedSectors.Num();
		for (const TPair<FIntPoint, TArray<int32>>& SectorPair : State.SectorCells)
		{
			if (GrantedSectors.Contains(SectorPair.Key))
			{
				continue;
			}
			for (const int32 HotIndex : SectorPair.Value)
			{
				++OutNumEvictCandidates;
				const FArcTerrainCellHot& Hot = State.HotCells[HotIndex];
				if (Hot.PinnedUntil > Now)
				{
					++OutNumHeldByPin;
					// Pinned or queued leavers stay until the hold clears; eviction is the exact-key wake.
					State.DeferredResidencyEvictions.Add(Hot.Key);
					continue;
				}
				if (Hot.bQueued != 0)
				{
					++OutNumHeldByQueue;
					State.DeferredResidencyEvictions.Add(Hot.Key);
					continue;
				}
				ToEvict.Add(Hot.Key);
			}
		}

		for (const FArcTerrainCellKey& ReadyKey : State.ReadyResidencyEvictions)
		{
			const FArcTerrainCellState* Cell = State.Cells.Find(ReadyKey);
			if (Cell == nullptr)
			{
				continue;
			}
			const FArcTerrainCellHot& Hot = State.Hot(*Cell);
			if (Hot.PinnedUntil > Now || Hot.bQueued != 0)
			{
				State.DeferredResidencyEvictions.Add(ReadyKey);
				continue;
			}
			const FIntPoint SectorKey = ArcTerrainAtlas::GroupKeyFromCell(ReadyKey, SectorShift);
			if (GrantedSectors.Contains(SectorKey))
			{
				continue;
			}
			ToEvict.AddUnique(ReadyKey);
		}
		State.ReadyResidencyEvictions.Reset();

		for (const FArcTerrainCellKey& EvictKey : ToEvict)
		{
			FArcTerrainCellState& Cell = State.Cells.FindChecked(EvictKey);
			Cell.TileSet.Release(RenderTargetPool);
			ArcTerrainSubsystemPrivate::ResetCellAtlasPublication(Cell);
			State.CompositeReadyCells.Remove(EvictKey);
			State.CompositeRetryCells.Remove(EvictKey);
			State.DeferredResidencyEvictions.Remove(EvictKey);
			State.RemoveCell(EvictKey);
			ArcTerrainDiag::NoteCellRemoved(EvictKey);
		}
		OutNumEvicted += ToEvict.Num();

		ArcTerrainCellCreationLocal_71a3::FHeightTileProbe HeightTileProbe(*Terrain, BaseHeightGrid);
#if WITH_EDITOR
		HeightTileProbe.SetRebakeQueue(State.PendingHeightTiles[0]);
#endif
		for (const FWantedSector& Sector : State.CachedWantedSectors)
		{
			if (Sector.Id.Tier != 0)
			{
				continue;
			}
			if (!PreviousResidentSectors.Contains(Sector.Id))
			{
				AdmitSectorContentCells(State, Sector.Id.Key, HeightTileProbe);
			}
		}
		for (const FArcTerrainCellKey& AdmitKey : State.PendingResidencyAdmissions)
		{
			const FIntPoint SectorKey = ArcTerrainAtlas::GroupKeyFromCell(AdmitKey, SectorShift);
			if (!GrantedSectors.Contains(SectorKey))
			{
				continue;
			}
			if (!State.Cells.Contains(AdmitKey) && !HeightTileProbe.IsTileAuthoritativeForCell(AdmitKey))
			{
				State.FindOrAddCell(AdmitKey);
				ArcTerrainDiag::NoteCellCreated(ArcTerrainDiag::ECellRail::Content, AdmitKey);
			}
			if (State.Cells.Contains(AdmitKey))
			{
				ArcTerrainResidencyIndex_c4e8::EnqueueCompositeReady(State, AdmitKey);
			}
		}
		State.PendingResidencyAdmissions.Reset();

		bool bMembershipChanged = State.ResidentSectors.Num() != NewResidentSectors.Num()
			|| State.ResidentWeightGroups.Num() != NewResidentWeightGroups.Num();
		bool bPublicationChanged = bMembershipChanged;
		bool bPinOrOrderChanged = bMembershipChanged;
		if (!bMembershipChanged)
		{
			for (int32 Index = 0; Index < NewResidentSectors.Num(); ++Index)
			{
				if (Index >= State.ResidentSectors.Num()
					|| State.ResidentSectors[Index].Id != NewResidentSectors[Index].Id)
				{
					bMembershipChanged = true;
					bPublicationChanged = true;
					bPinOrOrderChanged = true;
					break;
				}
				if (State.ResidentSectors[Index].bPinned != NewResidentSectors[Index].bPinned)
				{
					bPublicationChanged = true;
					bPinOrOrderChanged = true;
				}
				if (State.ResidentSectors[Index].DistSq != NewResidentSectors[Index].DistSq)
				{
					bPublicationChanged = true;
				}
			}
		}
		if (!bMembershipChanged)
		{
			for (int32 Index = 0; Index < NewResidentWeightGroups.Num(); ++Index)
			{
				if (Index >= State.ResidentWeightGroups.Num()
					|| State.ResidentWeightGroups[Index].Tier != NewResidentWeightGroups[Index].Tier
					|| State.ResidentWeightGroups[Index].Key != NewResidentWeightGroups[Index].Key)
				{
					bMembershipChanged = true;
					bPublicationChanged = true;
					bPinOrOrderChanged = true;
					break;
				}
				if (State.ResidentWeightGroups[Index].bPinned != NewResidentWeightGroups[Index].bPinned)
				{
					bPublicationChanged = true;
					bPinOrOrderChanged = true;
				}
				if (State.ResidentWeightGroups[Index].DistSq != NewResidentWeightGroups[Index].DistSq)
				{
					bPublicationChanged = true;
				}
			}
		}
		if (ToEvict.Num() > 0)
		{
			bMembershipChanged = true;
			bPublicationChanged = true;
			bPinOrOrderChanged = true;
		}

		State.ResidentSectors = MoveTemp(NewResidentSectors);
		State.ResidentWeightGroups = MoveTemp(NewResidentWeightGroups);

		// HotCells swap-removes; ResidentCells is rebuilt once after the mutating batch.
		State.ResidentCells.Reset();
		for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
		{
			if (Sector.Id.Tier != 0)
			{
				continue;
			}
			const TArray<int32>* SectorCellIndices = State.SectorCells.Find(Sector.Id.Key);
			if (SectorCellIndices != nullptr)
			{
				State.ResidentCells.Append(*SectorCellIndices);
			}
		}

		if (bMembershipChanged)
		{
			++State.ResidentSetRevision;
		}
		if (bPublicationChanged)
		{
			++State.AtlasSchedule.ResidencyPublicationRevision;
		}
		if (bPinOrOrderChanged)
		{
			++State.AtlasSchedule.DesiredAdmissionRevision;
		}

		if (State.ResidentCells.Num() > 0)
		{
			UpdateCellDistances(State.HotCells, State.ResidentCells, Config.CellSize(), TerrainTransform,
				State.ResidencySelectionCache.QuantizedPositions.Num() > 0
					? TConstArrayView<FVector>(State.ResidencySelectionCache.QuantizedPositions)
					: InvokerPositions);
		}
		for (const int32 HotIndex : State.ResidentCells)
		{
			State.HotCells[HotIndex].bResident = 1;
		}
		}

#if WITH_EDITORONLY_DATA
		TArray<FArcTerrainCellKey> ConsumedReady;
		for (const FArcTerrainCellKey& ReadyKey : State.CompositeReadyCells)
		{
			const FArcTerrainCellState* Cell = State.Cells.Find(ReadyKey);
			if (Cell == nullptr)
			{
				ConsumedReady.Add(ReadyKey);
				continue;
			}
			FArcTerrainCellHot& Hot = State.Hot(*Cell);
			if (Hot.bResident == 0)
			{
				continue;
			}
			++OutNumCompositeCandidates;
			InsertBoundedCandidate(Candidates, MaxCompositesPerFrame,
				FCompositeCandidate{ Terrain, &State, ReadyKey, Hot.InvokerDistSq, Hot.PinnedUntil > Now },
				FCompositeCandidateOrder());
		}
		for (const FCompositeCandidate& Candidate : Candidates)
		{
			if (Candidate.State == &State)
			{
				State.CompositeReadyCells.Remove(Candidate.Key);
			}
		}
		for (const FArcTerrainCellKey& ConsumedKey : ConsumedReady)
		{
			State.CompositeReadyCells.Remove(ConsumedKey);
		}
#else
		State.CompositeReadyCells.Reset();
#endif
	}

}

#if WITH_EDITOR
namespace ArcTerrainSubsystemPrivate
{
	void GatherBakeTileRefsForBounds(const AArcTerrain& Terrain, const FArcTerrainState& State,
		const FBox& LocalBounds, TArray<FArcTerrainBakeTileRef>& OutRefs)
	{
		OutRefs.Reset();

		const int32 WeightQuads = ResolveWeightQuads(Terrain.Config, State);

		TArray<int32> HeightShifts;
		ArcTerrain::ResolveAuthoredHeightTierShifts(Terrain, HeightShifts);
		for (int32 Tier = 0; Tier < HeightShifts.Num(); ++Tier)
		{
			FArcTerrainTileGrid Grid;
			if (!ArcTerrainTileBake::ResolveBakeTileGrid(Terrain, EArcTerrainRail::Height, Tier, WeightQuads, Grid))
			{
				continue;
			}
			TArray<FIntPoint> Keys;
			ArcTerrainTileBake::GatherTileKeysIntersectingBounds(Grid, LocalBounds, Keys);
			for (const FIntPoint& Key : Keys)
			{
				OutRefs.Add(FArcTerrainBakeTileRef{EArcTerrainRail::Height, Tier, Key});
			}
		}

		TArray<int32> WeightShifts;
		ArcTerrain::ResolveAuthoredWeightTierShifts(Terrain, WeightQuads, WeightShifts);
		for (int32 Tier = 0; Tier < WeightShifts.Num(); ++Tier)
		{
			FArcTerrainTileGrid Grid;
			if (!ArcTerrainTileBake::ResolveBakeTileGrid(Terrain, EArcTerrainRail::Weight, Tier, WeightQuads, Grid))
			{
				continue;
			}
			TArray<FIntPoint> Keys;
			ArcTerrainTileBake::GatherTileKeysIntersectingBounds(Grid, LocalBounds, Keys);
			for (const FIntPoint& Key : Keys)
			{
				OutRefs.Add(FArcTerrainBakeTileRef{EArcTerrainRail::Weight, Tier, Key});
			}
		}

		FArcTerrainTileGrid DataGrid;
		if (ArcTerrainTileBake::ResolveBakeTileGrid(Terrain, EArcTerrainRail::Data, 0, WeightQuads, DataGrid))
		{
			TArray<FIntPoint> Keys;
			ArcTerrainTileBake::GatherTileKeysIntersectingBounds(DataGrid, LocalBounds, Keys);
			for (const FIntPoint& Key : Keys)
			{
				OutRefs.Add(FArcTerrainBakeTileRef{EArcTerrainRail::Data, 0, Key});
			}
		}
	}
}
#endif

void UArcTerrainSubsystem::MarkBoundsDirty(FArcTerrainState& State, const AArcTerrain& Terrain,
	const FBox& LocalBounds)
{
	if (!LocalBounds.IsValid)
	{
		return;
	}

	const FArcTerrainConfig& Config = Terrain.Config;

	// THE COLLISION PATCHES' DIRTY SIGNAL — the same role Cell.Version used to play for collision, on
	// the lattice that now owns it. Only EXISTING patches are bumped: one the ring has not reached yet
	// computes its key from scratch when it is created, so a serial on a phantom entry would buy nothing
	// and would make the registry a creation rail for every edit anywhere on the terrain. Not
	// apron-widened — a patch heightfield has no apron, and the edge-inclusive rule already gives a
	// boundary edit both neighbours.
	TArray<FArcTerrainCollisionKey> DirtyPatches;
	ArcTerrain::CollisionPatchesIntersectingLocalBounds(
		LocalBounds, Terrain.CollisionPatchSize(), DirtyPatches);
	for (FArcTerrainCollisionKey PatchKey : DirtyPatches)
	{
		if (FArcTerrainCollisionPatch* Patch = State.CollisionPatches.Find(PatchKey))
		{
			++Patch->DirtySerial;
			if (State.CollisionWindowUnion.Contains(PatchKey)
				|| State.CollisionBootstrapKeys.Contains(PatchKey))
			{
				ArcTerrainCollisionIndex_e7b2::EnqueueReady(State, Terrain, PatchKey);
			}
		}
	}

	TArray<FArcTerrainCellKey> IntersectedCells;
	ArcTerrain::CellsIntersectingLocalBounds(LocalBounds, Config, IntersectedCells);
	const int32 SectorShift = State.SectorCellsShift != INDEX_NONE
		? State.SectorCellsShift
		: State.IndexedSectorShift;
	TSet<FIntPoint> GrantedTier0;
	for (const FArcTerrainResidentSector& Sector : State.ResidentSectors)
	{
		if (Sector.Id.Tier == 0)
		{
			GrantedTier0.Add(Sector.Id.Key);
		}
	}
	for (FArcTerrainCellKey CellKey : IntersectedCells)
	{
		const bool bCreated = !State.Cells.Contains(CellKey);
		FArcTerrainCellState& Cell = State.FindOrAddCell(CellKey);
		if (bCreated && ArcTerrainDiag::IsOn())
		{
			ArcTerrainDiag::NoteCellCreated(ArcTerrainDiag::ECellRail::Bounds, CellKey);
		}
		FArcTerrainCellHot& Hot = State.Hot(Cell);
		Hot.bDirty = 1;
		Cell.bCompositeResolveRetryPending = false;
		Cell.CompositeResolveRetryAfter = 0.0;
		Cell.CompositeResolveRetryCount = 0;
		ArcTerrainResidencyIndex_c4e8::EnqueueCompositeReady(State, CellKey);
		if (SectorShift != INDEX_NONE)
		{
			const FIntPoint SectorKey = ArcTerrainAtlas::GroupKeyFromCell(CellKey, SectorShift);
			if (GrantedTier0.Contains(SectorKey))
			{
				State.PendingResidencyAdmissions.Add(CellKey);
			}
		}
	}
	FArcTerrainTileAuthorityEvent BoundsEvent;
	BoundsEvent.Reason = EArcTerrainTileAuthorityReason::StampBoundsChanged;
	PublishLocalTileAuthorityEvent(State, BoundsEvent);

	// THE WORK QUEUE. This is the only moment the system knows the exact cell set an edit moved; the
	// bake phase consumes it instead of re-deriving it from a scan. Widened by one cell because the
	// composite's apron carries the NEIGHBOUR's global texel indices — an edit at a cell edge moves
	// texels that belong to the cell next door.
	const FBox ApronBounds = LocalBounds.ExpandBy(FVector(Config.CellSize(), Config.CellSize(), 0.0));
	TArray<FArcTerrainCellKey> QueuedCells;
	ArcTerrain::CellsIntersectingLocalBounds(ApronBounds, Config, QueuedCells);
	const int32 DirtyQueueNumBefore = State.DirtyQueue.Num();
	for (FArcTerrainCellKey CellKey : QueuedCells)
	{
		State.EnqueueCell(CellKey);
	}
	if (State.DirtyQueue.Num() > DirtyQueueNumBefore)
	{
		TerrainsWithDirtyCells.Add(const_cast<AArcTerrain*>(&Terrain));
	}

	// THE LIGHTING PATCHES' DIRTY SIGNAL, off the SAME apron-widened set and, like the collision rail,
	// bumping only EXISTING entries.
	for (FArcTerrainCellKey CellKey : QueuedCells)
	{
		if (FArcTerrainLightingPatch* LightingPatch = State.LightingPatches.Find(CellKey))
		{
			++LightingPatch->DirtySerial;
			if (State.LightingWindowUnion.Contains(CellKey))
			{
				ArcTerrainLightingIndex_6b19::EnqueueReady(State, Terrain, CellKey);
			}
		}
	}

	// THE FAR BLOCKS' DIRTY SIGNAL, off the SAME apron-widened set mapped onto the block lattice, and
	// like both rails above bumping only EXISTING entries.
	const int32 FarBlockCells = FMath::Max(Terrain.LightingFarBlockCells, 1);
	TSet<FArcTerrainFarBlockKey> DirtyFarBlocks;
	for (FArcTerrainCellKey CellKey : QueuedCells)
	{
		DirtyFarBlocks.Add(FArcTerrainFarBlockKey(
			static_cast<int32>(ArcTerrain::FloorDiv(static_cast<int64>(CellKey.X), FarBlockCells)),
			static_cast<int32>(ArcTerrain::FloorDiv(static_cast<int64>(CellKey.Y), FarBlockCells))));
	}
	for (const FArcTerrainFarBlockKey& BlockKey : DirtyFarBlocks)
	{
		if (FArcTerrainFarBlockPatch* FarPatch = State.FarBlockPatches.Find(BlockKey))
		{
			++FarPatch->DirtySerial;
			if (State.FarBlockWindowUnion.Contains(BlockKey))
			{
				ArcTerrainFarBlockIndex_2c08::EnqueueReady(State, Terrain, BlockKey);
			}
		}
	}
}

void UArcTerrainSubsystem::PinCellsInBounds(FArcTerrainState& State, const FArcTerrainConfig& Config,
	const FBox& LocalBounds, double InPinnedUntil)
{
	if (!LocalBounds.IsValid)
	{
		return;
	}

	// Same cell clamp as MarkBoundsDirty: only cells the footprint actually intersects pin.
	TArray<FArcTerrainCellKey> IntersectedCells;
	ArcTerrain::CellsIntersectingLocalBounds(LocalBounds, Config, IntersectedCells);
	for (FArcTerrainCellKey CellKey : IntersectedCells)
	{
		const bool bCreated = ArcTerrainDiag::IsOn() && !State.Cells.Contains(CellKey);
		FArcTerrainCellHot& Hot = State.Hot(State.FindOrAddCell(CellKey));
		if (bCreated)
		{
			ArcTerrainDiag::NoteCellCreated(ArcTerrainDiag::ECellRail::Pin, CellKey);
		}
		const bool bNewPin = !State.PinnedCells.Contains(CellKey);
		Hot.PinnedUntil = FMath::Max(Hot.PinnedUntil, InPinnedUntil);
		State.PinnedCells.Add(CellKey);
		if (bNewPin)
		{
			State.bPinMembershipDirty = true;
			if (State.NextPinExpiry <= 0.0 || InPinnedUntil < State.NextPinExpiry)
			{
				State.NextPinExpiry = InPinnedUntil;
			}
		}
	}
}
