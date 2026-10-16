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

#if WITH_EDITOR
void UArcTerrainSubsystem::RequestBakeWake()
{
	bBakeWakeRequested.store(true, std::memory_order_release);
}

void UArcTerrainSubsystem::EnqueueBakeCompletion(FArcTerrainBakeCompletionToken&& Token)
{
	if (bBakeShuttingDown.load(std::memory_order_acquire))
	{
		return;
	}
	BakeCompletionQueue.Enqueue(MoveTemp(Token));
	RequestBakeWake();
}

void UArcTerrainSubsystem::ArmBakeRetryTimer(double RetryAfter)
{
	if (bBakeShuttingDown.load(std::memory_order_acquire))
	{
		return;
	}
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (RetryAfter <= Now)
	{
		RequestBakeWake();
		return;
	}
	if (BakeRetryTimerHandle.IsValid() && ArmedBakeRetryAfter > 0.0 && ArmedBakeRetryAfter <= RetryAfter)
	{
		return;
	}
	ArmedBakeRetryAfter = RetryAfter;
	const float DelaySeconds = static_cast<float>(RetryAfter - Now);
	World->GetTimerManager().SetTimer(
		BakeRetryTimerHandle,
		FTimerDelegate::CreateUObject(this, &UArcTerrainSubsystem::HandleBakeRetryTimer),
		DelaySeconds,
		false);
}

void UArcTerrainSubsystem::ClearBakeRetryTimer()
{
	ArmedBakeRetryAfter = 0.0;
	UWorld* World = GetWorld();
	if (World != nullptr && BakeRetryTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(BakeRetryTimerHandle);
	}
	BakeRetryTimerHandle.Invalidate();
}

bool UArcTerrainSubsystem::CanRunTileBake(const UWorld* World) const
{
	const bool bGameWorld = World != nullptr && World->IsGameWorld();
	return GIsEditor && !bGameWorld && World != nullptr && World->WorldType == EWorldType::Editor;
}

void UArcTerrainSubsystem::HandleBakeRetryTimer()
{
	ClearBakeRetryTimer();
	if (!CanRunTileBake(GetWorld()))
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
		{
			for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				TerrainPair.Value.TileRails[RailIndex][Tier].ReleaseDeferred(Now);
			}
		}
	}
	RequestBakeWake();
	RefreshBakeRetryTimer();
}

void UArcTerrainSubsystem::RefreshBakeRetryTimer()
{
	double EarliestRetryAfter = 0.0;
	bool bHasRetry = false;
	for (const TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
		{
			for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				double RailRetryAfter = 0.0;
				if (TerrainPair.Value.TileRails[RailIndex][Tier].TryGetNextRetryAfter(RailRetryAfter))
				{
					if (!bHasRetry || RailRetryAfter < EarliestRetryAfter)
					{
						EarliestRetryAfter = RailRetryAfter;
						bHasRetry = true;
					}
				}
			}
		}
	}
	if (!bHasRetry)
	{
		ClearBakeRetryTimer();
		return;
	}
	ArmBakeRetryTimer(EarliestRetryAfter);
}

void UArcTerrainSubsystem::RefreshBakeProgressSnapshot()
{
	BakeProgressSnapshot = FBakeProgressSnapshot();
	NumDataPendingTiles = 0;
	NumDataInFlight = 0;
	for (const TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		const FArcTerrainState& State = TerrainPair.Value;
		NumDataPendingTiles += State.PendingDataTiles.Num();
		for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
		{
			for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				const FArcTerrainTileRailState& RailState = State.TileRails[RailIndex][Tier];
				BakeProgressSnapshot.Probes[RailIndex][Tier] += RailState.ProbesInFlight;
				BakeProgressSnapshot.Evaluations[RailIndex][Tier] += RailState.EvaluationsInFlight;
				BakeProgressSnapshot.Writes[RailIndex][Tier] += RailState.WritesInFlightCount;
				BakeProgressSnapshot.Deferred[RailIndex][Tier] += RailState.RetriesDeferred;
				if (RailIndex == static_cast<int32>(EArcTerrainRail::Data) && Tier == 0)
				{
					NumDataInFlight += RailState.Pending.Num() + RailState.WritesInFlightCount;
				}
			}
		}
		for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
		{
			BakeProgressSnapshot.Queue[static_cast<int32>(EArcTerrainRail::Height)][Tier] +=
				State.PendingHeightTiles[Tier].Num();
			BakeProgressSnapshot.Queue[static_cast<int32>(EArcTerrainRail::Weight)][Tier] +=
				State.PendingWeightTiles[Tier].Num();
		}
		BakeProgressSnapshot.Queue[static_cast<int32>(EArcTerrainRail::Data)][0] += State.PendingDataTiles.Num();
	}
	bBakeProgressSnapshotValid = true;
}

void UArcTerrainSubsystem::DrainBakeCompletions()
{
	FArcTerrainBakeCompletionToken Token;
	while (BakeCompletionQueue.Dequeue(Token))
	{
		FArcTerrainState* State = Terrains.Find(Token.Terrain);
		if (State == nullptr || !ArcTerrainTileAccess::IsValidTier(Token.Ref.Tier))
		{
			continue;
		}
		const int32 RailIndex = static_cast<int32>(Token.Ref.Rail);
		if (RailIndex < 0 || RailIndex >= ArcTerrain::NumTileRails)
		{
			continue;
		}
		FArcTerrainTileRailState& RailState = State->TileRails[RailIndex][Token.Ref.Tier];
		const FTileBakeLedgerEntry* Ledger = TileBakeLedger.Find(Token.ReservationId);
		if (Ledger == nullptr || Ledger->bAbandoned)
		{
			continue;
		}
		const FArcTerrainPendingTileBake* PendingBake = RailState.Pending.Find(Token.Ref.Key);
		if (PendingBake == nullptr || PendingBake->ReservationId != Token.ReservationId)
		{
			continue;
		}
		if (!State->PendingTiles(Token.Ref.Rail, Token.Ref.Tier).Contains(Token.Ref.Key))
		{
			continue;
		}
		if (RailState.WritesInFlight.Contains(Token.Ref.Key))
		{
			continue;
		}
		RailState.EnqueueReady(Token.Ref.Key);
	}
}

bool UArcTerrainSubsystem::HasImmediateBakeWork() const
{
	if (bBakeWakeRequested.load(std::memory_order_acquire))
	{
		return true;
	}
	if (!BakeCompletionQueue.IsEmpty())
	{
		return true;
	}
	if (bBakeStoppedForBudget)
	{
		return true;
	}
	for (const TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
	{
		const FArcTerrainState& State = TerrainPair.Value;
		if (State.PendingTileInvalidations.Num() > 0 || State.bDerivedGridRebuildPending)
		{
			return true;
		}
		for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
		{
			for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				if (State.TileRails[RailIndex][Tier].NumReady() > 0)
				{
					return true;
				}
			}
		}
	}
	return false;
}
#endif

void UArcTerrainSubsystem::TickCellBakeLifecycle(UWorld* World,
	TConstArrayView<FVector> InvokerPositions)
{
	(void)InvokerPositions;
	// DrainCellDirtyQueues must still run when bake is asleep: bQueued is residency's eviction hold.
	DrainCellDirtyQueues();

#if WITH_EDITOR
	DrainBakeCompletions();
	if (ArmedBakeRetryAfter > 0.0 && FPlatformTime::Seconds() >= ArmedBakeRetryAfter)
	{
		HandleBakeRetryTimer();
	}
	const bool bCanBuild = CanRunTileBake(World);
	const bool bImmediateBakeWork = bCanBuild && HasImmediateBakeWork();
	bBakeWakeRequested.store(false, std::memory_order_release);

	if (bImmediateBakeWork)
	{
		DrainAbandonedTileBakeReservations();
		const UArcTerrainSettings* Settings = GetDefault<UArcTerrainSettings>();
		const int32 SettleTicks = FMath::Max(Settings->SettleTicks, 1);
		const double HeavyDeadline =
			FPlatformTime::Seconds() + static_cast<double>(Settings->BakeBudgetMs) / 1000.0;
		int32 NumHeavySteps = 0;
		int32 NumBakeCompletions = 0;
		int32 NumBakeLaneScans = 0;
		int32 NumBakeCandidateExaminations = 0;
		bool bStoppedForBudget = false;

		for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
		{
			AArcTerrain* LiveTerrain = TerrainPair.Key.Get();
			if (LiveTerrain == nullptr)
			{
				continue;
			}
			FlushPendingDerivedGridRebuild(*LiveTerrain, TerrainPair.Value);
			ArcTerrainTileSeedLocal_c9f2::FlushTileInvalidations(*LiveTerrain, TerrainPair.Value);
		}

		RefreshBakeProgressSnapshot();
		if (!bDataBakePassActive && HasPendingDataTileWork())
		{
			bDataBakePassActive = true;
			SourceCache.SetCpuStatsEnabled(ArcTerrainDataMaps::IsBakeStatsEnabled());
		}

		TArray<AArcTerrain*, TInlineAllocator<4>> TerrainsToTick;
		TerrainsToTick.Reserve(Terrains.Num());
		for (TPair<TWeakObjectPtr<AArcTerrain>, FArcTerrainState>& TerrainPair : Terrains)
		{
			AArcTerrain* Terrain = TerrainPair.Key.Get();
			if (Terrain == nullptr)
			{
				InvalidateCBTBaseMesh(TerrainPair.Value);
				continue;
			}
			TerrainsToTick.Add(Terrain);
		}

		TerrainsToTick.Sort([](const AArcTerrain& Left, const AArcTerrain& Right)
		{
			return Left.GetPathName() < Right.GetPathName();
		});
		if (TerrainsToTick.IsEmpty())
		{
			NextBakeTerrain.Reset();
		}

		const int32 NumTerrainsToTick = TerrainsToTick.Num();
		int32 StartTerrainIndex = 0;
		if (AArcTerrain* NextTerrain = NextBakeTerrain.Get())
		{
			const int32 SavedTerrainIndex = TerrainsToTick.IndexOfByKey(NextTerrain);
			if (SavedTerrainIndex != INDEX_NONE)
			{
				StartTerrainIndex = SavedTerrainIndex;
			}
		}
		for (int32 TerrainOffset = 0; TerrainOffset < NumTerrainsToTick; ++TerrainOffset)
		{
			const int32 TerrainIndex = (StartTerrainIndex + TerrainOffset) % NumTerrainsToTick;
			AArcTerrain* Terrain = TerrainsToTick[TerrainIndex];
			FArcTerrainState* StatePtr = Terrains.Find(Terrain);
			check(StatePtr != nullptr);
			const FArcTerrainConfig& Config = Terrain->Config;
			if (Config.QuadsPerCell() < 1 || Config.CellSize() <= 0.0)
			{
				InvalidateCBTBaseMesh(*StatePtr);
				continue;
			}
			FArcTerrainState& State = *StatePtr;
			const FString TileDir = ArcTerrainTileNames::MakeTileDirForWrite(*Terrain);
			if (!TileDir.IsEmpty())
			{
				const int32 LaneScansBeforeTerrain = NumBakeLaneScans;
				const int32 CandidateExaminationsBeforeTerrain = NumBakeCandidateExaminations;
				TickTerrainTileBakes(*Terrain, State, TileDir, SettleTicks,
					NumHeavySteps, NumBakeCompletions, NumBakeLaneScans, NumBakeCandidateExaminations,
					HeavyDeadline, bStoppedForBudget);
				if (NumBakeLaneScans > LaneScansBeforeTerrain
					|| NumBakeCandidateExaminations > CandidateExaminationsBeforeTerrain)
				{
					NextBakeTerrain = TerrainsToTick[(TerrainIndex + 1) % NumTerrainsToTick];
				}
			}
		}

		bBakeStoppedForBudget = bStoppedForBudget;
		RefreshBakeProgressSnapshot();
		RefreshBakeRetryTimer();
		if (bDataBakePassActive && !HasPendingDataTileWork())
		{
			EndDataBakePass();
		}
		if (ArcTerrainDiag::GBakeProgress != 0)
		{
			bBakeProgressSessionActive = true;
		}
	}
	else
	{
		RefreshBakeProgressSnapshot();
		if (bDataBakePassActive && !HasPendingDataTileWork())
		{
			EndDataBakePass();
		}
		if (ArcTerrainDiag::GBakeProgress == 0)
		{
			bBakeProgressSessionActive = false;
		}
	}
#endif
}

#if WITH_EDITOR
bool UArcTerrainSubsystem::CanReserveTileBakeWork(EArcTerrainBakeReservationKind Kind, int64 ReservedBytes,
	const UArcTerrainSettings& Settings) const
{
	const int32 Limit = Kind == EArcTerrainBakeReservationKind::Probe
		? Settings.MaxTileBakeDDCProbes
		: (Kind == EArcTerrainBakeReservationKind::Evaluation
			? Settings.MaxTileBakeWorkerEvaluations
			: Settings.MaxTileBakeBackgroundWrites);
	const int32 InFlight = Kind == EArcTerrainBakeReservationKind::Probe
		? TileBakeProbesInFlight
		: (Kind == EArcTerrainBakeReservationKind::Evaluation
			? TileBakeEvaluationsInFlight
			: TileBakeWritesInFlight);
	const int64 ByteLimit = static_cast<int64>(FMath::Max(Settings.MaxTileBakeInFlightMB, 1)) * 1024 * 1024;
	return InFlight < FMath::Max(Limit, 1) && ReservedBytes <= ByteLimit
		&& TileBakeReservedBytes + ReservedBytes <= ByteLimit;
}

bool UArcTerrainSubsystem::CanReserveTileBakeWriteAfterEvaluation(uint64 EvaluationReservationId,
	int64 WriteReservedBytes, const UArcTerrainSettings& Settings) const
{
	const FTileBakeLedgerEntry* Evaluation = TileBakeLedger.Find(EvaluationReservationId);
	if (Evaluation == nullptr || Evaluation->Kind != EArcTerrainBakeReservationKind::Evaluation)
	{
		return false;
	}
	return CanReserveTileBakeWriteAfterReservation(EvaluationReservationId, WriteReservedBytes, Settings);
}

bool UArcTerrainSubsystem::CanReserveTileBakeWriteAfterReservation(uint64 ReservationId,
	int64 WriteReservedBytes, const UArcTerrainSettings& Settings) const
{
	const FTileBakeLedgerEntry* Reservation = TileBakeLedger.Find(ReservationId);
	if (Reservation == nullptr)
	{
		return false;
	}
	const int64 ByteLimit = static_cast<int64>(FMath::Max(Settings.MaxTileBakeInFlightMB, 1)) * 1024 * 1024;
	return TileBakeWritesInFlight < FMath::Max(Settings.MaxTileBakeBackgroundWrites, 1)
		&& WriteReservedBytes <= ByteLimit
		&& TileBakeReservedBytes - Reservation->ReservedBytes + WriteReservedBytes <= ByteLimit;
}

bool UArcTerrainSubsystem::IsTileWriteFenced(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
	FIntPoint TileKey) const
{
	const FString Fence = FString::Printf(TEXT("%s|%d|%d|%d|%d"), *Terrain.GetPathName(),
		static_cast<int32>(Rail), Tier, TileKey.X, TileKey.Y);
	return ActiveTileWriteFences.Contains(Fence);
}

uint64 UArcTerrainSubsystem::AcquireTileBakeWriteReservation(const AArcTerrain& Terrain, EArcTerrainRail Rail,
	int32 Tier, FIntPoint TileKey, int64 ReservedBytes)
{
	const FString Fence = FString::Printf(TEXT("%s|%d|%d|%d|%d"), *Terrain.GetPathName(),
		static_cast<int32>(Rail), Tier, TileKey.X, TileKey.Y);
	check(!ActiveTileWriteFences.Contains(Fence));
	const uint64 ReservationId = AcquireTileBakeReservation(EArcTerrainBakeReservationKind::Write, ReservedBytes,
		TFunction<bool()>(), Rail == EArcTerrainRail::Data);
	ActiveTileWriteFences.Add(Fence, ReservationId);
	FTileBakeLedgerEntry* Entry = TileBakeLedger.Find(ReservationId);
	check(Entry != nullptr);
	Entry->WriteFence = Fence;
	return ReservationId;
}

uint64 UArcTerrainSubsystem::AcquireTileBakeReservation(EArcTerrainBakeReservationKind Kind, int64 ReservedBytes,
	TFunction<bool()>&& IsComplete, bool bData)
{
	check(ReservedBytes >= 0);
	uint64 ReservationId = NextTileBakeReservationId++;
	if (ReservationId == 0)
	{
		ReservationId = NextTileBakeReservationId++;
	}
	FTileBakeLedgerEntry& Entry = TileBakeLedger.Add(ReservationId);
	Entry.Kind = Kind;
	Entry.ReservedBytes = ReservedBytes;
	Entry.bData = bData;
	Entry.IsComplete = MoveTemp(IsComplete);
	if (Kind == EArcTerrainBakeReservationKind::Probe)
	{
		++TileBakeProbesInFlight;
	}
	else if (Kind == EArcTerrainBakeReservationKind::Evaluation)
	{
		++TileBakeEvaluationsInFlight;
	}
	else
	{
		++TileBakeWritesInFlight;
	}
	TileBakeReservedBytes += ReservedBytes;
	return ReservationId;
}

void UArcTerrainSubsystem::ReleaseTileBakeReservation(uint64 ReservationId)
{
	FTileBakeLedgerEntry* Entry = TileBakeLedger.Find(ReservationId);
	if (Entry == nullptr)
	{
		return;
	}
	check(TileBakeReservedBytes >= Entry->ReservedBytes);
	TileBakeReservedBytes -= Entry->ReservedBytes;
	if (Entry->Kind == EArcTerrainBakeReservationKind::Probe)
	{
		check(TileBakeProbesInFlight > 0);
		--TileBakeProbesInFlight;
	}
	else if (Entry->Kind == EArcTerrainBakeReservationKind::Evaluation)
	{
		check(TileBakeEvaluationsInFlight > 0);
		--TileBakeEvaluationsInFlight;
	}
	else
	{
		check(TileBakeWritesInFlight > 0);
		--TileBakeWritesInFlight;
	}
	if (!Entry->WriteFence.IsEmpty())
	{
		uint64* FenceReservation = ActiveTileWriteFences.Find(Entry->WriteFence);
		if (FenceReservation != nullptr && *FenceReservation == ReservationId)
		{
			ActiveTileWriteFences.Remove(Entry->WriteFence);
		}
	}
	if (Entry->bAbandoned && Entry->bData)
	{
		check(NumAbandonedDataReservations > 0);
		--NumAbandonedDataReservations;
	}
	TileBakeLedger.Remove(ReservationId);
}

void UArcTerrainSubsystem::AbandonTileBakeReservation(uint64 ReservationId)
{
	if (FTileBakeLedgerEntry* Entry = TileBakeLedger.Find(ReservationId))
	{
		Entry->bAbandoned = true;
	}
}

void UArcTerrainSubsystem::AbandonPendingTileBake(FArcTerrainTileRailState& RailState, FIntPoint TileKey)
{
	if (!RailState.Pending.Contains(TileKey))
	{
		return;
	}
	FArcTerrainPendingTileBake Pending = RailState.TakePending(TileKey);
	AbandonTileBakeReservation(Pending.ReservationId);
	if (const FTileBakeLedgerEntry* Entry = TileBakeLedger.Find(Pending.ReservationId))
	{
		if (Entry->bData)
		{
			++NumAbandonedDataReservations;
		}
	}
}

void UArcTerrainSubsystem::AbandonRailPendingTileBakes(FArcTerrainTileRailState& RailState)
{
	TArray<FIntPoint> TileKeys;
	RailState.Pending.GetKeys(TileKeys);
	for (FIntPoint TileKey : TileKeys)
	{
		AbandonPendingTileBake(RailState, TileKey);
	}
}

void UArcTerrainSubsystem::DrainAbandonedTileBakeReservations()
{
	TArray<uint64> CompletedReservations;
	for (const TPair<uint64, FTileBakeLedgerEntry>& Pair : TileBakeLedger)
	{
		if (Pair.Value.bAbandoned && Pair.Value.IsComplete && Pair.Value.IsComplete())
		{
			CompletedReservations.Add(Pair.Key);
		}
	}
	for (uint64 ReservationId : CompletedReservations)
	{
		ReleaseTileBakeReservation(ReservationId);
	}
}

void UArcTerrainSubsystem::InvalidateRailWrites(FArcTerrainTileRailState& RailState)
{
	RailState.InvalidateWrites();
}

bool UArcTerrainSubsystem::HasPendingDataTileWork() const
{
	return NumDataPendingTiles > 0 || NumDataInFlight > 0 || NumAbandonedDataReservations > 0;
}

void UArcTerrainSubsystem::EndDataBakePass()
{
	if (SourceCache.IsCpuStatsEnabled())
	{
		UE_LOGF(LogArcTerrain, Log,"ArcTerrainSourceCacheStats %ls", *SourceCache.DescribeCpuStats());
		SourceCache.SetCpuStatsEnabled(false);
	}
	SourceCache.UnpinAllCpuFloats();
	bDataBakePassActive = false;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
namespace ArcTerrainTileWrite_8a3f
{
	int64 EstimateTilePayloadBytes(EArcTerrainRail Rail, const FArcTerrainTileGrid& Grid,
		const FArcTerrainConfig& Config)
	{
		const int64 Texels = static_cast<int64>(Grid.Resolution()) * Grid.Resolution();
		if (Rail == EArcTerrainRail::Height)
		{
			return Texels * sizeof(float);
		}
		if (Rail == EArcTerrainRail::Weight)
		{
			const int32 ChannelCount = Config.ChannelSet != nullptr ? Config.ChannelSet->Channels.Num() : 1;
			return Texels * FMath::Max(ChannelCount, 1);
		}
		return Texels * static_cast<int32>(EArcTerrainDataMap::Count);
	}

	int64 EstimateEvaluationBytes(EArcTerrainRail Rail, const FArcTerrainTileGrid& Grid,
		const FArcTerrainConfig& Config)
	{
		return EstimateTilePayloadBytes(Rail, Grid, Config) * 2;
	}

	bool IsBudgetExhausted(int32 NumSteps, double HeavyDeadline)
	{
		return NumSteps > 0 && FPlatformTime::Seconds() >= HeavyDeadline;
	}

	FString MakeTilePath(const FString& TileDir, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
	{
		const FString FileName = ArcTerrainTileNames::MakeTileFileName(Rail, Tier, TileKey);
		return FileName.IsEmpty() ? FString() : FPaths::Combine(TileDir, FileName);
	}


	void LaunchTileWrite(FString&& TilePath, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, FArcTerrainTileHeader&& Header, TArray<uint8>&& Body,
		TFunction<void(bool)>&& OnCompleted)
	{
		UE::Tasks::Launch(UE_SOURCE_LOCATION,
			[TilePath = MoveTemp(TilePath), Rail, Tier,
				TileKey, Header = MoveTemp(Header), Body = MoveTemp(Body),
				OnCompleted = MoveTemp(OnCompleted)]() mutable
			{
				const double WriteStart = FPlatformTime::Seconds();
				const int64 FileBytes = ArcTerrainTileIO::SaveTile(TilePath, Header, Body);
				const double WriteSeconds = FPlatformTime::Seconds() - WriteStart;
				const bool bWritten = FileBytes > 0;
				AsyncTask(ENamedThreads::GameThread,
					[Rail, Tier, TileKey, bWritten, WriteSeconds, FileBytes,
						OnCompleted = MoveTemp(OnCompleted)]()
					{
						ArcTerrainDiag::NoteTileWritten(Rail, Tier, TileKey, FileBytes, WriteSeconds,
							bWritten);
						OnCompleted(bWritten);
					});
			},
			UE::Tasks::ETaskPriority::BackgroundNormal);
	}

	void WriteWeightTile(const FString& TileDir, int32 Tier, FIntPoint TileKey,
		const FArcTerrainTileGrid& Grid,
		TConstArrayView<uint32> PlaneIds, TArray<uint8>&& Body, TFunction<void(bool)>&& OnCompleted)
	{
		LaunchTileWrite(MakeTilePath(TileDir, EArcTerrainRail::Weight, Tier, TileKey),
			EArcTerrainRail::Weight, Tier, TileKey,
			ArcTerrainTileFormat::MakeWeightHeader(Grid, PlaneIds), MoveTemp(Body), MoveTemp(OnCompleted));
	}

	/** Height and weight worker tasks already own immutable headers and bodies. Keep the path and the
	 *  publication callback on the game-thread authority, but do not repeat their CPU finalization here. */
	void WriteFinalizedTile(const FString& TileDir, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		FArcTerrainTileHeader&& Header, TArray<uint8>&& Body, TFunction<void(bool)>&& OnCompleted)
	{
		LaunchTileWrite(MakeTilePath(TileDir, Rail, Tier, TileKey), Rail, Tier, TileKey,
			MoveTemp(Header), MoveTemp(Body), MoveTemp(OnCompleted));
	}

	// The height rail's write: its header transports the tile's OWN quantization window, so the uint16
	// body decodes back into the range this tile actually covers.
	void WriteHeightTile(const FString& TileDir, int32 Tier, FIntPoint TileKey,
		const FArcTerrainTileGrid& Grid, float QuantMin, float QuantMax, TArray<uint8>&& Body,
		TFunction<void(bool)>&& OnCompleted)
	{
		LaunchTileWrite(MakeTilePath(TileDir, EArcTerrainRail::Height, Tier, TileKey),
			EArcTerrainRail::Height, Tier, TileKey,
			ArcTerrainTileFormat::MakeHeightHeader(Grid, QuantMin, QuantMax), MoveTemp(Body),
			MoveTemp(OnCompleted));
	}

	// The data rail's write: its header additionally transports one quantization window per plane, so a
	// mask whose values are signed or above unit survives the byte encoding.
	void WriteDataTile(const FString& TileDir, FIntPoint TileKey,
		const FArcTerrainTileGrid& Grid, TConstArrayView<uint8> DataMapIds, TConstArrayView<float> QuantMin,
		TConstArrayView<float> QuantMax, TArray<uint8>&& Body, TFunction<void(bool)>&& OnCompleted)
	{
		LaunchTileWrite(MakeTilePath(TileDir, EArcTerrainRail::Data, 0, TileKey),
			EArcTerrainRail::Data, 0, TileKey,
			ArcTerrainTileFormat::MakeDataHeader(Grid, DataMapIds, QuantMin, QuantMax), MoveTemp(Body),
			MoveTemp(OnCompleted));
	}

	// A tile whose rail now resolves to no plane at all: the file it left behind would otherwise keep
	// reading as valid content for an area that has none. The manifest entry goes only once the file is
	// actually gone — an entry dropped over a surviving file is a manifest that disowns real content, and
	// the next RebuildFromDisk would put the entry straight back.
	bool DropTile(const AArcTerrain& Terrain, const FString& TileDir, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey)
	{
		// The disk decides, NOT HasTile: that answers "is there a tile this configuration can read", which
		// is false for a file written on another lattice — exactly the file the drop has to remove. Delete
		// on an absent file succeeds, and NotifyTileDropped is a no-op for a key the manifest never named.
		const FString TilePath = MakeTilePath(TileDir, Rail, Tier, TileKey);
		if (!IFileManager::Get().Delete(*TilePath, /*RequireExists=*/false, /*EvenIfReadOnly=*/true,
			/*Quiet=*/true))
		{
			UE_LOGF(LogArcTerrain, Error,"Tile drop failed: cannot delete '%ls'.", *TilePath);
			return false;
		}
		ArcTerrainTileAccess::NotifyTileDropped(Terrain, Rail, Tier, TileKey);
		return true;
	}

	void HeightQuantWindow(TConstArrayView<float> Heights, float& OutMin, float& OutMax)
	{
		OutMin = 0.0f;
		OutMax = 0.0f;
		if (Heights.Num() <= 0)
		{
			return;
		}

		OutMin = Heights[0];
		OutMax = Heights[0];
		for (float Height : Heights)
		{
			OutMin = FMath::Min(OutMin, Height);
			OutMax = FMath::Max(OutMax, Height);
		}
	}

	/** Pure worker-side equivalent of WriteWeightTilePlanes' zero/base-plane filtering and file payload
	 *  assembly. Channel hashes are captured while gathering on the game thread, so this touches neither
	 *  UArcTerrainChannelSet nor any UObject. */
	void FinalizeWeightBakeResult(FArcTerrainTileBakeResult& Result, const FArcTerrainTileGrid& Grid,
		TConstArrayView<int32> ChannelIndices, TConstArrayView<uint32> PlaneIds, int32 BaseChannelIndex)
	{
		if (Result.Planes.Num() != ChannelIndices.Num() || ChannelIndices.Num() != PlaneIds.Num())
		{
			return;
		}

		const int64 PlaneBytes = static_cast<int64>(Grid.Resolution()) * Grid.Resolution();
		if (PlaneBytes <= 0 || PlaneBytes > TNumericLimits<int32>::Max())
		{
			return;
		}

		TArray<bool> PlaneKept;
		PlaneKept.Init(false, Result.Planes.Num());
		int32 BasePlaneIndex = INDEX_NONE;
		bool bAnyNonBaseKept = false;
		for (int32 PlaneIndex = 0; PlaneIndex < Result.Planes.Num(); ++PlaneIndex)
		{
			const TArray<uint8>& Plane = Result.Planes[PlaneIndex];
			if (Plane.Num() != PlaneBytes)
			{
				return;
			}
			if (ChannelIndices[PlaneIndex] == BaseChannelIndex)
			{
				BasePlaneIndex = PlaneIndex;
			}
			bool bAnyNonZero = false;
			for (uint8 Texel : Plane)
			{
				if (Texel != 0)
				{
					bAnyNonZero = true;
					break;
				}
			}
			if (bAnyNonZero)
			{
				PlaneKept[PlaneIndex] = true;
				bAnyNonBaseKept |= ChannelIndices[PlaneIndex] != BaseChannelIndex;
			}
		}
		if (BasePlaneIndex != INDEX_NONE && bAnyNonBaseKept)
		{
			PlaneKept[BasePlaneIndex] = true;
		}

		for (int32 PlaneIndex = 0; PlaneIndex < PlaneKept.Num(); ++PlaneIndex)
		{
			if (PlaneKept[PlaneIndex])
			{
				Result.KeptWeightPlaneIndices.Add(PlaneIndex);
			}
		}
		if (Result.KeptWeightPlaneIndices.IsEmpty())
		{
			Result.bWeightEmpty = true;
			Result.bWeightFinalized = true;
			return;
		}
		if (Result.KeptWeightPlaneIndices.Num() > ArcTerrainTileFormat::MaxPlaneCount)
		{
			Result.bWeightOverflow = true;
			Result.bWeightFinalized = true;
			return;
		}

		TArray<uint32> KeptPlaneIds;
		KeptPlaneIds.Reserve(Result.KeptWeightPlaneIndices.Num());
		Result.EncodedBody.Reserve(static_cast<int32>(PlaneBytes) * Result.KeptWeightPlaneIndices.Num());
		for (int32 PlaneIndex : Result.KeptWeightPlaneIndices)
		{
			KeptPlaneIds.Add(PlaneIds[PlaneIndex]);
			Result.EncodedBody.Append(Result.Planes[PlaneIndex]);
		}
		Result.EncodedHeader = ArcTerrainTileFormat::MakeWeightHeader(Grid, KeptPlaneIds);
		Result.bWeightFinalized = true;
	}

	/** Warn once per tier while it has any known violating tile. Compliant rewrites remove their keys and
	 *  rearm the tier once clear; a changed contract rearms every tier. */
	void WarnIfHeightBoundsViolated(const AArcTerrain& Terrain, FArcTerrainState& State, int32 Tier,
		FIntPoint TileKey, float QuantMin, float QuantMax)
	{
		if (Tier < 0 || Tier >= ArcTerrain::MaxTiers)
		{
			return;
		}

		const FVector2f HeightRangeCm = ArcTerrain::ResolveConservativeHeightRangeCm(
			Terrain.ConservativeMinHeightCm, Terrain.ConservativeMaxHeightCm);
		if (!State.bHeightBoundsWarningRangeInitialized
			|| State.WarnedHeightBoundsRangeCm.X != HeightRangeCm.X
			|| State.WarnedHeightBoundsRangeCm.Y != HeightRangeCm.Y)
		{
			for (TSet<FIntPoint>& WarnedTiles : State.WarnedHeightBoundsTiles)
			{
				WarnedTiles.Reset();
			}
			State.WarnedHeightBoundsRangeCm = HeightRangeCm;
			State.bHeightBoundsWarningRangeInitialized = true;
		}

		TSet<FIntPoint>& WarnedTiles = State.WarnedHeightBoundsTiles[Tier];
		const bool bViolates = !FMath::IsFinite(QuantMin) || !FMath::IsFinite(QuantMax)
			|| QuantMin < HeightRangeCm.X || QuantMax > HeightRangeCm.Y;
		if (!bViolates)
		{
			WarnedTiles.Remove(TileKey);
			return;
		}

		const bool bShouldWarn = WarnedTiles.IsEmpty();
		WarnedTiles.Add(TileKey);
		if (bShouldWarn)
		{
			UE_LOGF(LogArcTerrain, Warning,"Terrain '%ls' height tile tier %d key (%d,%d) quant range [%.3f, %.3f] cm "
					"exceeds conservative render bounds [%.3f, %.3f] cm; expand the authored height "
					"range to prevent false frustum culling.",
				*Terrain.GetPathName(), Tier, TileKey.X, TileKey.Y, QuantMin, QuantMax,
				HeightRangeCm.X, HeightRangeCm.Y);
		}
	}

	// THE BOUND ON THE INCOMPLETE-GATHER RE-ATTEMPT: 0.5 s doubling to an 8 s ceiling, per tile. The
	// candidate order is stable, so without it one stamp whose texture never decodes re-rasters its whole
	// tile at the head of the list every tick and the budget never reaches the tiles behind it.
	bool IsRetryDeferred(const FArcTerrainTileRailState& RailState, FIntPoint TileKey)
	{
		const FArcTerrainTileDecision* Decision = RailState.Decisions.Find(TileKey);
		return Decision != nullptr && Decision->RetryCount > 0
			&& FPlatformTime::Seconds() < Decision->RetryAfter;
	}

	void ScheduleRetry(FArcTerrainTileRailState& RailState, FIntPoint TileKey)
	{
		FArcTerrainTileDecision& Decision = RailState.Decisions.FindOrAdd(TileKey);
		RailState.MarkRetryDeferred(Decision);
		Decision.RetryAfter = ArcTerrainSubsystemPrivate::ScheduleExponentialRetry(
			Decision.RetryCount, 0.5, FPlatformTime::Seconds());
		RailState.Defer(TileKey, Decision.RetryAfter);
	}

	// THE KEY MEMO. Gathering and hashing every covering stamp is expensive, and in the steady state
	// every candidate paid it every tick before the WrittenKey compare could short-circuit.
	bool IsDecisionFresh(FArcTerrainTileRailState& RailState, FIntPoint TileKey)
	{
		FArcTerrainTileDecision* Decision = RailState.Decisions.Find(TileKey);
		if (Decision == nullptr || Decision->WrittenKey.IsZero()
			|| Decision->DecidedSerial != Decision->DirtySerial)
		{
			return false;
		}
		return true;
	}

	uint32 CurrentDirtySerial(FArcTerrainTileRailState& RailState, FIntPoint TileKey)
	{
		return RailState.Decisions.FindOrAdd(TileKey).DirtySerial;
	}

	// The decision a key produced, whatever it was: a write, a drop, or a refusal whose recipe will keep
	// producing the same refusal. Clears the backoff — the tile got an answer.
	//
	// DecidedSerial is the serial the KEY was computed at, which for an off-thread evaluation is its
	// launch serial: an edit landing mid-flight has already moved DirtySerial, and recording the finished
	// payload against the newer one would tell the memo a stale tile is current.
	//
	// A budget refusal and a deferred retry leave the entry in place, which is why their tiles come back
	// next tick. The other permanent answers — a fresh memo, a zero cache key, a lattice or a channel set
	// this rail cannot write against — drop the entry at their own exits, recording no decision.
	void RecordDecision(FArcTerrainTileRailState& RailState, TSet<FIntPoint>& PendingKeys, FIntPoint TileKey,
		const FIoHash& CacheKey, uint32 DecidedSerial, bool bCountsAsComplete = true)
	{
		FArcTerrainTileDecision& Decision = RailState.Decisions.FindOrAdd(TileKey);
		RailState.ClearRetryDeferred(TileKey, Decision);
		Decision.WrittenKey = CacheKey;
		Decision.DecidedSerial = DecidedSerial;
		Decision.RetryAfter = 0.0;
		Decision.RetryCount = 0;
		const bool bCurrentDecision = Decision.DecidedSerial == Decision.DirtySerial;
		if (bCurrentDecision && PendingKeys.Remove(TileKey) > 0)
		{
			RailState.RemoveReady(TileKey);
			if (bCountsAsComplete)
			{
				ArcTerrainDiag::NoteBakeCompleted();
			}
		}
	}

	// The key stays on the work list for the whole flight of the write: the loaded tile still holds
	// PRE-EDIT bytes until the manifest entry is published, and IsTileAuthoritativeForCell reads absence
	// from that list as permission to draw it, which would drop the edit off the screen until the write
	// lands. WritesInFlight is what stops the bake loop taking the key again meanwhile.
	FArcTerrainTileRailState::FWriteReservation BeginTileWrite(FArcTerrainTileRailState& RailState,
		TSet<FIntPoint>& PendingKeys, FIntPoint TileKey, const FIoHash& CacheKey, uint32 DecidedSerial,
		uint64 ReservationId, int64 ReservedBytes)
	{
		RecordDecision(RailState, PendingKeys, TileKey, CacheKey, DecidedSerial, false);
		PendingKeys.Add(TileKey);
		return RailState.BeginWrite(TileKey, ReservationId, ReservedBytes, DecidedSerial);
	}

	// The gather the recipe asks for cannot be completed — an undecodable source, a mask that did not
	// resolve. No key is recorded (the payload would be missing a contributor under a matching key) and the
	// tile leaves the work list: re-rastering a whole tile against a source that does not resolve is not a
	// recovery path, and the edit that fixes the source queues it again.
	//
	// THE BACKOFF IS CLEARED, NOT SCHEDULED: leaving the work list is what bounds the re-attempt, and
	// nothing visits the tile afterwards to observe a deadline. A standing RetryCount would instead make
	// IsRetryDeferred — the first test of every Probe* — skip the requeue the fixed source produces, for
	// up to the 8 s ceiling.
	void AbandonIncomplete(FArcTerrainTileRailState& RailState, TSet<FIntPoint>& PendingKeys,
		FIntPoint TileKey)
	{
		FArcTerrainTileDecision& Decision = RailState.Decisions.FindOrAdd(TileKey);
		RailState.ClearRetryDeferred(TileKey, Decision);
		Decision.RetryAfter = 0.0;
		Decision.RetryCount = 0;
		if (PendingKeys.Remove(TileKey) > 0)
		{
			RailState.RemoveReady(TileKey);
		}
	}

	// The memo lives in editor RAM, so a fresh session has none and would rewrite every tile it visits; the
	// manifest carries the same statement on disk. A ZERO key from either side is "unknown", never a match:
	// RebuildFromDisk names files it never opened, and unknown has to force the write it cannot rule out.
	bool IsDecidedKey(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		const FArcTerrainTileRailState& RailState, FIntPoint TileKey, const FIoHash& CacheKey)
	{
		const FArcTerrainTileDecision* Decision = RailState.Decisions.Find(TileKey);
		if (Decision != nullptr && !Decision->WrittenKey.IsZero())
		{
			return Decision->WrittenKey == CacheKey;
		}
		const FIoHash WrittenKey = ArcTerrainTileAccess::FindTileWrittenKey(Terrain, Rail, Tier, TileKey);
		return !WrittenKey.IsZero() && WrittenKey == CacheKey;
	}

	const TCHAR* DescribeDecisionState(const FArcTerrainTileRailState& RailState, FIntPoint TileKey,
		const FIoHash& CacheKey)
	{
		const FArcTerrainTileDecision* Decision = RailState.Decisions.Find(TileKey);
		if (Decision == nullptr)
		{
			return TEXT("none");
		}
		if (Decision->WrittenKey.IsZero())
		{
			return TEXT("key_zero");
		}
		return Decision->WrittenKey == CacheKey ? TEXT("key_match") : TEXT("key_mismatch");
	}

	const TCHAR* DescribeManifestState(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		const FArcTerrainTileRailState& RailState, FIntPoint TileKey, const FIoHash& CacheKey)
	{
		const FArcTerrainTileDecision* Decision = RailState.Decisions.Find(TileKey);
		if (Decision != nullptr && !Decision->WrittenKey.IsZero())
		{
			return TEXT("not_consulted");
		}
		const FIoHash WrittenKey = ArcTerrainTileAccess::FindTileWrittenKey(Terrain, Rail, Tier, TileKey);
		if (WrittenKey.IsZero())
		{
			return TEXT("none");
		}
		return WrittenKey == CacheKey ? TEXT("key_match") : TEXT("key_mismatch");
	}

	void NoteLaunchedBakeCoalesce(const AArcTerrain& Terrain, FArcTerrainTileRailState& RailState,
		EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey, int32 Contributors)
	{
		if (FArcTerrainTileDecision* Decision = RailState.Decisions.Find(TileKey))
		{
			ArcTerrainDiag::NoteBakeCoalesce(Terrain, Rail, Tier, TileKey,
				Decision->CoalescedInvalidations, Contributors, Decision->DirtySerial);
			Decision->CoalescedInvalidations = 0;
		}
	}
}

namespace ArcTerrainBakeWakeLocal_a41e
{
	FArcTerrainBakeCompletionToken MakeToken(AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, uint64 ReservationId)
	{
		FArcTerrainBakeCompletionToken Token;
		Token.Terrain = &Terrain;
		Token.Ref.Rail = Rail;
		Token.Ref.Tier = Tier;
		Token.Ref.Key = TileKey;
		Token.ReservationId = ReservationId;
		return Token;
	}

	void PostEvaluationCompletion(UArcTerrainSubsystem& Subsystem,
		const UE::Tasks::TTask<FArcTerrainTileBakeResult>& Evaluation,
		FArcTerrainBakeCompletionToken&& Token)
	{
		UArcTerrainSubsystem* SubsystemPtr = &Subsystem;
		// The evaluation continuation posts the token: the body must not signal before its handle is complete.
		UE::Tasks::Launch(UE_SOURCE_LOCATION,
			[SubsystemPtr, Token = MoveTemp(Token)]() mutable
			{
				SubsystemPtr->EnqueueBakeCompletion(MoveTemp(Token));
			},
			UE::Tasks::Prerequisites(Evaluation),
			UE::Tasks::ETaskPriority::BackgroundNormal);
	}

	ArcTerrainTileCache::FArcTerrainAsyncGetCompletion MakeProbeCompletion(UArcTerrainSubsystem& Subsystem,
		FArcTerrainBakeCompletionToken&& Token)
	{
		UArcTerrainSubsystem* SubsystemPtr = &Subsystem;
		return [SubsystemPtr, Token = MoveTemp(Token)]() mutable
		{
			SubsystemPtr->EnqueueBakeCompletion(MoveTemp(Token));
		};
	}
}

void UArcTerrainSubsystem::TickTerrainTileBakes(AArcTerrain& Terrain, FArcTerrainState& State,
	const FString& TileDir, int32 SettleTicks,
	int32& NumHeavySteps, int32& NumBakeCompletions, int32& NumBakeLaneScans, int32& NumBakeCandidateExaminations,
	double HeavyDeadline, bool& bStoppedForBudget)
{
	using namespace ArcTerrainSubsystemPrivate;
	using namespace ArcTerrainTileWrite_8a3f;

	check(IsInGameThread());

	const FArcTerrainConfig& Config = Terrain.Config;
	const UArcTerrainSettings* Settings = GetDefault<UArcTerrainSettings>();
	check(Settings != nullptr);
	const int32 WeightQuads = ResolveWeightQuads(Config, State);
	++State.BakePass;
	State.BakeSettleTicks = FMath::Max(SettleTicks, 0);
	TOptional<FArcTerrainDataMapBakeScope> DataBakeScope;

	constexpr int32 FineRailCount = 3;
	const int32 RailCursor = State.BakeLaneCursor;
	State.BakeLaneCursor = RailCursor + 1;
	if (State.BakeLaneCursor >= FineRailCount)
	{
		State.BakeLaneCursor = 0;
	}

	for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
	{
		const int32 NumRailsHere = (Tier == 0) ? FineRailCount : 2;
		const int32 StartRail = RailCursor % NumRailsHere;
		bool bStopLanes = false;
		for (int32 RailOffset = 0; RailOffset < NumRailsHere; ++RailOffset)
		{
		if (IsBudgetExhausted(NumBakeLaneScans, HeavyDeadline))
		{
			bStoppedForBudget = true;
			bStopLanes = true;
			break;
		}

		const int32 RailSlot = (StartRail + RailOffset) % NumRailsHere;
		++NumBakeLaneScans;

		const EArcTerrainRail Rail = RailSlot == 0
			? EArcTerrainRail::Height
			: (RailSlot == 1 ? EArcTerrainRail::Weight : EArcTerrainRail::Data);

		const int32 RailIndex = static_cast<int32>(Rail);
		FArcTerrainTileGrid Grid;
		if (!ArcTerrainTileBake::ResolveBakeTileGrid(Terrain, Rail, Tier, WeightQuads, Grid))
		{
			// No lattice means no tile keys at all, so the entries name nothing this rail can compute
			// and would sit there for the life of the session.
			const bool bResetHeight0 = Rail == EArcTerrainRail::Height && Tier == 0
				&& State.PendingTiles(Rail, Tier).Num() > 0;
			State.PendingTiles(Rail, Tier).Reset();
			if (bResetHeight0)
			{
				ArcTerrainResidencyIndex_c4e8::NotePendingHeightReset(State);
			}
			FArcTerrainTileRailState& InvalidRailState = State.TileRails[RailIndex][Tier];
			AbandonRailPendingTileBakes(InvalidRailState);
			InvalidateRailWrites(InvalidRailState);
			InvalidRailState.ResetPending();
			InvalidRailState.ResetWorkQueue();
			continue;
		}

		FArcTerrainTileRailState& RailState = State.TileRails[RailIndex][Tier];
		RailState.ReleaseDeferred(FPlatformTime::Seconds());
		ArcTerrainDiag::NoteBakeCandidates(Rail, Tier, RailState.NumReady());

		const int32 StepsBeforeLane = NumHeavySteps;
		const int32 ReadyCount = RailState.NumReady();
		for (int32 CandidateIndex = 0; CandidateIndex < ReadyCount; ++CandidateIndex)
		{
			if (IsBudgetExhausted(NumBakeCandidateExaminations, HeavyDeadline))
			{
				bStoppedForBudget = true;
				break;
			}
			FIntPoint TileKey;
			if (!RailState.TakeReady(TileKey))
			{
				break;
			}
			++NumBakeCandidateExaminations;
			TSet<FIntPoint>& PendingKeys = State.PendingTiles(Rail, Tier);
			if (!PendingKeys.Contains(TileKey))
			{
				continue;
			}
			const FArcTerrainTileDecision* Decision = RailState.Decisions.Find(TileKey);
			if (Decision != nullptr && Decision->ReadyAfterBakePass > State.BakePass)
			{
				RailState.EnqueueReady(TileKey);
				continue;
			}
			if (RailState.WritesInFlight.Contains(TileKey))
			{
				RailState.EnqueueReady(TileKey);
				continue;
			}
			const FArcTerrainPendingTileBake* InFlight = RailState.Pending.Find(TileKey);
			const bool bCompletedProbe = InFlight != nullptr && InFlight->Probe.IsValid()
				&& ArcTerrainTileCache::IsComplete(InFlight->Probe);
			const bool bCompletedInFlight = InFlight != nullptr
				&& ((InFlight->Evaluation.IsValid() && InFlight->Evaluation.IsCompleted())
					|| bCompletedProbe);
			if (InFlight != nullptr && InFlight->Evaluation.IsValid() && InFlight->Evaluation.IsCompleted())
			{
				const int64 WriteReservedBytes = Rail == EArcTerrainRail::Height
					? static_cast<int64>(Grid.Resolution()) * Grid.Resolution() * sizeof(uint16) + 4096
					: Rail == EArcTerrainRail::Weight
						? static_cast<int64>(Grid.Resolution()) * Grid.Resolution() * InFlight->ChannelIndices.Num() + 4096
						: EstimateTilePayloadBytes(EArcTerrainRail::Data, Grid, Config) + 4096;
				if (IsTileWriteFenced(Terrain, Rail, Tier, TileKey)
					|| !CanReserveTileBakeWriteAfterEvaluation(InFlight->ReservationId, WriteReservedBytes, *Settings))
				{
					RailState.EnqueueReady(TileKey);
					bStoppedForBudget = true;
					break;
				}
			}
			if (bCompletedInFlight && NumBakeCompletions >= Settings->MaxTileBakeCompletionsPerFrame)
			{
				RailState.EnqueueReady(TileKey);
				bStoppedForBudget = true;
				continue;
			}
			if (Rail == EArcTerrainRail::Height && bCompletedProbe
				&& !CanReserveTileBakeWork(EArcTerrainBakeReservationKind::Evaluation,
					EstimateEvaluationBytes(EArcTerrainRail::Height, Grid, Config), *Settings))
			{
				RailState.EnqueueReady(TileKey);
				bStoppedForBudget = true;
				break;
			}
			if (Rail == EArcTerrainRail::Weight && bCompletedProbe)
			{
				const int32 ChannelCount = Config.ChannelSet != nullptr ? Config.ChannelSet->Channels.Num() : 0;
				const int64 MaximumWriteReservedBytes = static_cast<int64>(Grid.Resolution()) * Grid.Resolution()
					* ChannelCount + 4096;
				if (IsTileWriteFenced(Terrain, Rail, Tier, TileKey)
					|| !CanReserveTileBakeWriteAfterReservation(InFlight->ReservationId,
						MaximumWriteReservedBytes, *Settings))
				{
					RailState.EnqueueReady(TileKey);
					bStoppedForBudget = true;
					break;
				}
			}
			if (Rail == EArcTerrainRail::Data && bCompletedProbe
				&& !CanReserveTileBakeWork(EArcTerrainBakeReservationKind::Evaluation,
					EstimateEvaluationBytes(EArcTerrainRail::Data, Grid, Config), *Settings))
			{
				RailState.EnqueueReady(TileKey);
				bStoppedForBudget = true;
				break;
			}
			if (InFlight == nullptr)
			{
				const int64 PayloadBytes = EstimateTilePayloadBytes(Rail, Grid, Config);
				if ((Rail == EArcTerrainRail::Height || Rail == EArcTerrainRail::Weight || Rail == EArcTerrainRail::Data)
					&& !CanReserveTileBakeWork(EArcTerrainBakeReservationKind::Probe, PayloadBytes, *Settings))
				{
					RailState.EnqueueReady(TileKey);
					bStoppedForBudget = true;
					break;
				}
			}
			if (Rail == EArcTerrainRail::Height)
			{
				ProbeAndRebuildHeightTile(Terrain, State, TileDir, Tier, TileKey, NumHeavySteps, *Settings);
			}
			else if (Rail == EArcTerrainRail::Weight)
			{
				ProbeAndRebuildWeightTile(Terrain, State, TileDir, Tier, TileKey, NumHeavySteps, *Settings);
			}
			else
			{
				if (!DataBakeScope.IsSet())
				{
					DataBakeScope.Emplace(SourceCache);
				}
				ProbeAndRebuildDataTile(Terrain, State, TileDir, TileKey, DataBakeScope.GetValue(),
					NumHeavySteps, *Settings);
			}
			const FArcTerrainPendingTileBake* PendingAfter = RailState.Pending.Find(TileKey);
			const bool bCompletionDrained = PendingAfter == nullptr
				|| (bCompletedProbe && PendingAfter->Evaluation.IsValid());
			NumBakeCompletions += bCompletedInFlight && bCompletionDrained ? 1 : 0;
			if (!PendingKeys.Contains(TileKey) || RailState.WritesInFlight.Contains(TileKey))
			{
				continue;
			}
			const FArcTerrainTileDecision* UpdatedDecision = RailState.Decisions.Find(TileKey);
			if (UpdatedDecision != nullptr && UpdatedDecision->bRetryDeferred)
			{
				continue;
			}
			const bool bAsyncInFlight = PendingAfter != nullptr
				&& ((PendingAfter->Evaluation.IsValid() && !PendingAfter->Evaluation.IsCompleted())
					|| (PendingAfter->Probe.IsValid() && !ArcTerrainTileCache::IsComplete(PendingAfter->Probe)));
			if (bAsyncInFlight)
			{
				continue;
			}
			RailState.EnqueueReady(TileKey);
		}
		ArcTerrainDiag::NoteBakeSteps(Rail, Tier, NumHeavySteps - StepsBeforeLane);
		}
		if (bStopLanes)
		{
			break;
		}
	}
}

void UArcTerrainSubsystem::ProbeAndRebuildHeightTile(AArcTerrain& Terrain, FArcTerrainState& State,
	const FString& TileDir, int32 Tier, FIntPoint TileKey, int32& NumHeightBakes,
	const UArcTerrainSettings& Settings)
{
	using namespace ArcTerrainSubsystemPrivate;
	using namespace ArcTerrainTileWrite_8a3f;

	FArcTerrainTileRailState& RailState =
		State.TileRails[static_cast<int32>(EArcTerrainRail::Height)][Tier];
	TSet<FIntPoint>& PendingKeys = State.PendingTiles(EArcTerrainRail::Height, Tier);
	const FArcTerrainConfig& Config = Terrain.Config;

	FArcTerrainTileGrid Grid;
	if (!ArcTerrainTileBake::ResolveBakeTileGrid(Terrain, EArcTerrainRail::Height, Tier, 0, Grid))
	{
		PendingKeys.Remove(TileKey);
		return;
	}
	const int32 Resolution = Grid.Resolution();
	const int32 PlaneTexels = Resolution * Resolution;

	if (FArcTerrainPendingTileBake* InFlight = RailState.Pending.Find(TileKey))
	{
		if (InFlight->Evaluation.IsValid())
		{
			if (!InFlight->Evaluation.IsCompleted())
			{
				return;
			}
			const int64 MaximumWriteReservedBytes = static_cast<int64>(PlaneTexels) * sizeof(uint16) + 4096;
			if (IsTileWriteFenced(Terrain, EArcTerrainRail::Height, Tier, TileKey)
				|| !CanReserveTileBakeWriteAfterEvaluation(InFlight->ReservationId, MaximumWriteReservedBytes,
					Settings))
			{
				return;
			}
			FArcTerrainPendingTileBake Done = RailState.TakePending(TileKey);
			ReleaseTileBakeReservation(Done.ReservationId);
			++NumHeightBakes;

			FArcTerrainTileBakeResult& Result = Done.Evaluation.GetResult();
			// A non-finite payload comes back EMPTY. Its key is latched all the same: the recipe that
			// produced it will keep producing it, and re-evaluating a tile this size every tick is not a
			// recovery path.
			if (Done.Resolution != Resolution || Result.Floats.Num() != PlaneTexels)
			{
				RecordDecision(RailState, PendingKeys, TileKey, Done.CacheKey, Done.DirtySerial);
				return;
			}

			if (Result.EncodedBody.Num() != PlaneTexels * static_cast<int32>(sizeof(uint16))
				|| !Result.EncodedHeader.IsValid())
			{
				RecordDecision(RailState, PendingKeys, TileKey, Done.CacheKey, Done.DirtySerial);
				return;
			}
			const int64 WriteReservedBytes = Result.EncodedBody.Num() + 4096;
			check(WriteReservedBytes <= MaximumWriteReservedBytes);
			WarnIfHeightBoundsViolated(Terrain, State, Tier, TileKey, Result.QuantMin, Result.QuantMax);
			const uint64 WriteReservationId = AcquireTileBakeWriteReservation(Terrain, EArcTerrainRail::Height,
				Tier, TileKey, WriteReservedBytes);
			const FArcTerrainTileRailState::FWriteReservation WriteReservation = BeginTileWrite(
				RailState, PendingKeys, TileKey, Done.CacheKey, Done.DirtySerial, WriteReservationId,
				WriteReservedBytes);
			const double WriteStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
			WriteFinalizedTile(TileDir, EArcTerrainRail::Height, Tier, TileKey, MoveTemp(Result.EncodedHeader),
				MoveTemp(Result.EncodedBody),
				MakeTileWriteCompletion(Terrain, EArcTerrainRail::Height, Tier, TileKey, Done.CacheKey,
					WriteReservation.ReservationId, WriteReservation.Generation));
			ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::HeightFinalizeWrite,
				WriteStartSeconds);
			if (!Done.bFromCache)
			{
				ArcTerrainTileBlobs::PutCachedHeightTexels(Done.CacheKey,
					TConstArrayView<float>(Result.Floats.GetData(), PlaneTexels));
			}
			return;
		}
		if (InFlight->Probe.IsValid() && !ArcTerrainTileCache::IsComplete(InFlight->Probe))
		{
			return;
		}
	}

	if (IsRetryDeferred(RailState, TileKey))
	{
		ArcTerrainDiag::NoteBakeDeferred(EArcTerrainRail::Height, Tier);
		return;
	}
	if (IsDecisionFresh(RailState, TileKey))
	{
		// The memo says the recipe has not moved since the tile was decided, so the bytes on disk are the
		// answer this pass would compute — the same standing as a write.
		ArcTerrainDiag::NoteBakeFresh(EArcTerrainRail::Height, Tier);
		PendingKeys.Remove(TileKey);
		return;
	}

	FArcTerrainTileBakeInputs Inputs;
	FIoHash CacheKey;
	{
		ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("tile cache key"), TileKey);
		if (!ArcTerrainTileBake::GatherTileBakeInputs(
			EArcTerrainRail::Height, Tier, TileKey, Terrain, State, Inputs))
		{
			PendingKeys.Remove(TileKey);
			return;
		}
		CacheKey = ArcTerrainTileBake::ComputeTileCacheKey(
			EArcTerrainRail::Height, Tier, TileKey, Terrain, State, Inputs, nullptr);
	}
	if (CacheKey.IsZero())
	{
		PendingKeys.Remove(TileKey);
		return;
	}
	const uint32 DirtySerial = CurrentDirtySerial(RailState, TileKey);
	if (IsDecidedKey(Terrain, EArcTerrainRail::Height, Tier, RailState, TileKey, CacheKey))
	{
		AbandonPendingTileBake(RailState, TileKey);
		RecordDecision(RailState, PendingKeys, TileKey, CacheKey, DirtySerial);
		return;
	}

	// The probe is consumed against the FRESHLY recomputed key: a texture re-import moves nothing else,
	// so a handle issued under a key the tile has since left must not be admitted.
	bool bProbeMissed = false;
	if (FArcTerrainPendingTileBake* Parked = RailState.Pending.Find(TileKey))
	{
		FArcTerrainPendingTileBake Taken = RailState.TakePending(TileKey);
		ReleaseTileBakeReservation(Taken.ReservationId);
		if (Taken.Probe.IsValid() && Taken.CacheKey == CacheKey && Taken.Resolution == Resolution)
		{
			TArray<float> CachedTexels;
			if (ArcTerrainTileBlobs::ConsumeCachedHeightTexels(Taken.Probe, Resolution, CachedTexels))
			{
				if (!CanReserveTileBakeWork(EArcTerrainBakeReservationKind::Evaluation,
					EstimateEvaluationBytes(EArcTerrainRail::Height, Grid, Config), Settings))
				{
					ScheduleRetry(RailState, TileKey);
					return;
				}
				FArcTerrainPendingTileBake Pending;
				Pending.CacheKey = CacheKey;
				Pending.DirtySerial = DirtySerial;
				Pending.Resolution = Resolution;
				Pending.ReservedBytes = EstimateEvaluationBytes(EArcTerrainRail::Height, Grid, Config);
				Pending.bFromCache = true;
				Pending.ReservationId = AcquireTileBakeReservation(EArcTerrainBakeReservationKind::Evaluation,
					Pending.ReservedBytes);
				Pending.Evaluation = UE::Tasks::Launch(UE_SOURCE_LOCATION,
					[Texels = MoveTemp(CachedTexels), Grid]() mutable -> FArcTerrainTileBakeResult
					{
						FArcTerrainHeightBakeResult Bake;
						ArcTerrainTileBlobs::BuildHeightBakeResult(MoveTemp(Texels), Bake);
						FArcTerrainTileBakeResult Result;
						if (!Bake.bFinite)
						{
							return Result;
						}
						Result.Floats = MoveTemp(Bake.Texels);
						Result.QuantMin = Bake.LocalMin;
						Result.QuantMax = Bake.LocalMax;
						ArcTerrainTileFormat::EncodeHeightTexels(Result.Floats, Result.QuantMin,
							Result.QuantMax, Result.EncodedBody);
						Result.EncodedHeader = ArcTerrainTileFormat::MakeHeightHeader(Grid, Result.QuantMin,
							Result.QuantMax);
						return Result;
					},
					UE::Tasks::ETaskPriority::BackgroundNormal);
				const UE::Tasks::TTask<FArcTerrainTileBakeResult> EvaluationForLedger = Pending.Evaluation;
				if (FTileBakeLedgerEntry* Ledger = TileBakeLedger.Find(Pending.ReservationId))
				{
					Ledger->IsComplete = [EvaluationForLedger]() { return EvaluationForLedger.IsCompleted(); };
				}
				ArcTerrainBakeWakeLocal_a41e::PostEvaluationCompletion(*this, Pending.Evaluation,
					ArcTerrainBakeWakeLocal_a41e::MakeToken(Terrain, EArcTerrainRail::Height, Tier, TileKey,
						Pending.ReservationId));
				RailState.AddPending(TileKey, MoveTemp(Pending));
				return;
			}
			// A miss evaluates in THIS call: re-issuing under the same key would loop the tile between
			// probe and probe for ever.
			bProbeMissed = true;
		}
	}

	if (!bProbeMissed)
	{
		FArcTerrainPendingTileBake Pending;
		Pending.CacheKey = CacheKey;
		Pending.DirtySerial = DirtySerial;
		Pending.Resolution = Resolution;
		Pending.ReservedBytes = EstimateTilePayloadBytes(EArcTerrainRail::Height, Grid, Config);
		Pending.ReservationId = AcquireTileBakeReservation(EArcTerrainBakeReservationKind::Probe,
			Pending.ReservedBytes);
		Pending.Probe = ArcTerrainTileBlobs::BeginGetCachedHeightTexels(CacheKey, Resolution,
			ArcTerrainBakeWakeLocal_a41e::MakeProbeCompletion(*this,
				ArcTerrainBakeWakeLocal_a41e::MakeToken(Terrain, EArcTerrainRail::Height, Tier, TileKey,
					Pending.ReservationId)));
		if (Pending.Probe.IsValid())
		{
			const ArcTerrainTileCache::FAsyncGetPtr ProbeForLedger = Pending.Probe;
			if (FTileBakeLedgerEntry* Ledger = TileBakeLedger.Find(Pending.ReservationId))
			{
				Ledger->IsComplete = [ProbeForLedger]() { return ArcTerrainTileCache::IsComplete(ProbeForLedger); };
			}
			NoteLaunchedBakeCoalesce(Terrain, RailState, EArcTerrainRail::Height, Tier, TileKey,
				Inputs.SortedStamps.Num());
			RailState.AddPending(TileKey, MoveTemp(Pending));
			++NumHeightBakes;
			return;
		}
		ReleaseTileBakeReservation(Pending.ReservationId);
	}

	ArcTerrainCpuComposite::FCellHeightEval Eval;
	if (!CanReserveTileBakeWork(EArcTerrainBakeReservationKind::Evaluation,
		EstimateEvaluationBytes(EArcTerrainRail::Height, Grid, Config), Settings))
	{
		ScheduleRetry(RailState, TileKey);
		return;
	}
	{
		// Reads GetSourceDescriptor and the source cache, so it cannot leave the game thread.
		ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("height gather ops"), TileKey);
		ArcTerrainCpuComposite::GatherCellHeightOps(SourceCache, Config, Terrain.GetTerrainTransform(),
			ArcTerrainTileBake::MakeTileRaster(Grid, TileKey), Terrain.HeightOptions.BaseHeightCm,
			Inputs.SortedStamps, Eval);
	}
	++NumHeightBakes;
	if (!Eval.bComplete)
	{
		ArcTerrainDiag::NoteBakeIncomplete(EArcTerrainRail::Height, Tier);
		AbandonIncomplete(RailState, PendingKeys, TileKey);
		return;
	}

	FArcTerrainPendingTileBake Pending;
	Pending.CacheKey = CacheKey;
	Pending.DirtySerial = DirtySerial;
	Pending.Resolution = Resolution;
	Pending.ReservedBytes = EstimateEvaluationBytes(EArcTerrainRail::Height, Grid, Config);
	Pending.ReservationId = AcquireTileBakeReservation(EArcTerrainBakeReservationKind::Evaluation,
		Pending.ReservedBytes);
	Pending.Evaluation = UE::Tasks::Launch(UE_SOURCE_LOCATION,
		[Eval = MoveTemp(Eval), Grid]() -> FArcTerrainTileBakeResult
		{
			TArray<float> Heights;
			ArcTerrainCpuComposite::EvaluateCellHeights(Eval, Heights);
			FArcTerrainHeightBakeResult Bake;
			ArcTerrainTileBlobs::BuildHeightBakeResult(MoveTemp(Heights), Bake);
			FArcTerrainTileBakeResult Result;
			if (!Bake.bFinite)
			{
				return Result;
			}
			Result.Floats = MoveTemp(Bake.Texels);
			Result.QuantMin = Bake.LocalMin;
			Result.QuantMax = Bake.LocalMax;
			ArcTerrainTileFormat::EncodeHeightTexels(Result.Floats, Result.QuantMin, Result.QuantMax,
				Result.EncodedBody);
			Result.EncodedHeader = ArcTerrainTileFormat::MakeHeightHeader(Grid, Result.QuantMin,
				Result.QuantMax);
			return Result;
		},
		UE::Tasks::ETaskPriority::BackgroundNormal);
	const UE::Tasks::TTask<FArcTerrainTileBakeResult> EvaluationForLedger = Pending.Evaluation;
	if (FTileBakeLedgerEntry* Ledger = TileBakeLedger.Find(Pending.ReservationId))
	{
		Ledger->IsComplete = [EvaluationForLedger]() { return EvaluationForLedger.IsCompleted(); };
	}
	ArcTerrainBakeWakeLocal_a41e::PostEvaluationCompletion(*this, Pending.Evaluation,
		ArcTerrainBakeWakeLocal_a41e::MakeToken(Terrain, EArcTerrainRail::Height, Tier, TileKey,
			Pending.ReservationId));
	if (!bProbeMissed)
	{
		NoteLaunchedBakeCoalesce(Terrain, RailState, EArcTerrainRail::Height, Tier, TileKey,
			Inputs.SortedStamps.Num());
	}
	RailState.AddPending(TileKey, MoveTemp(Pending));
}

void UArcTerrainSubsystem::ProbeAndRebuildWeightTile(AArcTerrain& Terrain, FArcTerrainState& State,
	const FString& TileDir, int32 Tier, FIntPoint TileKey, int32& NumWeightRebuilds,
	const UArcTerrainSettings& Settings)
{
	using namespace ArcTerrainSubsystemPrivate;
	using namespace ArcTerrainTileWrite_8a3f;

	FArcTerrainTileRailState& RailState =
		State.TileRails[static_cast<int32>(EArcTerrainRail::Weight)][Tier];
	TSet<FIntPoint>& PendingKeys = State.PendingTiles(EArcTerrainRail::Weight, Tier);
	const FArcTerrainConfig& Config = Terrain.Config;
	const int32 NumChannels = Config.ChannelSet ? Config.ChannelSet->Channels.Num() : 0;
	// The plane COUNT is one byte on disk and every present channel gets a plane now that the file is not
	// truncated, so a wider channel set cannot be addressed by the format at all.
	//
	// THE ENTRY GOES WITH THE REFUSAL, like a zero cache key: a terrain with no channel set queues weight
	// tiles it can never write, and an entry left here is walked — and re-refused — by every pass for
	// the life of the session. BOTH sets go: the
	// refusal sits ahead of the in-flight consumption below, so a channel set cleared mid-evaluation would
	// otherwise strand its Pending entry, which the candidate walk nominates every tick for ever.
	const bool bBaseAddressable = ArcTerrain::ResolveBaseChannelIndex(NumChannels) != INDEX_NONE;
	if (NumChannels <= 0 || NumChannels > ArcTerrainTileFormat::MaxPlaneCount || !bBaseAddressable)
	{
		if (NumChannels > 0 && !bBaseAddressable)
		{
			// Game thread only (the tile rails tick there), so the set needs no lock.
			static TSet<int32> LoggedUnaddressableBaseCounts;
			bool bAlreadyLogged = false;
			LoggedUnaddressableBaseCounts.Add(NumChannels, &bAlreadyLogged);
			if (!bAlreadyLogged)
			{
				UE_LOGF(LogArcTerrain, Error,
					"[Weight] Base channel %d is outside the terrain's %d-channel set — the weight rail "
					"bakes nothing, the atlas clears no base lane and the composite refuses to normalize. "
					"Logged once per channel count.",
					ArcTerrain::GBaseChannelIndex, NumChannels);
			}
		}
		PendingKeys.Remove(TileKey);
		AbandonPendingTileBake(RailState, TileKey);
		return;
	}

	FArcTerrainTileGrid Grid;
	if (!ArcTerrainTileBake::ResolveBakeTileGrid(Terrain, EArcTerrainRail::Weight, Tier,
		ResolveWeightQuads(Config, State), Grid))
	{
		PendingKeys.Remove(TileKey);
		AbandonPendingTileBake(RailState, TileKey);
		return;
	}
	const int32 Resolution = Grid.Resolution();

	if (FArcTerrainPendingTileBake* InFlight = RailState.Pending.Find(TileKey))
	{
		if (InFlight->Evaluation.IsValid())
		{
			if (!InFlight->Evaluation.IsCompleted())
			{
				return;
			}
			const int64 MaximumWriteReservedBytes = static_cast<int64>(Resolution) * Resolution
				* InFlight->ChannelIndices.Num() + 4096;
			if (IsTileWriteFenced(Terrain, EArcTerrainRail::Weight, Tier, TileKey)
				|| !CanReserveTileBakeWriteAfterEvaluation(InFlight->ReservationId, MaximumWriteReservedBytes,
					Settings))
			{
				return;
			}
			FArcTerrainPendingTileBake Done = RailState.TakePending(TileKey);
			ReleaseTileBakeReservation(Done.ReservationId);
			++NumWeightRebuilds;

			FArcTerrainTileBakeResult& Result = Done.Evaluation.GetResult();
			if (Done.Resolution != Resolution || Result.Planes.Num() != Done.ChannelIndices.Num()
				|| !Result.bWeightFinalized)
			{
				// A short plane set is not an empty tile: writing one would claim the layers are gone.
				ScheduleRetry(RailState, TileKey);
				return;
			}
			if (Result.bWeightEmpty)
			{
				if (DropTile(Terrain, TileDir, EArcTerrainRail::Weight, Tier, TileKey))
				{
					RecordDecision(RailState, PendingKeys, TileKey, Done.CacheKey, Done.DirtySerial);
				}
				else
				{
					ScheduleRetry(RailState, TileKey);
				}
				return;
			}
			if (Result.bWeightOverflow)
			{
				UE_LOGF(LogArcTerrain, Error,"[Weight] Tile (%d,%d): %d planes exceed the %d the tile header can encode — not written.",
					TileKey.X, TileKey.Y, Result.KeptWeightPlaneIndices.Num(), ArcTerrainTileFormat::MaxPlaneCount);
				RecordDecision(RailState, PendingKeys, TileKey, Done.CacheKey, Done.DirtySerial);
				return;
			}

			const int64 WriteReservedBytes = Result.EncodedBody.Num() + 4096;
			check(WriteReservedBytes <= MaximumWriteReservedBytes);
			const uint64 WriteReservationId = AcquireTileBakeWriteReservation(Terrain, EArcTerrainRail::Weight,
				Tier, TileKey, WriteReservedBytes);
			const FArcTerrainTileRailState::FWriteReservation WriteReservation = BeginTileWrite(
				RailState, PendingKeys, TileKey, Done.CacheKey, Done.DirtySerial, WriteReservationId,
				WriteReservedBytes);
			WriteFinalizedTile(TileDir, EArcTerrainRail::Weight, Tier, TileKey,
				MoveTemp(Result.EncodedHeader), MoveTemp(Result.EncodedBody),
				MakeTileWriteCompletion(Terrain, EArcTerrainRail::Weight, Tier, TileKey, Done.CacheKey,
					WriteReservation.ReservationId, WriteReservation.Generation));

			// DDC keeps the existing index-labelled, unencoded evaluated planes; the worker's header/body are
			// an output optimization and must not become a second cache schema.
			TArray<ArcTerrainWeights::FArcTerrainWeightPlane> Stored;
			Stored.Reserve(Result.KeptWeightPlaneIndices.Num());
			for (int32 PlaneIndex : Result.KeptWeightPlaneIndices)
			{
				ArcTerrainWeights::FArcTerrainWeightPlane& Plane = Stored.AddDefaulted_GetRef();
				Plane.ChannelIndex = Done.ChannelIndices[PlaneIndex];
				Plane.G8 = Result.Planes[PlaneIndex];
			}
			ArcTerrainTileBlobs::PutCachedWeightPlanes(Done.CacheKey, *Config.ChannelSet, Stored);
			return;
		}
		if (InFlight->Probe.IsValid() && !ArcTerrainTileCache::IsComplete(InFlight->Probe))
		{
			return;
		}
		if (InFlight->Probe.IsValid())
		{
			const int64 MaximumWriteReservedBytes = static_cast<int64>(Resolution) * Resolution * NumChannels + 4096;
			if (IsTileWriteFenced(Terrain, EArcTerrainRail::Weight, Tier, TileKey)
				|| !CanReserveTileBakeWriteAfterReservation(InFlight->ReservationId,
					MaximumWriteReservedBytes, Settings))
			{
				return;
			}
		}
	}

	if (IsRetryDeferred(RailState, TileKey))
	{
		ArcTerrainDiag::NoteBakeDeferred(EArcTerrainRail::Weight, Tier);
		return;
	}
	if (IsDecisionFresh(RailState, TileKey))
	{
		ArcTerrainDiag::NoteBakeFresh(EArcTerrainRail::Weight, Tier);
		PendingKeys.Remove(TileKey);
		return;
	}

	FArcTerrainTileBakeInputs Inputs;
	FIoHash CacheKey;
	{
		ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("tile cache key"), TileKey);
		if (!ArcTerrainTileBake::GatherTileBakeInputs(
			EArcTerrainRail::Weight, Tier, TileKey, Terrain, State, Inputs))
		{
			PendingKeys.Remove(TileKey);
			return;
		}
		CacheKey = ArcTerrainTileBake::ComputeTileCacheKey(
			EArcTerrainRail::Weight, Tier, TileKey, Terrain, State, Inputs, nullptr);
	}
	if (CacheKey.IsZero())
	{
		PendingKeys.Remove(TileKey);
		return;
	}
	const uint32 DirtySerial = CurrentDirtySerial(RailState, TileKey);
	if (IsDecidedKey(Terrain, EArcTerrainRail::Weight, Tier, RailState, TileKey, CacheKey))
	{
		AbandonPendingTileBake(RailState, TileKey);
		RecordDecision(RailState, PendingKeys, TileKey, CacheKey, DirtySerial);
		return;
	}

	bool bProbeMissed = false;
	if (FArcTerrainPendingTileBake* Parked = RailState.Pending.Find(TileKey))
	{
		FArcTerrainPendingTileBake Taken = RailState.TakePending(TileKey);
		ReleaseTileBakeReservation(Taken.ReservationId);
		if (Taken.Probe.IsValid() && Taken.CacheKey == CacheKey && Taken.Resolution == Resolution)
		{
			TArray<ArcTerrainWeights::FArcTerrainWeightPlane> CachedPlanes;
			if (ArcTerrainTileBlobs::ConsumeCachedWeightPlanes(Taken.Probe, Resolution, *Config.ChannelSet,
				CachedPlanes))
			{
				++NumWeightRebuilds;
				WriteWeightTilePlanes(Terrain, TileDir, Tier, TileKey, Grid, CachedPlanes, CacheKey,
					DirtySerial, /*bFromCache=*/true, State, RailState, Settings, PendingKeys);
				return;
			}
			bProbeMissed = true;
		}
	}

	TArray<int32> PresentChannels;
	GatherPresentWeightChannels(Config, Inputs.SortedStamps, PresentChannels);
	if (PresentChannels.Num() == 0)
	{
		// Weightless area: the tile is the absence of a file, not an empty one.
		if (DropTile(Terrain, TileDir, EArcTerrainRail::Weight, Tier, TileKey))
		{
			RecordDecision(RailState, PendingKeys, TileKey, CacheKey, DirtySerial);
		}
		else
		{
			// A delete that failed is transient (a lock, a read-only bit), so the tile stays a candidate —
			// and the backoff is what stops it re-gathering its whole stamp stack every visit meanwhile.
			ScheduleRetry(RailState, TileKey);
		}
		return;
	}

	if (!bProbeMissed)
	{
		FArcTerrainPendingTileBake Pending;
		Pending.CacheKey = CacheKey;
		Pending.DirtySerial = DirtySerial;
		Pending.Resolution = Resolution;
		Pending.ReservedBytes = EstimateTilePayloadBytes(EArcTerrainRail::Weight, Grid, Config);
		Pending.ReservationId = AcquireTileBakeReservation(EArcTerrainBakeReservationKind::Probe,
			Pending.ReservedBytes);
		Pending.Probe = ArcTerrainTileBlobs::BeginGetCachedWeightPlanes(CacheKey, Resolution, NumChannels,
			ArcTerrainBakeWakeLocal_a41e::MakeProbeCompletion(*this,
				ArcTerrainBakeWakeLocal_a41e::MakeToken(Terrain, EArcTerrainRail::Weight, Tier, TileKey,
					Pending.ReservationId)));
		if (Pending.Probe.IsValid())
		{
			const ArcTerrainTileCache::FAsyncGetPtr ProbeForLedger = Pending.Probe;
			if (FTileBakeLedgerEntry* Ledger = TileBakeLedger.Find(Pending.ReservationId))
			{
				Ledger->IsComplete = [ProbeForLedger]() { return ArcTerrainTileCache::IsComplete(ProbeForLedger); };
			}
			NoteLaunchedBakeCoalesce(Terrain, RailState, EArcTerrainRail::Weight, Tier, TileKey,
				Inputs.SortedStamps.Num());
			RailState.AddPending(TileKey, MoveTemp(Pending));
			++NumWeightRebuilds;
			return;
		}
		ReleaseTileBakeReservation(Pending.ReservationId);
	}

	ArcTerrainCpuComposite::FCellWeightEval Eval;
	if (!CanReserveTileBakeWork(EArcTerrainBakeReservationKind::Evaluation,
		EstimateEvaluationBytes(EArcTerrainRail::Weight, Grid, Config), Settings))
	{
		ScheduleRetry(RailState, TileKey);
		return;
	}
	{
		ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("weight gather ops"), TileKey);
		ArcTerrainCpuComposite::GatherCellWeightOps(SourceCache, Config, Terrain.GetTerrainTransform(),
			ArcTerrainTileBake::MakeTileRaster(Grid, TileKey), PresentChannels, Inputs.SortedStamps, Eval);
	}
	++NumWeightRebuilds;
	if (!Eval.bComplete)
	{
		ArcTerrainDiag::NoteBakeIncomplete(EArcTerrainRail::Weight, Tier);
		AbandonIncomplete(RailState, PendingKeys, TileKey);
		return;
	}

	TArray<uint32> PresentChannelIds;
	PresentChannelIds.Reserve(PresentChannels.Num());
	for (int32 ChannelIndex : PresentChannels)
	{
		PresentChannelIds.Add(ArcTerrain::ChannelNameHash(Config.ChannelSet->Channels[ChannelIndex]));
	}

	FArcTerrainPendingTileBake Pending;
	Pending.CacheKey = CacheKey;
	Pending.DirtySerial = DirtySerial;
	Pending.Resolution = Resolution;
	Pending.ChannelIndices = PresentChannels;
	Pending.ReservedBytes = EstimateEvaluationBytes(EArcTerrainRail::Weight, Grid, Config);
	Pending.ReservationId = AcquireTileBakeReservation(EArcTerrainBakeReservationKind::Evaluation,
		Pending.ReservedBytes);
	Pending.Evaluation = UE::Tasks::Launch(UE_SOURCE_LOCATION,
		[Eval = MoveTemp(Eval), Grid, PresentChannels, PresentChannelIds,
			BaseChannelIndex = ArcTerrain::GBaseChannelIndex]() -> FArcTerrainTileBakeResult
		{
			FArcTerrainTileBakeResult Result;
			ArcTerrainCpuComposite::EvaluateCellWeightPlanes(Eval, Result.Planes);
			FinalizeWeightBakeResult(Result, Grid, PresentChannels, PresentChannelIds, BaseChannelIndex);
			return Result;
		},
		UE::Tasks::ETaskPriority::BackgroundNormal);
	const UE::Tasks::TTask<FArcTerrainTileBakeResult> EvaluationForLedger = Pending.Evaluation;
	if (FTileBakeLedgerEntry* Ledger = TileBakeLedger.Find(Pending.ReservationId))
	{
		Ledger->IsComplete = [EvaluationForLedger]() { return EvaluationForLedger.IsCompleted(); };
	}
	ArcTerrainBakeWakeLocal_a41e::PostEvaluationCompletion(*this, Pending.Evaluation,
		ArcTerrainBakeWakeLocal_a41e::MakeToken(Terrain, EArcTerrainRail::Weight, Tier, TileKey,
			Pending.ReservationId));
	if (!bProbeMissed)
	{
		NoteLaunchedBakeCoalesce(Terrain, RailState, EArcTerrainRail::Weight, Tier, TileKey,
			Inputs.SortedStamps.Num());
	}
	RailState.AddPending(TileKey, MoveTemp(Pending));
}

void UArcTerrainSubsystem::WriteWeightTilePlanes(AArcTerrain& Terrain, const FString& TileDir, int32 Tier,
	FIntPoint TileKey, const FArcTerrainTileGrid& Grid,
	TConstArrayView<ArcTerrainWeights::FArcTerrainWeightPlane> Planes, const FIoHash& CacheKey,
	uint32 DecidedSerial, bool bFromCache, FArcTerrainState& State, FArcTerrainTileRailState& RailState,
	const UArcTerrainSettings& Settings,
	TSet<FIntPoint>& PendingKeys)
{
	using namespace ArcTerrainTileWrite_8a3f;

	const int64 PlaneBytes = static_cast<int64>(Grid.Resolution()) * Grid.Resolution();
	const UArcTerrainChannelSet* ChannelSet = Terrain.Config.ChannelSet;

	// Kept whenever any other plane survives, so an unpainted texel reads full base weight.
	const int32 BaseChannelIndex = (ChannelSet != nullptr && ChannelSet->Channels.IsValidIndex(ArcTerrain::GBaseChannelIndex))
		? ArcTerrain::GBaseChannelIndex
		: INDEX_NONE;

	// The all-zero filter the section persist applies: such a plane samples as absent either way, and
	// writing it costs a whole plane of the tile. Two-pass — rank, then build the kept list.
	TArray<bool> PlaneKept;
	PlaneKept.Init(false, Planes.Num());
	int32 BasePlaneIndex = INDEX_NONE;
	bool bAnyNonBaseKept = false;
	for (int32 PlaneIndex = 0; PlaneIndex < Planes.Num(); ++PlaneIndex)
	{
		const ArcTerrainWeights::FArcTerrainWeightPlane& Plane = Planes[PlaneIndex];
		if (Plane.ChannelIndex < 0 || Plane.ChannelIndex > ArcTerrainTileFormat::MaxPlaneCount
			|| !ChannelSet || !ChannelSet->Channels.IsValidIndex(Plane.ChannelIndex)
			|| Plane.G8.Num() != PlaneBytes)
		{
			continue;
		}
		if (Plane.ChannelIndex == BaseChannelIndex)
		{
			BasePlaneIndex = PlaneIndex;
		}
		bool bAnyNonZero = false;
		for (uint8 Texel : Plane.G8)
		{
			if (Texel != 0)
			{
				bAnyNonZero = true;
				break;
			}
		}
		if (bAnyNonZero)
		{
			PlaneKept[PlaneIndex] = true;
			if (Plane.ChannelIndex != BaseChannelIndex)
			{
				bAnyNonBaseKept = true;
			}
		}
	}
	if (BasePlaneIndex != INDEX_NONE && bAnyNonBaseKept)
	{
		PlaneKept[BasePlaneIndex] = true;
	}

	TArray<int32> KeptIndices;
	KeptIndices.Reserve(Planes.Num());
	for (int32 PlaneIndex = 0; PlaneIndex < Planes.Num(); ++PlaneIndex)
	{
		if (PlaneKept[PlaneIndex])
		{
			KeptIndices.Add(PlaneIndex);
		}
	}

	if (KeptIndices.Num() == 0)
	{
		if (DropTile(Terrain, TileDir, EArcTerrainRail::Weight, Tier, TileKey))
		{
			RecordDecision(RailState, PendingKeys, TileKey, CacheKey, DecidedSerial);
		}
		else
		{
			ScheduleRetry(RailState, TileKey);
		}
		return;
	}

	// One plane per present channel and one BYTE of header per plane: a wider set cannot be encoded.
	// The per-cell slice budget no longer caps this, so the format's own ceiling has to.
	//
	// LATCHED, not retried: the refusal is a property of the recipe, so the same key would produce it
	// again — without the latch the tile re-evaluates its whole weight stack on the game thread and logs
	// this line on every visit. A channel-set edit moves the key and the refusal is reconsidered.
	if (KeptIndices.Num() > ArcTerrainTileFormat::MaxPlaneCount)
	{
		UE_LOGF(LogArcTerrain, Error,"[Weight] Tile (%d,%d): %d planes exceed the %d the tile header can encode — not written.",
			TileKey.X, TileKey.Y, KeptIndices.Num(), ArcTerrainTileFormat::MaxPlaneCount);
		RecordDecision(RailState, PendingKeys, TileKey, CacheKey, DecidedSerial);
		return;
	}

	// The one index -> hash conversion on the write side: everything in memory stays index-keyed.
	TArray<uint32> PlaneIds;
	PlaneIds.Reserve(KeptIndices.Num());
	TArray<uint8> Body;
	Body.Reserve(static_cast<int32>(PlaneBytes) * KeptIndices.Num());
	for (int32 KeptIndex : KeptIndices)
	{
		PlaneIds.Add(ArcTerrain::ChannelNameHash(ChannelSet->Channels[Planes[KeptIndex].ChannelIndex]));
		Body.Append(Planes[KeptIndex].G8);
	}

	const int64 WriteReservedBytes = Body.Num() + 4096;
	if (IsTileWriteFenced(Terrain, EArcTerrainRail::Weight, Tier, TileKey)
		|| !CanReserveTileBakeWork(EArcTerrainBakeReservationKind::Write, WriteReservedBytes, Settings))
	{
		ScheduleRetry(RailState, TileKey);
		return;
	}
	const uint64 WriteReservationId = AcquireTileBakeWriteReservation(Terrain, EArcTerrainRail::Weight,
		Tier, TileKey, WriteReservedBytes);
	const FArcTerrainTileRailState::FWriteReservation WriteReservation = BeginTileWrite(
		RailState, PendingKeys, TileKey, CacheKey, DecidedSerial, WriteReservationId, WriteReservedBytes);
	WriteWeightTile(TileDir, Tier, TileKey, Grid, PlaneIds, MoveTemp(Body),
		MakeTileWriteCompletion(Terrain, EArcTerrainRail::Weight, Tier, TileKey, CacheKey,
			WriteReservation.ReservationId, WriteReservation.Generation));
	if (!bFromCache)
	{
		TArray<ArcTerrainWeights::FArcTerrainWeightPlane> Stored;
		Stored.Reserve(KeptIndices.Num());
		for (int32 KeptIndex : KeptIndices)
		{
			ArcTerrainWeights::FArcTerrainWeightPlane& Plane = Stored.AddDefaulted_GetRef();
			Plane.ChannelIndex = Planes[KeptIndex].ChannelIndex;
			Plane.G8 = Planes[KeptIndex].G8;
		}
		ArcTerrainTileBlobs::PutCachedWeightPlanes(CacheKey, *ChannelSet, Stored);
	}
}

TFunction<void(bool)> UArcTerrainSubsystem::MakeTileWriteCompletion(AArcTerrain& Terrain,
	EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey, const FIoHash& RecipeKey, uint64 ReservationId,
	uint64 WriteGeneration)
{
	return [WeakSubsystem = TWeakObjectPtr<UArcTerrainSubsystem>(this),
		WeakTerrain = TWeakObjectPtr<AArcTerrain>(&Terrain), Rail, Tier, TileKey, RecipeKey, ReservationId,
		WriteGeneration](bool bWritten)
	{
		if (UArcTerrainSubsystem* Subsystem = WeakSubsystem.Get())
		{
			Subsystem->FinishTileWrite(WeakTerrain, Rail, Tier, TileKey, RecipeKey, ReservationId,
				WriteGeneration, bWritten);
		}
	};
}

void UArcTerrainSubsystem::FinishTileWrite(TWeakObjectPtr<AArcTerrain> WeakTerrain, EArcTerrainRail Rail,
	int32 Tier, FIntPoint TileKey, const FIoHash& RecipeKey, uint64 ReservationId, uint64 WriteGeneration,
	bool bWritten)
{
	check(IsInGameThread());
	ReleaseTileBakeReservation(ReservationId);
	RequestBakeWake();

	FArcTerrainState* State = Terrains.Find(WeakTerrain);
	if (State == nullptr || !ArcTerrainTileAccess::IsValidTier(Tier))
	{
		return; // the terrain was unregistered and its tile store released; publishing would resurrect it
	}
	FArcTerrainTileRailState& RailState = State->TileRails[static_cast<int32>(Rail)][Tier];
	if (!RailState.FinishWrite(TileKey, ReservationId, WriteGeneration))
	{
		if (bWritten)
		{
			AArcTerrain* WrittenTerrain = WeakTerrain.Get();
			if (WrittenTerrain == nullptr)
			{
				return;
			}
			const FString TileDir = ArcTerrainTileNames::MakeTileDirForWrite(*WrittenTerrain);
			if (!TileDir.IsEmpty())
			{
				ArcTerrainTileWrite_8a3f::DropTile(*WrittenTerrain, TileDir, Rail, Tier, TileKey);
			}
		}
		return;
	}
	// A dead actor publishes nothing, so it must not dequeue either: falling through to the failure path
	// is what keeps the memo and the work list agreeing with the manifest.
	AArcTerrain* WrittenTerrain = WeakTerrain.Get();
	if (bWritten && WrittenTerrain != nullptr)
	{
		// The entry and the key it carries are published only once the bytes are on disk: a file the
		// manifest does not name is rewritten next session, while an entry naming a file that never landed
		// is unreachable for ever.
		{
			ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("tile manifest entry"), TileKey);
			ArcTerrainTileAccess::NotifyTileWritten(*WrittenTerrain, Rail, Tier, TileKey, RecipeKey);
		}
		// An edit that landed mid-flight has already moved DirtySerial, and the bytes just published are
		// the PRE-EDIT ones. Dequeuing on that would drop the edit for good.
		const FArcTerrainTileDecision* Decision = RailState.Decisions.Find(TileKey);
		if (Decision == nullptr || Decision->DecidedSerial == Decision->DirtySerial)
		{
			if (State->PendingTiles(Rail, Tier).Remove(TileKey) > 0)
			{
				RailState.RemoveReady(TileKey);
				ArcTerrainDiag::NoteBakeCompleted();
				if (Rail == EArcTerrainRail::Height && Tier == 0)
				{
					ArcTerrainResidencyIndex_c4e8::NotePendingHeightChanged(*State, TileKey);
				}
			}
		}
		else if (State->PendingTiles(Rail, Tier).Contains(TileKey))
		{
			RailState.EnqueueReady(TileKey);
		}
		return;
	}

	// Zero reads as unknown on both sides, so the next pass asks the manifest for the key the file on disk
	// really has rather than trusting a key whose bytes never landed.
	FArcTerrainTileDecision& Decision = RailState.Decisions.FindOrAdd(TileKey);
	Decision.WrittenKey = FIoHash();
	const bool bNewPending = !State->PendingTiles(Rail, Tier).Contains(TileKey);
	State->PendingTiles(Rail, Tier).Add(TileKey);
	if (bNewPending && Rail == EArcTerrainRail::Height && Tier == 0)
	{
		ArcTerrainResidencyIndex_c4e8::NotePendingHeightChanged(*State, TileKey);
	}
	ArcTerrainTileWrite_8a3f::ScheduleRetry(RailState, TileKey);
}

void UArcTerrainSubsystem::ProbeAndRebuildDataTile(AArcTerrain& Terrain, FArcTerrainState& State,
	const FString& TileDir, FIntPoint TileKey, FArcTerrainDataMapBakeScope& BakeScope,
	int32& NumDataBakes, const UArcTerrainSettings& Settings)
{
	using namespace ArcTerrainTileWrite_8a3f;

	FArcTerrainTileRailState& RailState = State.TileRails[static_cast<int32>(EArcTerrainRail::Data)][0];
	TSet<FIntPoint>& PendingKeys = State.PendingTiles(EArcTerrainRail::Data, 0);
	const FArcTerrainConfig& Config = Terrain.Config;

	FArcTerrainTileGrid Grid;
	if (!ArcTerrainTileBake::ResolveBakeTileGrid(Terrain, EArcTerrainRail::Data, 0, 0, Grid))
	{
		PendingKeys.Remove(TileKey);
		return;
	}
	const int32 Resolution = Grid.Resolution();
	const int32 PlaneTexels = Resolution * Resolution;

	if (IsRetryDeferred(RailState, TileKey))
	{
		ArcTerrainDiag::NoteBakeDeferred(EArcTerrainRail::Data, 0);
		return;
	}
	if (IsDecisionFresh(RailState, TileKey))
	{
		ArcTerrainDiag::NoteBakeFresh(EArcTerrainRail::Data, 0);
		PendingKeys.Remove(TileKey);
		return;
	}

	FArcTerrainTileBakeInputs Inputs;
	ArcTerrainDataMaps::FArcTerrainDataMapKeyInputs DataInputs;
	TConstArrayView<ArcTerrainDataMaps::FArcTerrainDataMapWindow> MaskWindows;
	double MaskWindowGatherSeconds = 0.0;
	FIoHash CacheKey;
	{
		ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("tile cache key"), TileKey);
		if (!ArcTerrainTileBake::GatherTileBakeInputs(
			EArcTerrainRail::Data, 0, TileKey, Terrain, State, Inputs))
		{
			PendingKeys.Remove(TileKey);
			return;
		}
		const double DataStartSeconds = ArcTerrainLeafTiming_8f21::Begin();
		MaskWindows = ArcTerrainDataMaps::GetOrGatherMaskWindows(State, MaskWindowGatherSeconds);
		ArcTerrainDataMaps::GatherDataMapKeyInputsForStamps(Config, Terrain.GetTerrainTransform(),
			Inputs.SortedStamps, MaskWindows, DataInputs);
		ArcTerrainLeafTiming_8f21::End(ArcTerrainDiag::ELeafTiming::TileKeyData, DataStartSeconds);
		CacheKey = ArcTerrainTileBake::ComputeTileCacheKey(
			EArcTerrainRail::Data, 0, TileKey, Terrain, State, Inputs, &DataInputs);
	}
	if (CacheKey.IsZero())
	{
		PendingKeys.Remove(TileKey);
		return;
	}
	const uint32 DirtySerial = CurrentDirtySerial(RailState, TileKey);
	if (IsDecidedKey(Terrain, EArcTerrainRail::Data, 0, RailState, TileKey, CacheKey))
	{
		RecordDecision(RailState, PendingKeys, TileKey, CacheKey, DirtySerial);
		return;
	}

	FArcTerrainDataMapCellBake Bake;
	bool bFromCache = false;
	bool bHaveBake = false;
	bool bProbeMissed = false;
	if (FArcTerrainPendingTileBake* InFlight = RailState.Pending.Find(TileKey))
	{
		if (InFlight->Evaluation.IsValid())
		{
			if (!InFlight->Evaluation.IsCompleted())
			{
				return;
			}
			FArcTerrainPendingTileBake Done = RailState.TakePending(TileKey);
			ReleaseTileBakeReservation(Done.ReservationId);
			if (Done.CacheKey == CacheKey && Done.Resolution == Resolution)
			{
				FArcTerrainTileBakeResult& WorkerResult = Done.Evaluation.GetResult();
				if (WorkerResult.DataMapBake.IsValid()
					&& WorkerResult.DataMapBake->DataWindowRevision == State.DataWindowRevision)
				{
					ArcTerrainDataMaps::ReportDataMapTileBakeWarnings(*WorkerResult.DataMapBake,
						MaskWindows, BakeScope);
					ArcTerrainDataMaps::ReportDataMapTileBakeStats(*WorkerResult.DataMapBake);
					Bake = MoveTemp(WorkerResult.DataMapBake->Bake);
					bFromCache = Done.bFromCache;
					bHaveBake = true;
				}
			}
		}
		else
		{
			if (!ArcTerrainTileCache::IsComplete(InFlight->Probe))
			{
				return;
			}
			FArcTerrainPendingTileBake Done = RailState.TakePending(TileKey);
			ReleaseTileBakeReservation(Done.ReservationId);
			if (Done.CacheKey == CacheKey && Done.Resolution == Resolution)
			{
				FArcTerrainDataMapCellBake CachedBake;
				const bool bBlobCacheHit = ArcTerrainTileBlobs::ConsumeCachedDataMasks(Done.Probe, TileKey,
					CachedBake);
				bool bValidCache = bBlobCacheHit;
				if (bBlobCacheHit)
				{
					for (const FArcTerrainDataMapCellMask& Mask : CachedBake.Masks)
					{
						bValidCache &= Mask.Quads == Grid.TileQuads;
					}
				}
				UE_CLOGF(ArcTerrainDataMaps::IsBakeStatsEnabled(), LogArcTerrain, Log,
					"ArcTerrainRebakeReason tile=%d,%d rail=Data tier=0 decision=%ls manifest=%ls blob_cache=%ls "
					"blob_demoted=%d dirty_serial=%u",
					TileKey.X, TileKey.Y,
					DescribeDecisionState(RailState, TileKey, CacheKey),
					DescribeManifestState(Terrain, EArcTerrainRail::Data, 0, RailState, TileKey, CacheKey),
					bBlobCacheHit ? TEXT("hit") : TEXT("miss"),
					bBlobCacheHit && !bValidCache ? 1 : 0, DirtySerial);
				if (bValidCache)
				{
					if (CanReserveTileBakeWork(EArcTerrainBakeReservationKind::Evaluation,
						EstimateEvaluationBytes(EArcTerrainRail::Data, Grid, Config), Settings))
					{
						FArcTerrainPendingTileBake Pending;
						Pending.CacheKey = CacheKey;
						Pending.DirtySerial = DirtySerial;
						Pending.Resolution = Resolution;
						Pending.ReservedBytes = EstimateEvaluationBytes(EArcTerrainRail::Data, Grid, Config);
						Pending.bFromCache = true;
						Pending.ReservationId = AcquireTileBakeReservation(
							EArcTerrainBakeReservationKind::Evaluation, Pending.ReservedBytes,
							TFunction<bool()>(), true);
						Pending.Evaluation = UE::Tasks::Launch(UE_SOURCE_LOCATION,
							[BakeResult = MoveTemp(CachedBake), DataWindowRevision = State.DataWindowRevision]()
								mutable -> FArcTerrainTileBakeResult
							{
								FArcTerrainTileBakeResult Result;
								Result.DataMapBake = MakeShared<FArcTerrainDataMapTileBakeResult, ESPMode::ThreadSafe>();
								Result.DataMapBake->Bake = MoveTemp(BakeResult);
								Result.DataMapBake->DataWindowRevision = DataWindowRevision;
								return Result;
							}, UE::Tasks::ETaskPriority::BackgroundNormal);
						const UE::Tasks::TTask<FArcTerrainTileBakeResult> EvaluationForLedger = Pending.Evaluation;
						if (FTileBakeLedgerEntry* Ledger = TileBakeLedger.Find(Pending.ReservationId))
						{
							Ledger->IsComplete = [EvaluationForLedger]()
							{
								return EvaluationForLedger.IsCompleted();
							};
						}
						ArcTerrainBakeWakeLocal_a41e::PostEvaluationCompletion(*this, Pending.Evaluation,
							ArcTerrainBakeWakeLocal_a41e::MakeToken(Terrain, EArcTerrainRail::Data, 0, TileKey,
								Pending.ReservationId));
						RailState.AddPending(TileKey, MoveTemp(Pending));
						++NumDataBakes;
						return;
					}
					ScheduleRetry(RailState, TileKey);
					return;
				}
				bProbeMissed = true;
			}
		}
	}

	if (!bHaveBake)
	{
		if (!bProbeMissed)
		{
			FArcTerrainPendingTileBake Pending;
			Pending.CacheKey = CacheKey;
			Pending.DirtySerial = DirtySerial;
			Pending.Resolution = Resolution;
			Pending.ReservedBytes = EstimateTilePayloadBytes(EArcTerrainRail::Data, Grid, Config);
			Pending.ReservationId = AcquireTileBakeReservation(EArcTerrainBakeReservationKind::Probe,
				Pending.ReservedBytes, TFunction<bool()>(), true);
			Pending.Probe = ArcTerrainTileBlobs::BeginGetCachedDataMasks(CacheKey,
				ArcTerrainBakeWakeLocal_a41e::MakeProbeCompletion(*this,
					ArcTerrainBakeWakeLocal_a41e::MakeToken(Terrain, EArcTerrainRail::Data, 0, TileKey,
						Pending.ReservationId)));
			if (Pending.Probe.IsValid())
			{
				const ArcTerrainTileCache::FAsyncGetPtr ProbeForLedger = Pending.Probe;
				if (FTileBakeLedgerEntry* Ledger = TileBakeLedger.Find(Pending.ReservationId))
				{
					Ledger->IsComplete = [ProbeForLedger]()
					{
						return ArcTerrainTileCache::IsComplete(ProbeForLedger);
					};
				}
				NoteLaunchedBakeCoalesce(Terrain, RailState, EArcTerrainRail::Data, 0, TileKey,
					Inputs.SortedStamps.Num());
				RailState.AddPending(TileKey, MoveTemp(Pending));
				++NumDataBakes;
				return;
			}
			ReleaseTileBakeReservation(Pending.ReservationId);
		}

		const int64 EvaluationBytes = EstimateEvaluationBytes(EArcTerrainRail::Data, Grid, Config);
		// Bake in-flight charges tile eval/write, not source-cache pins (SourceCacheMB / SourceCachePinMB).
		if (!CanReserveTileBakeWork(EArcTerrainBakeReservationKind::Evaluation, EvaluationBytes, Settings))
		{
			ScheduleRetry(RailState, TileKey);
			return;
		}
		FArcTerrainDataMapTileSnapshot Snapshot;
		{
			ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("data mask gather snapshot"), TileKey);
			const double TexelToLocal = static_cast<double>(Grid.TileExtentM) * 100.0
				/ static_cast<double>(Grid.TileQuads);
			ArcTerrainDataMaps::GatherDataMapTileSnapshot(Terrain.GetTerrainTransform(), TileKey,
				Inputs.SortedStamps, MaskWindows, State.DataWindowRevision, MaskWindowGatherSeconds,
				Grid.TileQuads, TexelToLocal, BakeScope, Snapshot);
		}
		bool bAnyUnresolved = false;
		for (const FArcTerrainDataMapTileSnapshot::FMaskWork& Work : Snapshot.Work)
		{
			bAnyUnresolved |= Work.bUnresolved;
		}
		if (bAnyUnresolved)
		{
			ArcTerrainDiag::NoteBakeIncomplete(EArcTerrainRail::Data, 0);
			AbandonIncomplete(RailState, PendingKeys, TileKey);
			return;
		}
		FArcTerrainPendingTileBake Pending;
		Pending.CacheKey = CacheKey;
		Pending.DirtySerial = DirtySerial;
		Pending.Resolution = Resolution;
		Pending.ReservedBytes = EvaluationBytes;
		Pending.ReservationId = AcquireTileBakeReservation(EArcTerrainBakeReservationKind::Evaluation,
			Pending.ReservedBytes, TFunction<bool()>(), true);
		Pending.Evaluation = UE::Tasks::Launch(UE_SOURCE_LOCATION,
			[Snapshot = MoveTemp(Snapshot)]() mutable -> FArcTerrainTileBakeResult
			{
				FArcTerrainTileBakeResult Result;
				Result.DataMapBake = MakeShared<FArcTerrainDataMapTileBakeResult, ESPMode::ThreadSafe>(
					ArcTerrainDataMaps::BakeDataMapTileSnapshot(Snapshot));
				return Result;
			}, UE::Tasks::ETaskPriority::BackgroundNormal);
		const UE::Tasks::TTask<FArcTerrainTileBakeResult> EvaluationForLedger = Pending.Evaluation;
		if (FTileBakeLedgerEntry* Ledger = TileBakeLedger.Find(Pending.ReservationId))
		{
			Ledger->IsComplete = [EvaluationForLedger]() { return EvaluationForLedger.IsCompleted(); };
		}
		ArcTerrainBakeWakeLocal_a41e::PostEvaluationCompletion(*this, Pending.Evaluation,
			ArcTerrainBakeWakeLocal_a41e::MakeToken(Terrain, EArcTerrainRail::Data, 0, TileKey,
				Pending.ReservationId));
		if (!bProbeMissed)
		{
			NoteLaunchedBakeCoalesce(Terrain, RailState, EArcTerrainRail::Data, 0, TileKey,
				Inputs.SortedStamps.Num());
		}
		RailState.AddPending(TileKey, MoveTemp(Pending));
		++NumDataBakes;
		return;
	}

	++NumDataBakes;

	bool bAnyUnresolved = false;
	TArray<int32> KeptIndices;
	KeptIndices.Reserve(Bake.Masks.Num());
	for (int32 MaskIndex = 0; MaskIndex < Bake.Masks.Num(); ++MaskIndex)
	{
		const FArcTerrainDataMapCellMask& Mask = Bake.Masks[MaskIndex];
		bAnyUnresolved |= Mask.bUnresolved;
		if (Mask.Quads == Grid.TileQuads && Mask.Texels.Num() == PlaneTexels)
		{
			KeptIndices.Add(MaskIndex);
		}
	}

	if (bAnyUnresolved)
	{
		ArcTerrainDiag::NoteBakeIncomplete(EArcTerrainRail::Data, 0);
		AbandonIncomplete(RailState, PendingKeys, TileKey);
		return;
	}

	if (KeptIndices.Num() == 0)
	{
		if (DropTile(Terrain, TileDir, EArcTerrainRail::Data, 0, TileKey))
		{
			RecordDecision(RailState, PendingKeys, TileKey, CacheKey, DirtySerial);
		}
		else
		{
			ScheduleRetry(RailState, TileKey);
		}
		return;
	}

	KeptIndices.Sort([&Bake](int32 A, int32 B)
	{
		return static_cast<uint8>(Bake.Masks[A].Map) < static_cast<uint8>(Bake.Masks[B].Map);
	});

	TArray<uint8> DataMapIds;
	DataMapIds.Reserve(KeptIndices.Num());
	TArray<float> QuantMin;
	QuantMin.Reserve(KeptIndices.Num());
	TArray<float> QuantMax;
	QuantMax.Reserve(KeptIndices.Num());
	TArray<uint8> Body;
	Body.Reserve(PlaneTexels * KeptIndices.Num());
	for (int32 KeptIndex : KeptIndices)
	{
		const FArcTerrainDataMapCellMask& Mask = Bake.Masks[KeptIndex];
		DataMapIds.Add(static_cast<uint8>(Mask.Map));
		QuantMin.Add(Mask.QuantMin);
		QuantMax.Add(Mask.QuantMax);
		Body.Append(Mask.Texels);
	}

	const int64 WriteReservedBytes = Body.Num() + 4096;
	if (IsTileWriteFenced(Terrain, EArcTerrainRail::Data, 0, TileKey)
		|| !CanReserveTileBakeWork(EArcTerrainBakeReservationKind::Write, WriteReservedBytes, Settings))
	{
		ScheduleRetry(RailState, TileKey);
		return;
	}
	const uint64 WriteReservationId = AcquireTileBakeWriteReservation(Terrain, EArcTerrainRail::Data, 0,
		TileKey, WriteReservedBytes);
	const FArcTerrainTileRailState::FWriteReservation WriteReservation = BeginTileWrite(
		RailState, PendingKeys, TileKey, CacheKey, DirtySerial, WriteReservationId, WriteReservedBytes);
	WriteDataTile(TileDir, TileKey, Grid, DataMapIds, QuantMin, QuantMax, MoveTemp(Body),
		MakeTileWriteCompletion(Terrain, EArcTerrainRail::Data, 0, TileKey, CacheKey,
			WriteReservation.ReservationId, WriteReservation.Generation));
	if (!bFromCache)
	{
		ArcTerrainTileBlobs::PutCachedDataMasks(CacheKey, Bake);
	}
}
#endif
