// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "ArcKnowledgeTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "StateTreeInstanceData.h"
#include "StateTreeReference.h"
#include "StateTreeExecutionTypes.h"
#include "ArcAdvertisementExecutionContext.generated.h"

struct FMassExecutionContext;
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
	void SetKnowledgeLocation(const FVector& InLocation) { KnowledgeLocation = InLocation; }
	void SetSourceEntityLocation(const FVector& InLocation) { SourceEntityLocation = InLocation; }

	bool IsValid() const { return Owner != nullptr; }
	EStateTreeRunStatus GetLastRunStatus() const { return LastRunStatus; }

	// --- Debug Accessors ---
	const FStateTreeInstanceData& GetStateTreeInstanceData() const { return StateTreeInstanceData; }
	FArcKnowledgeHandle GetAdvertisementHandle() const { return AdvertisementHandle; }
	FMassEntityHandle GetSourceEntity() const { return SourceEntity; }
	FMassEntityHandle GetExecutingEntity() const { return ExecutingEntity; }
	const FInstancedStruct& GetAdvertisementPayload() const { return AdvertisementPayload; }

	// --- Lifecycle ---

	/**
	 * Prepares the StateTree execution context and starts the tree.
	 * @param Instruction The StateTree instruction to execute.
	 * @param MassContext Optional Mass execution context — when provided, creates FMassStateTreeExecutionContext
	 *                    enabling Mass-specific nodes and entity fragment binding in the inner tree.
	 * @return True if the tree started successfully and is ready to be ticked.
	 */
	bool Activate(const FArcAdvertisementInstruction_StateTree& Instruction, FMassExecutionContext* MassContext = nullptr);

	/**
	 * Updates the underlying StateTree.
	 * @param MassContext Optional Mass execution context for fragment resolution.
	 * @return True if still running, false if done.
	 */
	bool Tick(float DeltaTime, FMassExecutionContext* MassContext = nullptr);

	/** Stops the underlying StateTree. */
	void Deactivate(FMassExecutionContext* MassContext = nullptr);

	/** Sends event for the StateTree. Will be received on the next tick. */
	void SendEvent(const FGameplayTag Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName());

protected:
	/**
	 * Injects all external data views into the StateTree execution context.
	 * @return True if all required data views are valid.
	 */
	bool SetContextRequirements(FStateTreeExecutionContext& StateTreeContext, FMassExecutionContext* MassContext);

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

	UPROPERTY()
	FVector KnowledgeLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector SourceEntityLocation = FVector::ZeroVector;

	EStateTreeRunStatus LastRunStatus = EStateTreeRunStatus::Unset;

private:
	/** Cached reference to the active StateTree for Tick/Deactivate. */
	const FStateTreeReference* ActiveStateTreeReference = nullptr;

	/** Reentrancy guard. */
	FStateTreeExecutionContext* CurrentlyRunningExecContext = nullptr;
};
