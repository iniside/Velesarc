#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcTargetingContext.h"
#include "ArcTargetingDebug.h"
#include "Persistent/ArcTargetingPersistentEntry.h"
#include "ArcTargetingSubsystem.generated.h"

class UArcTargetingPreset;

DECLARE_DELEGATE_OneParam(FArcTargetingCompletedDelegate, FArcTargetingHandle);
DECLARE_MULTICAST_DELEGATE_OneParam(FArcOnTargetingRequestRecordedForDebug, const FArcTargetingDebugTrace&);
DECLARE_MULTICAST_DELEGATE_OneParam(FArcTargetingResultChanged, const TArray<FArcTargetCandidate>*);

/**
 * One in-flight or completed targeting request. Owned by the subsystem; never
 * exposed by value. Callers reach the context (and therefore Candidates) via
 * GetContext / GetMutableContext on the handle.
 */
struct FArcTargetingRequest
{
	TWeakObjectPtr<const UArcTargetingPreset> Preset;
	// Original seed snapshot. Restored over Context at each requeue so prior-
	// pass cursor mutations (e.g. LineTrace bAdvanceCursorToHit) don't leak
	// into the next iteration.
	FArcTargetingContext SeedContext;
	FArcTargetingContext Context;
	EArcTargetingState State = EArcTargetingState::Pending;
	FArcTargetingCompletedDelegate OnCompleted;
	bool bRequeueOnCompletion = false;
	bool bCancelled = false;
	bool bPersistent = false;
	FArcTargetingPersistentKey PersistentKey;
	EArcTargetingDebugMode DebugMode = EArcTargetingDebugMode::Off;
#if UE_DEBUG_RENDERING_ENABLED
	FArcTargetingDebugSnapshot DebugSnapshot;
	FArcTargetingDebugTrace DebugTrace;
#endif
};

/**
 * Owns all live targeting requests for one world.
 *
 * Sync surface: MakeRequest + ExecuteSync.
 * Async surface: StartAsync enqueues a handle for time-budgeted draining in
 * Tick; Cancel removes it (best-effort — pipeline runs atomically per request).
 */
UCLASS()
class ARCTARGETING_API UArcTargetingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UArcTargetingSubsystem* Get(const UObject* WorldContext);

	// UWorldSubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// FTickableObjectBase
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }

	/**
	 * Allocate a new request seeded with `SeedContext`. The preset is held by
	 * weak ptr; if it goes away before the request runs, ExecuteSync/StartAsync
	 * will fail safely.
	 */
	FArcTargetingHandle MakeRequest(const UArcTargetingPreset* Preset, const FArcTargetingContext& SeedContext);

	/**
	 * Run the request's full pipeline now, in the calling frame. Bypasses the
	 * async queue. Safe to call on a completed handle (no-op).
	 */
	void ExecuteSync(FArcTargetingHandle Handle);

	/**
	 * Enqueue the request for async execution. Sets the completion delegate and
	 * requeue flag, then pushes the handle onto the FIFO drain queue. No-op if
	 * the handle is unknown or already Done/Cancelled.
	 */
	void StartAsync(FArcTargetingHandle Handle, FArcTargetingCompletedDelegate OnCompleted, bool bRequeueOnCompletion = false);

	/**
	 * Best-effort cancellation. Pending+queued requests are removed from the
	 * queue and marked Cancelled (delegate will not fire). Running requests are
	 * flagged so the in-flight pipeline suppresses its delegate. Does not free
	 * the handle — the caller still owns it and must call Release.
	 */
	void Cancel(FArcTargetingHandle Handle);

	/** Drop the request and its context. Idempotent. */
	void Release(FArcTargetingHandle Handle);

	/** Read-only access to a request's context (results live in Candidates). */
	const FArcTargetingContext* GetContext(FArcTargetingHandle Handle) const;

	/** Mutable access for callers that need to edit ExtraData before execute. */
	FArcTargetingContext* GetMutableContext(FArcTargetingHandle Handle);

	/** Lifecycle state for a handle. Returns Cancelled if the handle is unknown. */
	EArcTargetingState GetState(FArcTargetingHandle Handle) const;

	/** Per-request debug mode. Capped by the global `arc.Targeting.Debug` CVar. */
	void SetDebugMode(FArcTargetingHandle Handle, EArcTargetingDebugMode Mode);

	bool PushPersistentTargeting(
		FMassEntityHandle OwnerEntity,
		FGameplayTag Tag,
		const UArcTargetingPreset* Preset,
		float MinInterval = 0.f);

	void RemovePersistentTargeting(FMassEntityHandle OwnerEntity, FGameplayTag Tag);

	bool HasPersistentTargeting(FMassEntityHandle OwnerEntity, FGameplayTag Tag) const;

	void PublishResult(FMassEntityHandle OwnerEntity, FGameplayTag Tag, TArray<FArcTargetCandidate> Candidates);

	void ClearPublishedResult(FMassEntityHandle OwnerEntity, FGameplayTag Tag);

	const TArray<FArcTargetCandidate>* GetLatestPersistentResult(FMassEntityHandle OwnerEntity, FGameplayTag Tag) const;

	void GetPersistentKeys(TArray<FArcTargetingPersistentKey>& OutKeys) const;

	FDelegateHandle SubscribeResult(FMassEntityHandle OwnerEntity, FGameplayTag Tag, FArcTargetingResultChanged::FDelegate Delegate);

	void UnsubscribeResult(FMassEntityHandle OwnerEntity, FGameplayTag Tag, FDelegateHandle Handle);

#if UE_DEBUG_RENDERING_ENABLED
	/**
	 * Ref-counted enable for history capture. The debugger window increments on
	 * open (+1) and decrements on close (-1). Capture only runs while count > 0.
	 */
	void SetHistoryRecordingRefCount(int32 Delta);

	bool IsHistoryRecordingEnabled() const { return HistoryRecordingRefCount > 0; }

	/** View of all captured traces in chronological order (oldest first). */
	TConstArrayView<FArcTargetingDebugTrace> GetRecentTraces() const;

	/** Discard all captured traces. */
	void ClearRecentTraces();

	/**
	 * Static multicast delegate broadcast each time a non-persistent request
	 * completes and its trace is pushed into RecentTraces.
	 */
	static FArcOnTargetingRequestRecordedForDebug& OnRequestRecordedForDebug();
#endif // UE_DEBUG_RENDERING_ENABLED

private:
	void ExecuteRequestAtomic(FArcTargetingHandle Handle, FArcTargetingRequest& Request);

	EArcTargetingDebugMode ResolveDebugMode(const FArcTargetingRequest& Request) const;

	bool IsEntityValid(FMassEntityHandle OwnerEntity) const;
	bool PreparePersistentRequest(FArcTargetingHandle Handle, FArcTargetingRequest& Request);
	void OnPersistentCompleted(FArcTargetingHandle Handle, FArcTargetingPersistentKey Key);
	void BroadcastResultChanged(const FArcTargetingPersistentKey& Key);

	TMap<uint64, FArcTargetingRequest> Requests;
	TArray<uint64> AsyncQueue;
	uint64 NextHandleId = 1;

	TMap<FArcTargetingPersistentKey, FArcTargetingPersistentEntry> PersistentEntries;

	// A key lives in either PersistentEntries or PublishedResults, never both.
	TMap<FArcTargetingPersistentKey, TArray<FArcTargetCandidate>> PublishedResults;

	TMap<FArcTargetingPersistentKey, TSharedRef<FArcTargetingResultChanged>> ResultChannels;

#if UE_DEBUG_RENDERING_ENABLED
	int32 HistoryRecordingRefCount = 0;
	TArray<FArcTargetingDebugTrace> RecentTraces;
#endif
};
