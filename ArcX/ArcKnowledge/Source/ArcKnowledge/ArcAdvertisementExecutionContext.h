// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "ArcKnowledgeTypes.h"
#include "InstancedStruct.h"
#include "StateTreeInstanceData.h"
#include "StateTreeReference.h"
#include "StateTreeExecutionTypes.h"
#include "ArcAdvertisementExecutionContext.generated.h"

struct FGameplayTag;
struct FStateTreeExecutionContext;
struct FArcAdvertisementInstruction_StateTree;

DECLARE_LOG_CATEGORY_EXTERN(LogArcAdvertisement, Log, All);

/**
 * Execution context for running an advertisement's StateTree behavior.
 * Lighter version of FArcGameplayInteractionContext without SmartObject dependencies.
 *
 * Wraps StateTree execution with lifecycle: Activate -> Tick -> Deactivate.
 *
 * Owner (UObject*) is required -- used as the FStateTreeExecutionContext owner and to obtain the UWorld.
 * ContextActor (AActor*) is optional -- provided as schema context data when entity has an associated actor.
 */
USTRUCT()
struct ARCKNOWLEDGE_API FArcAdvertisementExecutionContext
{
	GENERATED_BODY()

public:
	// --- Setup ---
	void SetOwner(UObject* InOwner) { Owner = InOwner; }
	void SetExecutingEntity(const FMassEntityHandle& InHandle) { ExecutingEntity = InHandle; }
	void SetSourceEntity(const FMassEntityHandle& InHandle) { SourceEntity = InHandle; }
	void SetContextActor(AActor* InActor) { ContextActor = InActor; }
	void SetAdvertisementHandle(const FArcKnowledgeHandle& InHandle) { AdvertisementHandle = InHandle; }
	void SetAdvertisementPayload(const FInstancedStruct& InPayload) { AdvertisementPayload = InPayload; }

	bool IsValid() const { return Owner != nullptr; }
	EStateTreeRunStatus GetLastRunStatus() const { return LastRunStatus; }

	// --- Lifecycle ---

	/**
	 * Prepares the StateTree execution context and starts the tree.
	 * @return True if the tree started successfully and is ready to be ticked.
	 */
	bool Activate(const FArcAdvertisementInstruction_StateTree& Instruction);

	/**
	 * Updates the underlying StateTree.
	 * @return True if still running, false if done.
	 */
	bool Tick(float DeltaTime);

	/** Stops the underlying StateTree. */
	void Deactivate();

	/** Sends event for the StateTree. Will be received on the next tick. */
	void SendEvent(const FGameplayTag Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName());

protected:
	/**
	 * Injects all external data views into the StateTree execution context.
	 * @return True if all required data views are valid.
	 */
	bool SetContextRequirements(FStateTreeExecutionContext& StateTreeContext);

	UPROPERTY()
	FStateTreeInstanceData StateTreeInstanceData;

	UPROPERTY()
	FMassEntityHandle ExecutingEntity;

	UPROPERTY()
	FMassEntityHandle SourceEntity;

	/** UObject owner for FStateTreeExecutionContext. Provides UWorld access. */
	UPROPERTY()
	TObjectPtr<UObject> Owner = nullptr;

	/** Optional actor associated with the executing entity. Provided as context data when available. */
	UPROPERTY()
	TObjectPtr<AActor> ContextActor = nullptr;

	UPROPERTY()
	FArcKnowledgeHandle AdvertisementHandle;

	UPROPERTY()
	FInstancedStruct AdvertisementPayload;

	EStateTreeRunStatus LastRunStatus = EStateTreeRunStatus::Unset;

private:
	/** Cached reference to the active StateTree for Tick/Deactivate. */
	const FStateTreeReference* ActiveStateTreeReference = nullptr;

	/** Reentrancy guard. */
	FStateTreeExecutionContext* CurrentlyRunningExecContext = nullptr;
};
