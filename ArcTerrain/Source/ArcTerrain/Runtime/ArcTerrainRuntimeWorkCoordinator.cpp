// Copyright Lukasz Baran. All Rights Reserved.

#include "Runtime/ArcTerrainRuntimeWorkCoordinator.h"

#include "ArcTerrainModule.h"
#include "ArcTerrainSettings.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"

namespace ArcTerrainRuntimeWorkCoordinator_a81c
{
	int32 ClampLimit(int32 Value)
	{
		return FMath::Max(Value, 0);
	}

	int32 LaneIndex(EArcTerrainRuntimeLane Lane)
	{
		return FMath::Clamp(static_cast<int32>(Lane), 0,
			static_cast<int32>(EArcTerrainRuntimeLane::Count) - 1);
	}

	void UniquePayloads(const TArray<FArcTerrainRetainedPayload>& In, TArray<FArcTerrainRetainedPayload>& Out)
	{
		Out.Reset();
		TSet<uint32> Seen;
		for (const FArcTerrainRetainedPayload& Payload : In)
		{
			if (Payload.PayloadId == 0)
			{
				continue;
			}
			bool bAlready = false;
			Seen.Add(Payload.PayloadId, &bAlready);
			if (!bAlready)
			{
				Out.Add(Payload);
			}
		}
	}

	uint64 SumNewPayloadBytes(const TArray<FArcTerrainRetainedPayload>& Unique,
		TFunctionRef<bool(uint32)> IsRetained)
	{
		uint64 Bytes = 0;
		for (const FArcTerrainRetainedPayload& Payload : Unique)
		{
			if (!IsRetained(Payload.PayloadId))
			{
				Bytes += Payload.Bytes;
			}
		}
		return Bytes;
	}
}

FArcTerrainRuntimeReservationToken::FArcTerrainRuntimeReservationToken(
	FArcTerrainRuntimeReservationToken&& Other) noexcept
	: Coordinator(MoveTemp(Other.Coordinator))
	, ReservationId(Other.ReservationId)
{
	Other.ReservationId = 0;
	Other.Coordinator.Reset();
}

FArcTerrainRuntimeReservationToken& FArcTerrainRuntimeReservationToken::operator=(
	FArcTerrainRuntimeReservationToken&& Other) noexcept
{
	if (this != &Other)
	{
		Release();
		Coordinator = MoveTemp(Other.Coordinator);
		ReservationId = Other.ReservationId;
		Other.ReservationId = 0;
		Other.Coordinator.Reset();
	}
	return *this;
}

FArcTerrainRuntimeReservationToken::~FArcTerrainRuntimeReservationToken()
{
	Release();
}

void FArcTerrainRuntimeReservationToken::Release()
{
	if (ReservationId == 0)
	{
		return;
	}
	const uint64 Id = ReservationId;
	ReservationId = 0;
	TSharedPtr<FArcTerrainRuntimeWorkCoordinator, ESPMode::ThreadSafe> Pinned = Coordinator.Pin();
	Coordinator.Reset();
	if (Pinned.IsValid())
	{
		Pinned->ApplyReservationRelease(Id);
	}
}

uint64 FArcTerrainRuntimeReservationToken::Detach()
{
	const uint64 Id = ReservationId;
	ReservationId = 0;
	Coordinator.Reset();
	return Id;
}

bool FArcTerrainRuntimeWorkContext::TryGrow(uint64 AdditionalBytes)
{
	if (AdditionalBytes == 0)
	{
		return true;
	}
	if (Coordinator == nullptr)
	{
		return false;
	}
	if (AdditionalBytes <= RemainingPreReserve)
	{
		RemainingPreReserve -= AdditionalBytes;
		return true;
	}
	const uint64 Extra = AdditionalBytes - RemainingPreReserve;
	RemainingPreReserve = 0;
	return Coordinator->TryGrowReservation(ReservationId, Extra);
}

bool FArcTerrainRuntimeWorkContext::IsAbandoned() const
{
	return Coordinator == nullptr || Coordinator->IsReservationAbandoned(ReservationId);
}

FArcTerrainRuntimeWorkCoordinator::FArcTerrainRuntimeWorkCoordinator()
{
}

FArcTerrainRuntimeWorkCoordinator::~FArcTerrainRuntimeWorkCoordinator()
{
	if (!bShutdown)
	{
		ShutdownAndWait();
	}
}

FArcTerrainRuntimeWorkCoordinator* FArcTerrainRuntimeWorkCoordinator::TryGet()
{
	if (FArcTerrainModule* Module = FModuleManager::GetModulePtr<FArcTerrainModule>(TEXT("ArcTerrain")))
	{
		return Module->GetRuntimeCoordinator();
	}
	return nullptr;
}

void FArcTerrainRuntimeWorkCoordinator::ReadBudgetsFromSettings()
{
	if (GIsInitialLoad || !UObjectInitialized())
	{
		return;
	}
	const UArcTerrainSettings* Settings = GetDefault<UArcTerrainSettings>();
	if (Settings == nullptr)
	{
		return;
	}
	MaxTasks = FMath::Max(Settings->RuntimeWorkerTasks, 1);
	MaxOwnedBytes = static_cast<uint64>(FMath::Max(Settings->RuntimeOwnedMiB, 1)) << 20;
	CompletionGrantsPerFrame = FMath::Max(Settings->RuntimeCompletionsPerFrame, 1);
	DefaultLaneLimits[static_cast<int32>(EArcTerrainRuntimeLane::Atlas)] =
		FMath::Max(Settings->RuntimeAtlasLaunchesPerFrame, 0);
	DefaultLaneLimits[static_cast<int32>(EArcTerrainRuntimeLane::Lighting)] =
		FMath::Max(Settings->RuntimeLightingLaunchesPerFrame, 0);
	DefaultLaneLimits[static_cast<int32>(EArcTerrainRuntimeLane::Far)] =
		FMath::Max(Settings->RuntimeFarLaunchesPerFrame, 0);
}

FArcTerrainRuntimeReservationToken FArcTerrainRuntimeWorkCoordinator::MakeToken(uint64 ReservationId)
{
	FArcTerrainRuntimeReservationToken Token;
	Token.Coordinator = AsShared();
	Token.ReservationId = ReservationId;
	return Token;
}

bool FArcTerrainRuntimeWorkCoordinator::TryReserveSubmission(
	const FArcTerrainRuntimeReservationRequest& Request,
	FArcTerrainRuntimeReservationToken& OutToken)
{
	OutToken.Release();
	if (static_cast<uint8>(Request.Lane) >= static_cast<uint8>(EArcTerrainRuntimeLane::Count)
		|| Request.OwnerId.Value == 0)
	{
		return false;
	}

	TArray<FArcTerrainRetainedPayload> Unique;
	ArcTerrainRuntimeWorkCoordinator_a81c::UniquePayloads(Request.Payloads, Unique);

	FScopeLock Lock(&Mutex);
	if (bShutdown)
	{
		return false;
	}

	const uint64 NewPayloadBytes = ArcTerrainRuntimeWorkCoordinator_a81c::SumNewPayloadBytes(
		Unique,
		[this](uint32 PayloadId)
		{
			return PayloadRetains.Contains(PayloadId);
		});
	const uint64 NewBytes = Request.ExactOwnedInputBytes + Request.PreReservedResultBytes + NewPayloadBytes;
	if (OwnedBytes + NewBytes > MaxOwnedBytes)
	{
		return false;
	}

	const uint64 ReservationId = NextReservationId++;
	FReservationRecord& Record = Reservations.Add(ReservationId);
	Record.OwnerId = Request.OwnerId;
	Record.TerrainId = Request.TerrainId;
	Record.Lane = Request.Lane;
	Record.JobOwnedBytes = Request.ExactOwnedInputBytes + Request.PreReservedResultBytes;
	Record.PreReservedResultBytes = Request.PreReservedResultBytes;
	Record.PayloadIds.Reserve(Unique.Num());
	Record.Payloads.Reserve(Unique.Num());
	for (const FArcTerrainRetainedPayload& Payload : Unique)
	{
		Record.PayloadIds.Add(Payload.PayloadId);
		Record.Payloads.Add(Payload.Payload);
	}
	RetainPayloads_NoLock(Unique);
	OwnedBytes += Request.ExactOwnedInputBytes + Request.PreReservedResultBytes;
	OutToken = MakeToken(ReservationId);
	return true;
}

void FArcTerrainRuntimeWorkCoordinator::SetTerrainLaneLaunchLimit(
	FArcTerrainRuntimeOwnerId OwnerId,
	uint64 TerrainId,
	EArcTerrainRuntimeLane Lane,
	int32 MaxLaunchesPerFrame)
{
	if (static_cast<uint8>(Lane) >= static_cast<uint8>(EArcTerrainRuntimeLane::Count))
	{
		return;
	}
	FScopeLock Lock(&Mutex);
	if (bShutdown)
	{
		return;
	}
	const FArcTerrainRuntimeOwnerTerrainLane Key{ OwnerId, TerrainId, Lane };
	LaneLimits.Add(Key, ArcTerrainRuntimeWorkCoordinator_a81c::ClampLimit(MaxLaunchesPerFrame));
}

void FArcTerrainRuntimeWorkCoordinator::Submit(
	FArcTerrainRuntimeReservationToken&& Token,
	UE::Tasks::ETaskPriority TaskPriority,
	TUniqueFunction<FArcTerrainRuntimeResult(FArcTerrainRuntimeWorkContext&)>&& Work)
{
	const uint64 ReservationId = Token.Detach();
	if (ReservationId == 0 || !Work)
	{
		if (ReservationId != 0)
		{
			ApplyReservationRelease(ReservationId);
		}
		return;
	}

	TSharedPtr<FQueuedJob, ESPMode::ThreadSafe> Job = MakeShared<FQueuedJob, ESPMode::ThreadSafe>();
	Job->ReservationId = ReservationId;
	Job->TaskPriority = TaskPriority;
	Job->Work = MoveTemp(Work);

	FScopeLock Lock(&Mutex);
	if (bShutdown)
	{
		Lock.Unlock();
		ApplyReservationRelease(ReservationId);
		return;
	}

	const FReservationRecord* Record = Reservations.Find(ReservationId);
	if (Record == nullptr || Record->bAbandoned)
	{
		Lock.Unlock();
		ApplyReservationRelease(ReservationId);
		return;
	}

	Job->OwnerId = Record->OwnerId;
	Job->TerrainId = Record->TerrainId;
	Job->Lane = Record->Lane;
	Job->PreReservedResultBytes = Record->PreReservedResultBytes;
	ReadyJobs.Add(ReservationId, Job);
	EnqueueReady_NoLock(ReservationId, Record->OwnerId, Record->TerrainId, Record->Lane);
}

void FArcTerrainRuntimeWorkCoordinator::BeginFrameAndPump(uint64 FrameCounter)
{
	TArray<UE::Tasks::FTask> Ignored;
	{
		FScopeLock Lock(&Mutex);
		if (bShutdown)
		{
			return;
		}
		ReadBudgetsFromSettings();
		if (LastPumpedFrame != FrameCounter)
		{
			LastPumpedFrame = FrameCounter;
			LaunchedThisFrame.Reset();
			GrantedCredits.Reset();
			GrantCompletions_NoLock();
		}
		LaunchReadyJobs_NoLock();
	}
}

void FArcTerrainRuntimeWorkCoordinator::DrainGrantedCompletions(
	FArcTerrainRuntimeOwnerId OwnerId,
	TFunctionRef<void(FArcTerrainRuntimeCompletion&&)> Consumer)
{
	TArray<FArcTerrainRuntimeCompletion> Batch;
	{
		FScopeLock Lock(&Mutex);
		int32* Granted = GrantedCredits.Find(OwnerId);
		FOwnerCompletions* OwnerCompletions = Completions.Find(OwnerId);
		if (Granted == nullptr || *Granted <= 0 || OwnerCompletions == nullptr
			|| OwnerCompletions->PendingCount() == 0)
		{
			return;
		}
		const int32 LaneCount = static_cast<int32>(EArcTerrainRuntimeLane::Count);
		int32 Remaining = *Granted;
		int32 IdleRounds = 0;
		Batch.Reserve(Remaining);
		while (Remaining > 0 && IdleRounds < LaneCount)
		{
			OwnerCompletions->DrainCursor = OwnerCompletions->DrainCursor % LaneCount;
			TArray<FArcTerrainRuntimeCompletion>& LaneQueue =
				OwnerCompletions->Lanes[OwnerCompletions->DrainCursor];
			OwnerCompletions->DrainCursor = (OwnerCompletions->DrainCursor + 1) % LaneCount;
			if (LaneQueue.Num() == 0)
			{
				++IdleRounds;
				continue;
			}
			Batch.Add(MoveTemp(LaneQueue[0]));
			LaneQueue.RemoveAt(0, EAllowShrinking::No);
			--Remaining;
			IdleRounds = 0;
		}
		*Granted = Remaining;
		if (OwnerCompletions->PendingCount() == 0)
		{
			Completions.Remove(OwnerId);
		}
	}
	for (FArcTerrainRuntimeCompletion& Completion : Batch)
	{
		Consumer(MoveTemp(Completion));
	}
}

void FArcTerrainRuntimeWorkCoordinator::AbandonTerrain(FArcTerrainRuntimeOwnerId OwnerId, uint64 TerrainId)
{
	FScopeLock Lock(&Mutex);
	RemoveReadyJobs_NoLock(OwnerId, TerrainId, true);
	MarkInFlightAbandoned_NoLock(OwnerId, TerrainId, true);
	DiscardCompletions_NoLock(OwnerId, TerrainId, true);

	for (int32 OwnerIndex = 0; OwnerIndex < OwnerQueues.Num(); ++OwnerIndex)
	{
		FOwnerQueue& OwnerQueue = OwnerQueues[OwnerIndex];
		if (OwnerQueue.OwnerId != OwnerId)
		{
			continue;
		}
		TArray<int32> RemoveIndices;
		for (int32 TerrainIndex = 0; TerrainIndex < OwnerQueue.Terrains.Num(); ++TerrainIndex)
		{
			if (OwnerQueue.Terrains[TerrainIndex].TerrainId == TerrainId)
			{
				RemoveIndices.Add(TerrainIndex);
			}
		}
		for (int32 Reverse = RemoveIndices.Num() - 1; Reverse >= 0; --Reverse)
		{
			DropEmptyTerrainAt_NoLock(OwnerQueue, RemoveIndices[Reverse]);
		}
		if (OwnerQueue.Terrains.Num() == 0)
		{
			RemoveEmptyOwner_NoLock(OwnerId);
		}
		break;
	}
}

void FArcTerrainRuntimeWorkCoordinator::AbandonOwner(FArcTerrainRuntimeOwnerId OwnerId)
{
	FScopeLock Lock(&Mutex);
	RemoveReadyJobs_NoLock(OwnerId, 0, false);
	MarkInFlightAbandoned_NoLock(OwnerId, 0, false);
	DiscardCompletions_NoLock(OwnerId, 0, false);
	RemoveEmptyOwner_NoLock(OwnerId);
	TArray<FArcTerrainRuntimeOwnerTerrainLane> LimitKeys;
	for (const TPair<FArcTerrainRuntimeOwnerTerrainLane, int32>& Pair : LaneLimits)
	{
		if (Pair.Key.OwnerId == OwnerId)
		{
			LimitKeys.Add(Pair.Key);
		}
	}
	for (const FArcTerrainRuntimeOwnerTerrainLane& Key : LimitKeys)
	{
		LaneLimits.Remove(Key);
	}
	GrantedCredits.Remove(OwnerId);
	GrantOwnerOrder.Remove(OwnerId);
}

void FArcTerrainRuntimeWorkCoordinator::ShutdownAndWait()
{
	TArray<UE::Tasks::FTask> Tasks;
	{
		FScopeLock Lock(&Mutex);
		bShutdown = true;
		RemoveReadyJobs_NoLock(FArcTerrainRuntimeOwnerId(), 0, false);
		MarkInFlightAbandoned_NoLock(FArcTerrainRuntimeOwnerId(), 0, false);
		for (TPair<uint64, FInFlightJob>& Pair : InFlightJobs)
		{
			if (Pair.Value.Task.IsValid())
			{
				Tasks.Add(Pair.Value.Task);
			}
		}
	}

	for (UE::Tasks::FTask& Task : Tasks)
	{
		Task.Wait();
	}

	{
		FScopeLock Lock(&Mutex);
		TArray<FArcTerrainRuntimeOwnerId> Owners;
		Completions.GetKeys(Owners);
		for (const FArcTerrainRuntimeOwnerId& OwnerId : Owners)
		{
			DiscardCompletions_NoLock(OwnerId, 0, false);
		}
		Completions.Reset();
		GrantedCredits.Reset();
		GrantOwnerOrder.Reset();
		ReadyJobs.Reset();
		OwnerQueues.Reset();
	}

	FlushRenderingCommands();

	FScopeLock Lock(&Mutex);
	TArray<uint64> LeftoverIds;
	Reservations.GetKeys(LeftoverIds);
	for (const uint64 ReservationId : LeftoverIds)
	{
		ReleaseReservation_NoLock(ReservationId);
	}
	InFlightJobs.Reset();
	check(OwnedBytes == 0);
	check(LiveTasks == 0);
	check(Reservations.Num() == 0);
	check(PayloadRetains.Num() == 0);
}

void FArcTerrainRuntimeWorkCoordinator::ApplyReservationRelease(uint64 ReservationId)
{
	FScopeLock Lock(&Mutex);
	ReleaseReservation_NoLock(ReservationId);
}

bool FArcTerrainRuntimeWorkCoordinator::TryGrowReservation(uint64 ReservationId, uint64 AdditionalBytes)
{
	if (AdditionalBytes == 0)
	{
		return true;
	}
	FScopeLock Lock(&Mutex);
	FReservationRecord* Record = Reservations.Find(ReservationId);
	if (Record == nullptr || Record->bAbandoned)
	{
		return false;
	}
	if (OwnedBytes + AdditionalBytes > MaxOwnedBytes)
	{
		return false;
	}
	OwnedBytes += AdditionalBytes;
	Record->JobOwnedBytes += AdditionalBytes;
	return true;
}

bool FArcTerrainRuntimeWorkCoordinator::IsReservationAbandoned(uint64 ReservationId) const
{
	FScopeLock Lock(&Mutex);
	const FReservationRecord* Record = Reservations.Find(ReservationId);
	return Record == nullptr || Record->bAbandoned;
}

void FArcTerrainRuntimeWorkCoordinator::ReleaseReservation_NoLock(uint64 ReservationId)
{
	FReservationRecord Record;
	if (!Reservations.RemoveAndCopyValue(ReservationId, Record))
	{
		return;
	}
	if (OwnedBytes >= Record.JobOwnedBytes)
	{
		OwnedBytes -= Record.JobOwnedBytes;
	}
	else
	{
		OwnedBytes = 0;
	}
	ReleasePayloadIds_NoLock(Record.PayloadIds);
}

void FArcTerrainRuntimeWorkCoordinator::RetainPayloads_NoLock(const TArray<FArcTerrainRetainedPayload>& Payloads)
{
	for (const FArcTerrainRetainedPayload& Payload : Payloads)
	{
		FPayloadRetain& Retain = PayloadRetains.FindOrAdd(Payload.PayloadId);
		if (Retain.RefCount == 0)
		{
			Retain.Bytes = Payload.Bytes;
			Retain.Payload = Payload.Payload;
			OwnedBytes += Payload.Bytes;
		}
		++Retain.RefCount;
	}
}

void FArcTerrainRuntimeWorkCoordinator::ReleasePayloadIds_NoLock(TConstArrayView<uint32> PayloadIds)
{
	TArray<uint32> DropIds;
	for (const uint32 PayloadId : PayloadIds)
	{
		FPayloadRetain* Retain = PayloadRetains.Find(PayloadId);
		if (Retain == nullptr)
		{
			continue;
		}
		--Retain->RefCount;
		if (Retain->RefCount <= 0)
		{
			if (OwnedBytes >= Retain->Bytes)
			{
				OwnedBytes -= Retain->Bytes;
			}
			else
			{
				OwnedBytes = 0;
			}
			DropIds.Add(PayloadId);
		}
	}
	for (const uint32 PayloadId : DropIds)
	{
		PayloadRetains.Remove(PayloadId);
	}
}

FArcTerrainRuntimeWorkCoordinator::FOwnerQueue& FArcTerrainRuntimeWorkCoordinator::FindOrAddOwner_NoLock(
	FArcTerrainRuntimeOwnerId OwnerId)
{
	for (FOwnerQueue& Existing : OwnerQueues)
	{
		if (Existing.OwnerId == OwnerId)
		{
			return Existing;
		}
	}
	FOwnerQueue& Added = OwnerQueues.AddDefaulted_GetRef();
	Added.OwnerId = OwnerId;
	return Added;
}

FArcTerrainRuntimeWorkCoordinator::FTerrainQueue& FArcTerrainRuntimeWorkCoordinator::FindOrAddTerrain_NoLock(
	FOwnerQueue& OwnerQueue, uint64 TerrainId)
{
	for (FTerrainQueue& Existing : OwnerQueue.Terrains)
	{
		if (Existing.TerrainId == TerrainId)
		{
			return Existing;
		}
	}
	FTerrainQueue& Added = OwnerQueue.Terrains.AddDefaulted_GetRef();
	Added.TerrainId = TerrainId;
	return Added;
}

void FArcTerrainRuntimeWorkCoordinator::EnqueueReady_NoLock(uint64 ReservationId,
	FArcTerrainRuntimeOwnerId OwnerId, uint64 TerrainId, EArcTerrainRuntimeLane Lane)
{
	FOwnerQueue& OwnerQueue = FindOrAddOwner_NoLock(OwnerId);
	FTerrainQueue& TerrainQueue = FindOrAddTerrain_NoLock(OwnerQueue, TerrainId);
	FLaneQueue& LaneQueue = TerrainQueue.Lanes[ArcTerrainRuntimeWorkCoordinator_a81c::LaneIndex(Lane)];
	LaneQueue.JobIds.Add(ReservationId);
}

void FArcTerrainRuntimeWorkCoordinator::CompactLaneQueue_NoLock(FLaneQueue& Queue)
{
	if (Queue.Head <= 0)
	{
		return;
	}
	if (Queue.Head >= Queue.JobIds.Num())
	{
		Queue.JobIds.Reset();
		Queue.Head = 0;
		return;
	}
	Queue.JobIds.RemoveAt(0, Queue.Head, EAllowShrinking::No);
	Queue.Head = 0;
}

bool FArcTerrainRuntimeWorkCoordinator::IsTerrainQueueEmpty_NoLock(const FTerrainQueue& TerrainQueue) const
{
	for (int32 Lane = 0; Lane < static_cast<int32>(EArcTerrainRuntimeLane::Count); ++Lane)
	{
		const FLaneQueue& LaneQueue = TerrainQueue.Lanes[Lane];
		if (LaneQueue.Head < LaneQueue.JobIds.Num())
		{
			return false;
		}
	}
	return true;
}

void FArcTerrainRuntimeWorkCoordinator::DropEmptyTerrainAt_NoLock(FOwnerQueue& OwnerQueue, int32 TerrainIndex)
{
	if (!OwnerQueue.Terrains.IsValidIndex(TerrainIndex))
	{
		return;
	}
	OwnerQueue.Terrains.RemoveAt(TerrainIndex, EAllowShrinking::No);
	if (OwnerQueue.Terrains.Num() == 0)
	{
		OwnerQueue.TerrainCursor = 0;
		return;
	}
	if (OwnerQueue.TerrainCursor > TerrainIndex)
	{
		--OwnerQueue.TerrainCursor;
	}
	OwnerQueue.TerrainCursor %= OwnerQueue.Terrains.Num();
}

void FArcTerrainRuntimeWorkCoordinator::RemoveReadyJobs_NoLock(FArcTerrainRuntimeOwnerId OwnerId,
	uint64 TerrainIdFilter, bool bFilterTerrain)
{
	TArray<uint64> CancelIds;
	for (int32 OwnerIndex = 0; OwnerIndex < OwnerQueues.Num(); ++OwnerIndex)
	{
		FOwnerQueue& OwnerQueue = OwnerQueues[OwnerIndex];
		if (OwnerId.Value != 0 && OwnerQueue.OwnerId != OwnerId)
		{
			continue;
		}
		for (FTerrainQueue& TerrainQueue : OwnerQueue.Terrains)
		{
			if (bFilterTerrain && TerrainQueue.TerrainId != TerrainIdFilter)
			{
				continue;
			}
			for (int32 Lane = 0; Lane < static_cast<int32>(EArcTerrainRuntimeLane::Count); ++Lane)
			{
				FLaneQueue& LaneQueue = TerrainQueue.Lanes[Lane];
				for (int32 Index = LaneQueue.Head; Index < LaneQueue.JobIds.Num(); ++Index)
				{
					CancelIds.Add(LaneQueue.JobIds[Index]);
				}
				LaneQueue.JobIds.Reset();
				LaneQueue.Head = 0;
			}
		}
	}

	for (const uint64 ReservationId : CancelIds)
	{
		ReadyJobs.Remove(ReservationId);
		ReleaseReservation_NoLock(ReservationId);
	}

	if (OwnerId.Value == 0 && !bFilterTerrain)
	{
		OwnerQueues.Reset();
		OwnerCursor = 0;
	}
}

void FArcTerrainRuntimeWorkCoordinator::DiscardCompletions_NoLock(FArcTerrainRuntimeOwnerId OwnerId,
	uint64 TerrainIdFilter, bool bFilterTerrain)
{
	if (OwnerId.Value == 0 && !bFilterTerrain)
	{
		for (TPair<FArcTerrainRuntimeOwnerId, FOwnerCompletions>& Pair : Completions)
		{
			for (int32 Lane = 0; Lane < static_cast<int32>(EArcTerrainRuntimeLane::Count); ++Lane)
			{
				for (FArcTerrainRuntimeCompletion& Completion : Pair.Value.Lanes[Lane])
				{
					const uint64 ReservationId = Completion.Reservation.Detach();
					ReleaseReservation_NoLock(ReservationId);
				}
			}
		}
		Completions.Reset();
		return;
	}

	FOwnerCompletions* OwnerCompletions = Completions.Find(OwnerId);
	if (OwnerCompletions == nullptr)
	{
		return;
	}
	for (int32 Lane = 0; Lane < static_cast<int32>(EArcTerrainRuntimeLane::Count); ++Lane)
	{
		TArray<FArcTerrainRuntimeCompletion>& LaneQueue = OwnerCompletions->Lanes[Lane];
		TArray<int32> RemoveIndices;
		for (int32 Index = 0; Index < LaneQueue.Num(); ++Index)
		{
			if (!bFilterTerrain || LaneQueue[Index].TerrainId == TerrainIdFilter)
			{
				RemoveIndices.Add(Index);
			}
		}
		for (int32 Reverse = RemoveIndices.Num() - 1; Reverse >= 0; --Reverse)
		{
			const uint64 ReservationId = LaneQueue[RemoveIndices[Reverse]].Reservation.Detach();
			ReleaseReservation_NoLock(ReservationId);
			LaneQueue.RemoveAt(RemoveIndices[Reverse], EAllowShrinking::No);
		}
	}
	if (OwnerCompletions->PendingCount() == 0)
	{
		Completions.Remove(OwnerId);
	}
}

void FArcTerrainRuntimeWorkCoordinator::MarkInFlightAbandoned_NoLock(FArcTerrainRuntimeOwnerId OwnerId,
	uint64 TerrainIdFilter, bool bFilterTerrain)
{
	for (TPair<uint64, FInFlightJob>& Pair : InFlightJobs)
	{
		FQueuedJob& Job = *Pair.Value.Job;
		if (OwnerId.Value != 0 && Job.OwnerId != OwnerId)
		{
			continue;
		}
		if (bFilterTerrain && Job.TerrainId != TerrainIdFilter)
		{
			continue;
		}
		Job.bAbandoned.store(true, std::memory_order_release);
		if (FReservationRecord* Record = Reservations.Find(Job.ReservationId))
		{
			Record->bAbandoned = true;
		}
	}
}

void FArcTerrainRuntimeWorkCoordinator::CompleteJob(uint64 ReservationId, FArcTerrainRuntimeResult&& Result)
{
	FScopeLock Lock(&Mutex);
	FInFlightJob InFlight;
	InFlightJobs.RemoveAndCopyValue(ReservationId, InFlight);
	if (LiveTasks > 0)
	{
		--LiveTasks;
	}

	TSharedPtr<FQueuedJob, ESPMode::ThreadSafe> Job = InFlight.Job;
	const bool bAbandoned = Job.IsValid() && Job->bAbandoned.load(std::memory_order_acquire);
	if (!Job.IsValid() || bAbandoned || bShutdown)
	{
		ReleaseReservation_NoLock(ReservationId);
		return;
	}

	FArcTerrainRuntimeCompletion Completion;
	Completion.OwnerId = Job->OwnerId;
	Completion.TerrainId = Job->TerrainId;
	Completion.Lane = Job->Lane;
	Completion.Status = Result.Status;
	Completion.Payload = MoveTemp(Result.Payload);
	Completion.Reservation = MakeToken(ReservationId);
	FOwnerCompletions& OwnerCompletions = Completions.FindOrAdd(Job->OwnerId);
	if (OwnerCompletions.PendingCount() == 0)
	{
		GrantOwnerOrder.AddUnique(Job->OwnerId);
	}
	OwnerCompletions.Lanes[ArcTerrainRuntimeWorkCoordinator_a81c::LaneIndex(Job->Lane)].Add(
		MoveTemp(Completion));
}

void FArcTerrainRuntimeWorkCoordinator::GrantCompletions_NoLock()
{
	if (CompletionGrantsPerFrame <= 0 || GrantOwnerOrder.Num() == 0)
	{
		return;
	}

	TArray<int32> EmptyOwners;
	int32 Remaining = CompletionGrantsPerFrame;
	int32 IdleRounds = 0;
	const int32 OwnerCount = GrantOwnerOrder.Num();
	while (Remaining > 0 && IdleRounds < OwnerCount)
	{
		if (GrantOwnerOrder.Num() == 0)
		{
			break;
		}
		GrantCursor = GrantCursor % GrantOwnerOrder.Num();
		const FArcTerrainRuntimeOwnerId OwnerId = GrantOwnerOrder[GrantCursor];
		const FOwnerCompletions* OwnerCompletions = Completions.Find(OwnerId);
		const int32 AlreadyGranted = GrantedCredits.FindRef(OwnerId);
		const int32 Pending = OwnerCompletions != nullptr ? OwnerCompletions->PendingCount() : 0;
		if (Pending > AlreadyGranted)
		{
			GrantedCredits.FindOrAdd(OwnerId) = AlreadyGranted + 1;
			--Remaining;
			IdleRounds = 0;
		}
		else
		{
			++IdleRounds;
		}
		++GrantCursor;
	}
}

int32 FArcTerrainRuntimeWorkCoordinator::GetLaneLimit_NoLock(FArcTerrainRuntimeOwnerId OwnerId,
	uint64 TerrainId, EArcTerrainRuntimeLane Lane) const
{
	const FArcTerrainRuntimeOwnerTerrainLane Key{ OwnerId, TerrainId, Lane };
	if (const int32* Override = LaneLimits.Find(Key))
	{
		return *Override;
	}
	return DefaultLaneLimits[ArcTerrainRuntimeWorkCoordinator_a81c::LaneIndex(Lane)];
}

void FArcTerrainRuntimeWorkCoordinator::LaunchReadyJobs_NoLock()
{
	if (OwnerQueues.Num() == 0)
	{
		return;
	}

	int32 IdleRounds = 0;
	const int32 MaxIdle = FMath::Max(OwnerQueues.Num() * 3, 1);
	while (LiveTasks < MaxTasks && IdleRounds < MaxIdle)
	{
		if (OwnerQueues.Num() == 0)
		{
			break;
		}
		OwnerCursor = OwnerCursor % OwnerQueues.Num();
		FOwnerQueue& OwnerQueue = OwnerQueues[OwnerCursor];
		if (OwnerQueue.Terrains.Num() == 0)
		{
			const FArcTerrainRuntimeOwnerId EmptyOwnerId = OwnerQueue.OwnerId;
			RemoveEmptyOwner_NoLock(EmptyOwnerId);
			continue;
		}
		OwnerQueue.TerrainCursor = OwnerQueue.TerrainCursor % OwnerQueue.Terrains.Num();
		FTerrainQueue& TerrainQueue = OwnerQueue.Terrains[OwnerQueue.TerrainCursor];
		for (int32 LaneIndex = 0; LaneIndex < static_cast<int32>(EArcTerrainRuntimeLane::Count); ++LaneIndex)
		{
			CompactLaneQueue_NoLock(TerrainQueue.Lanes[LaneIndex]);
		}
		if (IsTerrainQueueEmpty_NoLock(TerrainQueue))
		{
			DropEmptyTerrainAt_NoLock(OwnerQueue, OwnerQueue.TerrainCursor);
			if (OwnerQueue.Terrains.Num() == 0)
			{
				const FArcTerrainRuntimeOwnerId EmptyOwnerId = OwnerQueue.OwnerId;
				RemoveEmptyOwner_NoLock(EmptyOwnerId);
			}
			continue;
		}
		TerrainQueue.LaneCursor = TerrainQueue.LaneCursor % static_cast<int32>(EArcTerrainRuntimeLane::Count);
		const EArcTerrainRuntimeLane Lane =
			static_cast<EArcTerrainRuntimeLane>(TerrainQueue.LaneCursor);
		FLaneQueue& LaneQueue = TerrainQueue.Lanes[TerrainQueue.LaneCursor];

		const FArcTerrainRuntimeOwnerTerrainLane LimitKey{ OwnerQueue.OwnerId, TerrainQueue.TerrainId, Lane };
		const int32 Limit = GetLaneLimit_NoLock(OwnerQueue.OwnerId, TerrainQueue.TerrainId, Lane);
		const int32 AlreadyLaunched = LaunchedThisFrame.FindRef(LimitKey);
		uint64 ReservationId = 0;
		if (LaneQueue.Head < LaneQueue.JobIds.Num() && AlreadyLaunched < Limit)
		{
			ReservationId = LaneQueue.JobIds[LaneQueue.Head];
			++LaneQueue.Head;
		}

		TerrainQueue.LaneCursor =
			(TerrainQueue.LaneCursor + 1) % static_cast<int32>(EArcTerrainRuntimeLane::Count);
		if (TerrainQueue.LaneCursor == 0)
		{
			OwnerQueue.TerrainCursor = (OwnerQueue.TerrainCursor + 1) % OwnerQueue.Terrains.Num();
			if (OwnerQueue.TerrainCursor == 0)
			{
				OwnerCursor = (OwnerCursor + 1) % OwnerQueues.Num();
			}
		}

		if (ReservationId == 0)
		{
			++IdleRounds;
			continue;
		}

		TSharedPtr<FQueuedJob, ESPMode::ThreadSafe> Job;
		if (!ReadyJobs.RemoveAndCopyValue(ReservationId, Job) || !Job.IsValid())
		{
			++IdleRounds;
			continue;
		}

		IdleRounds = 0;
		LaunchedThisFrame.FindOrAdd(LimitKey) = AlreadyLaunched + 1;
		++LiveTasks;

		TWeakPtr<FArcTerrainRuntimeWorkCoordinator, ESPMode::ThreadSafe> WeakSelf = AsShared();
		const uint64 PreReserved = Job->PreReservedResultBytes;
		FInFlightJob& InFlight = InFlightJobs.Add(ReservationId);
		InFlight.Job = Job;
		InFlight.Task = UE::Tasks::Launch(UE_SOURCE_LOCATION,
			[Job, WeakSelf, PreReserved]()
			{
				FArcTerrainRuntimeResult Result;
				if (Job->bAbandoned.load(std::memory_order_acquire))
				{
					Result.Status = EArcTerrainRuntimeWorkStatus::Cancelled;
				}
				else if (TSharedPtr<FArcTerrainRuntimeWorkCoordinator, ESPMode::ThreadSafe> Coordinator =
					WeakSelf.Pin())
				{
					FArcTerrainRuntimeWorkContext Context;
					Context.Coordinator = Coordinator.Get();
					Context.ReservationId = Job->ReservationId;
					Context.RemainingPreReserve = PreReserved;
					Result = Job->Work(Context);
				}
				else
				{
					Result.Status = EArcTerrainRuntimeWorkStatus::Cancelled;
				}
				if (TSharedPtr<FArcTerrainRuntimeWorkCoordinator, ESPMode::ThreadSafe> Coordinator =
					WeakSelf.Pin())
				{
					Coordinator->CompleteJob(Job->ReservationId, MoveTemp(Result));
				}
			},
			Job->TaskPriority);
	}
}

void FArcTerrainRuntimeWorkCoordinator::RemoveEmptyOwner_NoLock(FArcTerrainRuntimeOwnerId OwnerId)
{
	TArray<int32> RemoveIndices;
	for (int32 Index = 0; Index < OwnerQueues.Num(); ++Index)
	{
		if (OwnerQueues[Index].OwnerId == OwnerId)
		{
			RemoveIndices.Add(Index);
		}
	}
	for (int32 Reverse = RemoveIndices.Num() - 1; Reverse >= 0; --Reverse)
	{
		OwnerQueues.RemoveAt(RemoveIndices[Reverse], EAllowShrinking::No);
	}
	if (OwnerCursor >= OwnerQueues.Num())
	{
		OwnerCursor = 0;
	}
}
