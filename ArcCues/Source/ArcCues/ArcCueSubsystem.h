// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "StateTreeEvents.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/WeakObjectPtr.h"

#include "ArcCueBehavior.h"

#include "ArcCueSubsystem.generated.h"

class UArcCueDefinition;
struct FArcCueBehaviorState;
struct FAssetData;

/**
 * One live persistent cue tracked by UArcCueSubsystem.
 *
 * Per-behavior state slots line up with UArcCueDefinition::Behaviors index-
 * for-index. A slot may remain empty until a behavior initializes it from an
 * event.
 */
UCLASS()
class UArcActiveCueInstance : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FArcCueHandle Handle;

	UPROPERTY()
	TObjectPtr<UArcCueDefinition> Definition = nullptr;

	UPROPERTY()
	FArcCueParameters Parameters;

	UPROPERTY()
	TArray<TInstancedStruct<FArcCueBehaviorState>> BehaviorStates;

	UPROPERTY()
	float ElapsedTime = 0.0f;
};

enum class EArcCuePendingOperationType : uint8
{
	CueEvent,
	StateTreeEvent,
	Remove
};

USTRUCT()
struct FArcCuePendingOperation
{
	GENERATED_BODY()

	EArcCuePendingOperationType Type = EArcCuePendingOperationType::CueEvent;

	UPROPERTY()
	FArcCueHandle Handle;

	UPROPERTY()
	FArcCueEvent CueEvent;

	UPROPERTY()
	FStateTreeEvent StateTreeEvent;
};

/**
 * World-scoped cue lifecycle owner.
 *
 * On Initialize() builds an AssetRegistry-backed tag-to-definitions cache
 * (mirrors FGameplayCueObjectLibrary minus the eager-load tax). Listens for
 * OnAssetAdded/OnAssetRemoved to keep current. Definitions are loaded
 * lazily on first fire of their tag.
 */
UCLASS()
class ARCCUES_API UArcCueSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UArcCueSubsystem* Get(const UObject* WorldContext);

	// USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableObjectBase
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }

	/** One-shot fire — runs OnExecute on every behavior of every matching definition. */
	void ExecuteCue(FGameplayTag CueTag, const FArcCueParameters& Params);
	void ExecuteCue(UArcCueDefinition* Definition, const FArcCueParameters& Params);

	/** Persistent fire — creates a retained instance and runs OnApplication. */
	FArcCueHandle AddCue(FGameplayTag CueTag, const FArcCueParameters& Params);
	FArcCueHandle AddCue(UArcCueDefinition* Definition, const FArcCueParameters& Params);

	void SendCueEvent(FArcCueHandle Handle, const FArcCueEvent& Event);
	void SendCueStateTreeEvent(FArcCueHandle Handle, const FStateTreeEvent& Event);

	/** Cascade OnRemoved on every stateful behavior, then drop the entry. */
	void RemoveCue(FArcCueHandle Handle);

	/** Remove all active cues with the given CueTag targeting Entity. */
	void RemoveCuesByTag(FGameplayTag CueTag, FMassEntityHandle TargetEntity);

	/** Remove every active cue currently targeting Entity. */
	void RemoveAllCues(FMassEntityHandle TargetEntity);

	void RemoveCuesByOwner(FMassEntityHandle OwnerEntity);
	void OnCueAttachmentLost(FArcCueHandle Handle, int32 BehaviorIndex, FMassEntityHandle AttachmentEntity);
	void QueueCueEntityDestroy(FMassEntityHandle Entity);

	/**
	 * Test-only: register a transient UArcCueDefinition into the discovery cache.
	 *
	 * Production discovery is AssetRegistry-driven and only sees on-disk assets.
	 * Tests construct definitions via NewObject(GetTransientPackage(), ...) which
	 * never appear in the registry. This hook lets tests inject those into the
	 * same TagToDefinitions map that AssetRegistry callbacks update, leaving the
	 * production discovery code path entirely unchanged.
	 */
	void RegisterDefinitionForTests(UArcCueDefinition* Definition);
	void UnregisterDefinitionForTests(UArcCueDefinition* Definition);

private:
	void RebuildDiscoveryCache();
	void OnAssetAdded(const FAssetData& AssetData);
	void OnAssetRemoved(const FAssetData& AssetData);

	/** Resolve definitions registered for CueTag, loading any soft refs that have not been loaded yet. */
	void GatherMatchingDefinitions(FGameplayTag CueTag, const FGameplayTagContainer& ContextTags, TArray<UArcCueDefinition*>& OutDefinitions);

	FArcCueHandle AllocateHandle();
	void BeginCallbackDispatch();
	void EndCallbackDispatch();
	void SendCueEventImmediate(FArcCueHandle Handle, const FArcCueEvent& Event);
	void SendCueStateTreeEventImmediate(FArcCueHandle Handle, const FStateTreeEvent& Event);
	void RemoveCueImmediate(FArcCueHandle Handle);
	void FlushPendingOperations();
	void FlushPendingEntityDestroys(FMassEntityManager* EntityManager);

private:
	/** Lookup table built from AssetRegistry on Initialize and kept in sync via OnAssetAdded/Removed. */
	TMap<FGameplayTag, TArray<TSoftObjectPtr<UArcCueDefinition>>> TagToDefinitions;

	/** Active persistent cues by handle. */
	UPROPERTY(Transient)
	TMap<FArcCueHandle, TObjectPtr<UArcActiveCueInstance>> ActiveCues;

	/** Per-target index to keep RemoveCuesByTag / RemoveAllCues O(active cues on that target). */
	TMultiMap<FMassEntityHandle, FArcCueHandle> CuesByTarget;
	TMultiMap<FMassEntityHandle, FArcCueHandle> CuesByOwner;

	/** Atomic-ish handle counter (subsystem is per-world / game-thread for cue calls; plain counter is fine). */
	uint32 NextHandleId = 1;
	int32 CallbackDispatchDepth = 0;
	bool bFlushingPendingOperations = false;

	UPROPERTY(Transient)
	TArray<FArcCuePendingOperation> PendingOperations;

	TArray<FMassEntityHandle> PendingEntityDestroys;

	FDelegateHandle OnAssetAddedHandle;
	FDelegateHandle OnAssetRemovedHandle;
};
