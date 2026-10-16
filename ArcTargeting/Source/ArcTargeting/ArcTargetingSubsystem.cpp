#include "ArcTargetingSubsystem.h"

#include "ArcTargetingModule.h"
#include "ArcTargetingPreset.h"
#include "Nodes/ArcTargetingNode.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Class.h"

namespace
{
	// Per-frame async drain budget. The pipeline is atomic per request, so this
	// caps how many requests start in a frame, not how long any single one runs.
	TAutoConsoleVariable<float> CVarMaxAsyncTickTime(
		TEXT("arc.Targeting.MaxAsyncTickTime"),
		0.00005f,
		TEXT("Max wall-clock seconds spent draining the async targeting queue per Tick (default 0.00005s = 0.05ms)."),
		ECVF_Default);

	// Force-on floor for per-request debug mode. max(CVar, RequestMode) wins.
	TAutoConsoleVariable<int32> CVarDebug(
		TEXT("arc.Targeting.Debug"),
		0,
		TEXT("Force-on floor for per-request targeting debug mode. The effective mode is max(CVar, RequestMode).\n")
		TEXT("  0 = let request decide (no forced drawing)\n")
		TEXT("  1 = force at least Live (one-frame draw on execute)\n")
		TEXT("  2 = force at least Persist (capture snapshot, replay every Tick)"),
		ECVF_Default);

#if UE_DEBUG_RENDERING_ENABLED
	// Maximum number of completed (non-persistent) requests retained in the
	// debugger history ring buffer. Drop oldest when over cap.
	TAutoConsoleVariable<int32> CVarHistorySize(
		TEXT("arc.Targeting.Debugger.HistorySize"),
		64,
		TEXT("Max number of completed targeting requests retained in the debugger history (drops oldest when over)."),
		ECVF_Default);
#endif
}


UArcTargetingSubsystem* UArcTargetingSubsystem::Get(const UObject* WorldContext)
{
	if (WorldContext == nullptr)
	{
		return nullptr;
	}
	const UWorld* World = WorldContext->GetWorld();
	return World ? World->GetSubsystem<UArcTargetingSubsystem>() : nullptr;
}

bool UArcTargetingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld() || World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview;
	}
	return false;
}

void UArcTargetingSubsystem::Tick(float DeltaTime)
{
	const double Deadline = FPlatformTime::Seconds() + (double)CVarMaxAsyncTickTime.GetValueOnGameThread();
	while (AsyncQueue.Num() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		const uint64 Id = AsyncQueue[0];
		AsyncQueue.RemoveAt(0, 1, EAllowShrinking::No);
		FArcTargetingRequest* Request = Requests.Find(Id);
		if (Request == nullptr)
		{
			continue;
		}
		if (Request->bCancelled || Request->State == EArcTargetingState::Cancelled)
		{
			continue;
		}
		if (Request->bPersistent && !PreparePersistentRequest(FArcTargetingHandle(Id), *Request))
		{
			continue;
		}
		ExecuteRequestAtomic(FArcTargetingHandle(Id), *Request);
		Request = Requests.Find(Id);
		if (Request == nullptr)
		{
			continue;
		}
		if (Request->bRequeueOnCompletion && !Request->bCancelled)
		{
			// Reset for the next pass — keep Preset/OnCompleted/flags, drop results.
			// DebugSnapshot is intentionally NOT reset here: persist-replay below
			// needs it to survive the Pending interval between requeue iterations.
			// ExecuteRequestAtomic resets it at the start of the next pass when bCapture is on.
			Request->State = EArcTargetingState::Pending;
			Request->Context = Request->SeedContext;
			AsyncQueue.Add(Id);
		}
	}

	// Persistent registry — re-enqueue throttled entries whose interval has elapsed.
	{
		const double Now = FPlatformTime::Seconds();
		TArray<FArcTargetingPersistentKey> InvalidKeys;
		for (TPair<FArcTargetingPersistentKey, FArcTargetingPersistentEntry>& Pair : PersistentEntries)
		{
			FArcTargetingPersistentEntry& Entry = Pair.Value;
			if (!IsEntityValid(Entry.Key.OwnerEntity))
			{
				InvalidKeys.Add(Entry.Key);
				continue;
			}
			if (Entry.MinInterval <= 0.f) { continue; }
			if (Entry.bRunPending) { continue; }
			if (Entry.LastCompletedSeconds >= 0.0 && (Now - Entry.LastCompletedSeconds) < (double)Entry.MinInterval) { continue; }
			FArcTargetingRequest* Request = Requests.Find(Entry.Handle.GetId());
			if (Request == nullptr) { continue; }
			// Reset for re-run; OnCompleted delegate stays bound from PushPersistentTargeting.
			Request->State = EArcTargetingState::Pending;
			Request->Context = Request->SeedContext;
			Request->bCancelled = false;
			Entry.bRunPending = true;
			AsyncQueue.Add(Entry.Handle.GetId());
		}
		for (const FArcTargetingPersistentKey& Key : InvalidKeys)
		{
			RemovePersistentTargeting(Key.OwnerEntity, Key.Tag);
		}

		TArray<FArcTargetingPersistentKey> RemovedPublishedKeys;
		for (TMap<FArcTargetingPersistentKey, TArray<FArcTargetCandidate>>::TIterator It = PublishedResults.CreateIterator(); It; ++It)
		{
			if (!IsEntityValid(It.Key().OwnerEntity))
			{
				RemovedPublishedKeys.Add(It.Key());
				It.RemoveCurrent();
			}
		}
		for (const FArcTargetingPersistentKey& Key : RemovedPublishedKeys)
		{
			BroadcastResultChanged(Key);
		}

		for (TPair<FArcTargetingPersistentKey, FArcTargetingPersistentEntry>& Pair : PersistentEntries)
		{
			FArcTargetingPersistentEntry& Entry = Pair.Value;
			if (Entry.MinInterval > 0.f || Entry.bRunPending)
			{
				continue;
			}
			FArcTargetingRequest* Request = Requests.Find(Entry.Handle.GetId());
			if (Request == nullptr)
			{
				continue;
			}
			Request->State = EArcTargetingState::Pending;
			Request->bCancelled = false;
			Entry.bRunPending = true;
			AsyncQueue.Add(Entry.Handle.GetId());
		}
	}

#if UE_DEBUG_RENDERING_ENABLED
	// Persist replay — re-emit captured op DrawDebug calls every frame.
	// Don't gate on State==Done: bRequeueOnCompletion requests flip back to
	// Pending immediately after Execute, so the snapshot from the latest
	// completed pass is what we want to replay during the Pending interval.
	for (TPair<uint64, FArcTargetingRequest>& Pair : Requests)
	{
		FArcTargetingRequest& Request = Pair.Value;
		if (Request.bCancelled)
		{
			continue;
		}
		if (ResolveDebugMode(Request) != EArcTargetingDebugMode::Persist)
		{
			continue;
		}
		if (!Request.Preset.IsValid())
		{
			continue;
		}

		const FArcTargetingContext& Ctx = Request.Context;
		const FArcTargetingDebugSnapshot& Snap = Request.DebugSnapshot;
		const UArcTargetingPreset* Preset = Request.Preset.Get();
		if (Preset == nullptr)
		{
			continue;
		}

		for (const FArcTargetingDebugNodeEntry& E : Snap.Nodes)
		{
			if (!Preset->Nodes.IsValidIndex(E.NodeIndex))
			{
				continue;
			}
			const FArcTargetingNode* Node = Preset->Nodes[E.NodeIndex].GetPtr<FArcTargetingNode>();
			if (Node == nullptr || !Node->bDrawDebug)
			{
				continue;
			}
			FArcTargetingContext ReplayCtx = Ctx;
			ReplayCtx.SourceLocation = E.SourceLocationAfter;
			ReplayCtx.SourceDirection = E.SourceDirectionAfter;
			Node->DrawDebug(ReplayCtx, MakeArrayView(E.CandidatesBefore), MakeArrayView(E.CandidatesAfter));
		}
	}
#endif
}

TStatId UArcTargetingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcTargetingSubsystem, STATGROUP_Tickables);
}

FArcTargetingHandle UArcTargetingSubsystem::MakeRequest(const UArcTargetingPreset* Preset, const FArcTargetingContext& SeedContext)
{
	const uint64 Id = NextHandleId++;
	FArcTargetingRequest& Slot = Requests.Add(Id);
	Slot.Preset = Preset;
	Slot.SeedContext = SeedContext;
	Slot.Context = SeedContext;
	Slot.State = EArcTargetingState::Pending;
	return FArcTargetingHandle(Id);
}

void UArcTargetingSubsystem::ExecuteSync(FArcTargetingHandle Handle)
{
	FArcTargetingRequest* Request = Requests.Find(Handle.GetId());
	if (Request == nullptr)
	{
		return;
	}
	if (Request->State == EArcTargetingState::Done || Request->State == EArcTargetingState::Cancelled)
	{
		return;
	}
	ExecuteRequestAtomic(Handle, *Request);
}

void UArcTargetingSubsystem::StartAsync(FArcTargetingHandle Handle, FArcTargetingCompletedDelegate OnCompleted, bool bRequeueOnCompletion)
{
	FArcTargetingRequest* Request = Requests.Find(Handle.GetId());
	if (Request == nullptr)
	{
		return;
	}
	if (Request->State == EArcTargetingState::Done || Request->State == EArcTargetingState::Cancelled)
	{
		return;
	}
	Request->OnCompleted = OnCompleted;
	Request->bRequeueOnCompletion = bRequeueOnCompletion;
	AsyncQueue.Add(Handle.GetId());
}

void UArcTargetingSubsystem::Cancel(FArcTargetingHandle Handle)
{
	FArcTargetingRequest* Request = Requests.Find(Handle.GetId());
	if (Request == nullptr)
	{
		return;
	}
	Request->bCancelled = true;
	if (Request->State == EArcTargetingState::Pending)
	{
		Request->State = EArcTargetingState::Cancelled;
		AsyncQueue.Remove(Handle.GetId());
	}
	// If Running, the in-flight pipeline observes bCancelled and suppresses the delegate.
}

void UArcTargetingSubsystem::Release(FArcTargetingHandle Handle)
{
	AsyncQueue.Remove(Handle.GetId());
	Requests.Remove(Handle.GetId());
}

const FArcTargetingContext* UArcTargetingSubsystem::GetContext(FArcTargetingHandle Handle) const
{
	const FArcTargetingRequest* Request = Requests.Find(Handle.GetId());
	return Request ? &Request->Context : nullptr;
}

FArcTargetingContext* UArcTargetingSubsystem::GetMutableContext(FArcTargetingHandle Handle)
{
	FArcTargetingRequest* Request = Requests.Find(Handle.GetId());
	return Request ? &Request->Context : nullptr;
}

EArcTargetingState UArcTargetingSubsystem::GetState(FArcTargetingHandle Handle) const
{
	const FArcTargetingRequest* Request = Requests.Find(Handle.GetId());
	return Request ? Request->State : EArcTargetingState::Cancelled;
}

void UArcTargetingSubsystem::SetDebugMode(FArcTargetingHandle Handle, EArcTargetingDebugMode Mode)
{
	if (FArcTargetingRequest* Request = Requests.Find(Handle.GetId()))
	{
		Request->DebugMode = Mode;
	}
}

EArcTargetingDebugMode UArcTargetingSubsystem::ResolveDebugMode(const FArcTargetingRequest& Request) const
{
	const int32 CVarValue = FMath::Clamp(CVarDebug.GetValueOnGameThread(), 0, 2);
	const EArcTargetingDebugMode CVarMode = static_cast<EArcTargetingDebugMode>(CVarValue);
	return (static_cast<uint8>(CVarMode) > static_cast<uint8>(Request.DebugMode)) ? CVarMode : Request.DebugMode;
}

void UArcTargetingSubsystem::ExecuteRequestAtomic(FArcTargetingHandle Handle, FArcTargetingRequest& Request)
{
	if (Request.bCancelled)
	{
		Request.State = EArcTargetingState::Cancelled;
		return;
	}

	Request.State = EArcTargetingState::Running;

	const UArcTargetingPreset* Preset = Request.Preset.Get();
	if (Preset == nullptr)
	{
		Request.State = EArcTargetingState::Done;
		if (!Request.bCancelled)
		{
			const FArcTargetingCompletedDelegate Completion = Request.OnCompleted;
			Completion.ExecuteIfBound(Handle);
		}
		return;
	}

	FArcTargetingContext& Ctx = Request.Context;

#if UE_DEBUG_RENDERING_ENABLED
	const EArcTargetingDebugMode EffectiveMode = ResolveDebugMode(Request);
	const bool bDoDraw = EffectiveMode != EArcTargetingDebugMode::Off;
	const bool bCapture = EffectiveMode == EArcTargetingDebugMode::Persist;
	const bool bRecordHistory = !Request.bPersistent && IsHistoryRecordingEnabled();
	if (bCapture)
	{
		Request.DebugSnapshot.Reset();
	}
	if (bRecordHistory)
	{
		Request.DebugTrace = FArcTargetingDebugTrace();
		Request.DebugTrace.Preset = Request.Preset;
		Request.DebugTrace.SourceActor = Ctx.SourceActor.Get();
		Request.DebugTrace.InitialCursor = Ctx.SourceLocation;
	}
#endif

	for (int32 NodeIndex = 0; NodeIndex < Preset->Nodes.Num(); ++NodeIndex)
	{
		const FArcTargetingNode* Node = Preset->Nodes[NodeIndex].GetPtr<FArcTargetingNode>();
		if (Node == nullptr)
		{
			continue;
		}

#if UE_DEBUG_RENDERING_ENABLED
		const bool bRecord = bDoDraw || bCapture || bRecordHistory;
		FArcTargetingDebugNodeEntry Entry;
		if (bRecord)
		{
			Entry.NodeIndex = NodeIndex;
			Entry.CandidatesBefore = Ctx.Candidates;
		}
#endif

		Node->Execute(Ctx, Ctx.Candidates);

#if UE_DEBUG_RENDERING_ENABLED
		if (bRecord)
		{
			Entry.CandidatesAfter = Ctx.Candidates;
			Entry.SourceLocationAfter = Ctx.SourceLocation;
			Entry.SourceDirectionAfter = Ctx.SourceDirection;

			if (bDoDraw && Node->bDrawDebug)
			{
				Node->DrawDebug(Ctx, MakeArrayView(Entry.CandidatesBefore), MakeArrayView(Entry.CandidatesAfter));
			}
			if (bCapture)
			{
				Request.DebugSnapshot.Nodes.Add(Entry);
			}
			if (bRecordHistory)
			{
				Request.DebugTrace.Nodes.Add(MoveTemp(Entry));
			}
		}
#endif
	}

	if (Request.bCancelled)
	{
		Request.State = EArcTargetingState::Cancelled;
		return;
	}

	Request.State = EArcTargetingState::Done;

#if UE_DEBUG_RENDERING_ENABLED
	if (bRecordHistory)
	{
		Request.DebugTrace.CompletedRealTimeSeconds = FPlatformTime::Seconds();
		Request.DebugTrace.FinalCandidates = Ctx.Candidates;

		const int32 Cap = FMath::Max(1, CVarHistorySize.GetValueOnGameThread());
		while (RecentTraces.Num() >= Cap)
		{
			RecentTraces.RemoveAt(0, 1, EAllowShrinking::No);
		}
		const int32 NewIndex = RecentTraces.Add(MoveTemp(Request.DebugTrace));
		OnRequestRecordedForDebug().Broadcast(RecentTraces[NewIndex]);
	}
#endif

	FArcTargetingRequest* CompletedRequest = Requests.Find(Handle.GetId());
	if (CompletedRequest == nullptr || CompletedRequest->bCancelled)
	{
		return;
	}
	const FArcTargetingCompletedDelegate Completion = CompletedRequest->OnCompleted;
	Completion.ExecuteIfBound(Handle);
}

void UArcTargetingSubsystem::Deinitialize()
{
	for (TPair<FArcTargetingPersistentKey, FArcTargetingPersistentEntry>& Pair : PersistentEntries)
	{
		Cancel(Pair.Value.Handle);
		Release(Pair.Value.Handle);
	}
	PersistentEntries.Empty();
	PublishedResults.Empty();
	ResultChannels.Empty();
	AsyncQueue.Empty();
	Requests.Empty();
	Super::Deinitialize();
}

bool UArcTargetingSubsystem::PushPersistentTargeting(
	FMassEntityHandle OwnerEntity,
	FGameplayTag Tag,
	const UArcTargetingPreset* Preset,
	float MinInterval)
{
	check(IsInGameThread());
	if (!IsEntityValid(OwnerEntity) || !Tag.IsValid() || Preset == nullptr || MinInterval < 0.f)
	{
		return false;
	}

	const FArcTargetingPersistentKey Key{OwnerEntity, Tag};
	if (!ensure(!PublishedResults.Contains(Key)))
	{
		UE_LOGF(LogArcTargeting, Warning, "Refused persistent targeting on %ls for entity %ls: the key already holds a published result.", *Tag.ToString(), *OwnerEntity.DebugGetDescription());
		return false;
	}
	bool bReplaced = false;
	if (FArcTargetingPersistentEntry* Existing = PersistentEntries.Find(Key))
	{
		Cancel(Existing->Handle);
		Release(Existing->Handle);
		PersistentEntries.Remove(Key);
		bReplaced = true;
	}

	const FArcTargetingHandle NewHandle = MakeRequest(Preset, FArcTargetingContext());
	if (FArcTargetingRequest* NewRequest = Requests.Find(NewHandle.GetId()))
	{
		NewRequest->bPersistent = true;
		NewRequest->PersistentKey = Key;
	}

	FArcTargetingPersistentEntry Entry;
	Entry.Key = Key;
	Entry.Handle = NewHandle;
	Entry.Preset.Reset(Preset);
	Entry.MinInterval = MinInterval;
	Entry.LastCompletedSeconds = -1.0;
	Entry.bRunPending = true;
	PersistentEntries.Add(Key, MoveTemp(Entry));

	StartAsync(
		NewHandle,
		FArcTargetingCompletedDelegate::CreateUObject(this, &UArcTargetingSubsystem::OnPersistentCompleted, Key),
		false);

	if (bReplaced)
	{
		BroadcastResultChanged(Key);
	}

	return true;
}

void UArcTargetingSubsystem::RemovePersistentTargeting(FMassEntityHandle OwnerEntity, FGameplayTag Tag)
{
	check(IsInGameThread());
	const FArcTargetingPersistentKey Key{OwnerEntity, Tag};
	FArcTargetingPersistentEntry* Entry = PersistentEntries.Find(Key);
	if (Entry == nullptr)
	{
		return;
	}
	const FArcTargetingHandle HandleCopy = Entry->Handle;
	Cancel(HandleCopy);
	Release(HandleCopy);
	PersistentEntries.Remove(Key);
	BroadcastResultChanged(Key);
}

bool UArcTargetingSubsystem::HasPersistentTargeting(FMassEntityHandle OwnerEntity, FGameplayTag Tag) const
{
	check(IsInGameThread());
	return IsEntityValid(OwnerEntity) && PersistentEntries.Contains(FArcTargetingPersistentKey{OwnerEntity, Tag});
}

void UArcTargetingSubsystem::PublishResult(FMassEntityHandle OwnerEntity, FGameplayTag Tag, TArray<FArcTargetCandidate> Candidates)
{
	check(IsInGameThread());
	if (!IsEntityValid(OwnerEntity) || !Tag.IsValid())
	{
		return;
	}

	const FArcTargetingPersistentKey Key{OwnerEntity, Tag};
	if (!ensure(!PersistentEntries.Contains(Key)))
	{
		UE_LOGF(LogArcTargeting, Warning, "Refused publication on %ls for entity %ls: the key is driven by a persistent preset.", *Tag.ToString(), *OwnerEntity.DebugGetDescription());
		return;
	}

	UE_LOGF(LogArcTargeting, Verbose, "PublishResult %ls entity %ls: %d candidates.", *Tag.ToString(), *OwnerEntity.DebugGetDescription(), Candidates.Num());
	PublishedResults.Add(Key, MoveTemp(Candidates));
	BroadcastResultChanged(Key);
}

void UArcTargetingSubsystem::ClearPublishedResult(FMassEntityHandle OwnerEntity, FGameplayTag Tag)
{
	check(IsInGameThread());
	const FArcTargetingPersistentKey Key{OwnerEntity, Tag};
	TArray<FArcTargetCandidate> Removed;
	if (PublishedResults.RemoveAndCopyValue(Key, Removed))
	{
		UE_LOGF(LogArcTargeting, Verbose, "ClearPublishedResult %ls entity %ls: dropped %d candidates.", *Tag.ToString(), *OwnerEntity.DebugGetDescription(), Removed.Num());
		BroadcastResultChanged(Key);
	}
}

const TArray<FArcTargetCandidate>* UArcTargetingSubsystem::GetLatestPersistentResult(FMassEntityHandle OwnerEntity, FGameplayTag Tag) const
{
	check(IsInGameThread());
	if (!IsEntityValid(OwnerEntity))
	{
		return nullptr;
	}
	const FArcTargetingPersistentKey Key{OwnerEntity, Tag};
	if (const TArray<FArcTargetCandidate>* Published = PublishedResults.Find(Key))
	{
		return Published;
	}
	const FArcTargetingPersistentEntry* Entry = PersistentEntries.Find(Key);
	return Entry != nullptr && Entry->bHasCompleted ? &Entry->LatestCandidates : nullptr;
}

void UArcTargetingSubsystem::GetPersistentKeys(TArray<FArcTargetingPersistentKey>& OutKeys) const
{
	check(IsInGameThread());
	OutKeys.Reset(PersistentEntries.Num() + PublishedResults.Num());
	for (const TPair<FArcTargetingPersistentKey, FArcTargetingPersistentEntry>& Pair : PersistentEntries)
	{
		if (IsEntityValid(Pair.Key.OwnerEntity))
		{
			OutKeys.Add(Pair.Key);
		}
	}
	for (const TPair<FArcTargetingPersistentKey, TArray<FArcTargetCandidate>>& Pair : PublishedResults)
	{
		if (IsEntityValid(Pair.Key.OwnerEntity))
		{
			OutKeys.Add(Pair.Key);
		}
	}
}

FDelegateHandle UArcTargetingSubsystem::SubscribeResult(FMassEntityHandle OwnerEntity, FGameplayTag Tag, FArcTargetingResultChanged::FDelegate Delegate)
{
	check(IsInGameThread());
	const FArcTargetingPersistentKey Key{OwnerEntity, Tag};
	TSharedRef<FArcTargetingResultChanged>* Channel = ResultChannels.Find(Key);
	if (Channel == nullptr)
	{
		Channel = &ResultChannels.Add(Key, MakeShared<FArcTargetingResultChanged>());
	}
	return (*Channel)->Add(MoveTemp(Delegate));
}

void UArcTargetingSubsystem::UnsubscribeResult(FMassEntityHandle OwnerEntity, FGameplayTag Tag, FDelegateHandle Handle)
{
	check(IsInGameThread());
	const FArcTargetingPersistentKey Key{OwnerEntity, Tag};
	TSharedRef<FArcTargetingResultChanged>* Channel = ResultChannels.Find(Key);
	if (Channel == nullptr)
	{
		return;
	}
	(*Channel)->Remove(Handle);
	if (!(*Channel)->IsBound())
	{
		ResultChannels.Remove(Key);
	}
}

#if UE_DEBUG_RENDERING_ENABLED
void UArcTargetingSubsystem::SetHistoryRecordingRefCount(int32 Delta)
{
	HistoryRecordingRefCount = FMath::Max(0, HistoryRecordingRefCount + Delta);
}

TConstArrayView<FArcTargetingDebugTrace> UArcTargetingSubsystem::GetRecentTraces() const
{
	return MakeArrayView(RecentTraces);
}

void UArcTargetingSubsystem::ClearRecentTraces()
{
	RecentTraces.Reset();
}

FArcOnTargetingRequestRecordedForDebug& UArcTargetingSubsystem::OnRequestRecordedForDebug()
{
	static FArcOnTargetingRequestRecordedForDebug Instance;
	return Instance;
}
#endif // UE_DEBUG_RENDERING_ENABLED

bool UArcTargetingSubsystem::IsEntityValid(FMassEntityHandle OwnerEntity) const
{
	UWorld* World = GetWorld();
	if (World == nullptr || !OwnerEntity.IsValid())
	{
		return false;
	}
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem == nullptr)
	{
		return false;
	}
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	return EntityManager.IsEntityValid(OwnerEntity) && EntityManager.IsEntityBuilt(OwnerEntity);
}

bool UArcTargetingSubsystem::PreparePersistentRequest(FArcTargetingHandle Handle, FArcTargetingRequest& Request)
{
	const FArcTargetingPersistentKey Key = Request.PersistentKey;
	FArcTargetingPersistentEntry* Entry = PersistentEntries.Find(Key);
	if (Entry == nullptr || Entry->Handle != Handle)
	{
		Release(Handle);
		return false;
	}
	if (!IsEntityValid(Key.OwnerEntity))
	{
		RemovePersistentTargeting(Key.OwnerEntity, Key.Tag);
		return false;
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem == nullptr)
	{
		RemovePersistentTargeting(Key.OwnerEntity, Key.Tag);
		return false;
	}
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	const FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Key.OwnerEntity);
	if (TransformFragment == nullptr)
	{
		RemovePersistentTargeting(Key.OwnerEntity, Key.Tag);
		return false;
	}

	const FTransform& Transform = TransformFragment->GetTransform();
	FArcTargetingContext Context;
	Context.SourceEntity = Key.OwnerEntity;
	Context.SourceLocation = Transform.GetLocation();
	Context.SourceDirection = Transform.GetRotation().GetForwardVector();
	Context.SourceObject = World;
	if (FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Key.OwnerEntity))
	{
		AActor* Actor = ActorFragment->GetMutable();
		Context.SourceActor = Actor;
		Context.InstigatorActor = Actor;
	}
	Request.SeedContext = Context;
	Request.Context = MoveTemp(Context);
	return true;
}

void UArcTargetingSubsystem::OnPersistentCompleted(FArcTargetingHandle Handle, FArcTargetingPersistentKey Key)
{
	FArcTargetingPersistentEntry* Entry = PersistentEntries.Find(Key);
	if (Entry == nullptr || Entry->Handle != Handle)
	{
		return;
	}

	if (const FArcTargetingContext* Ctx = GetContext(Handle))
	{
		Entry->LatestCandidates = Ctx->Candidates;
		Entry->bHasCompleted = true;
	}
	Entry->LastCompletedSeconds = FPlatformTime::Seconds();
	Entry->bRunPending = false;
	BroadcastResultChanged(Key);
}

void UArcTargetingSubsystem::BroadcastResultChanged(const FArcTargetingPersistentKey& Key)
{
	const TSharedRef<FArcTargetingResultChanged>* Found = ResultChannels.Find(Key);
	if (Found == nullptr)
	{
		return;
	}
	const TSharedRef<FArcTargetingResultChanged> Channel = *Found;
	const TArray<FArcTargetCandidate>* Latest = GetLatestPersistentResult(Key.OwnerEntity, Key.Tag);
	if (Latest == nullptr)
	{
		Channel->Broadcast(nullptr);
		return;
	}
	const TArray<FArcTargetCandidate> Snapshot = *Latest;
	Channel->Broadcast(&Snapshot);
}
