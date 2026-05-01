// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "StateTreeInstanceData.h"
#include "StateTreeTypes.h"

#include "ArcEffectStateTreeContext.generated.h"

class UArcEffectDefinition;
class UStateTree;
struct FStateTreeExecutionContext;
struct FConstStructView;

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectStateTreeContext
{
	GENERATED_BODY()

public:
	/**
	 * Initializes instance data, constructs the execution context and starts the StateTree.
	 * @return True if the tree was successfully started.
	 */
	bool Activate(const UStateTree* StateTree, UObject* InOwner);

	/**
	 * Ticks the StateTree by constructing an execution context on the stack.
	 * @return True if the tree is still running.
	 */
	bool Tick(float DeltaTime);

	/** Stops the StateTree and resets cached state. */
	void Deactivate();

	/**
	 * Sends an event into the StateTree then immediately ticks with DeltaTime=0.
	 * Called by tree tasks from delegate callbacks.
	 */
	void RequestTick(const FGameplayTag& Tag, FConstStructView Payload);

	EStateTreeRunStatus GetLastRunStatus() const { return LastRunStatus; }
	bool IsRunning() const { return LastRunStatus == EStateTreeRunStatus::Running; }
	const FStateTreeInstanceData& GetInstanceData() const { return InstanceData; }
	const UStateTree* GetCachedStateTree() const { return CachedStateTree; }

	void SetOwnerEntity(const FMassEntityHandle& InHandle) { OwnerEntity = InHandle; }
	void SetSourceEntity(const FMassEntityHandle& InHandle) { SourceEntity = InHandle; }
	void SetEffectDefinition(UArcEffectDefinition* InDefinition) { EffectDefinition = InDefinition; }
	void SetWorld(UWorld* InWorld) { World = InWorld; }

private:
	/** Populates context data views and sets the external data collection callback. */
	bool SetContextRequirements(FStateTreeExecutionContext& Context);

	UPROPERTY()
	FStateTreeInstanceData InstanceData;

	UPROPERTY()
	TObjectPtr<const UStateTree> CachedStateTree = nullptr;

	UPROPERTY()
	TObjectPtr<UObject> Owner = nullptr;

	UPROPERTY()
	FMassEntityHandle OwnerEntity;

	UPROPERTY()
	FMassEntityHandle SourceEntity;

	UPROPERTY()
	TObjectPtr<UArcEffectDefinition> EffectDefinition = nullptr;

	UPROPERTY()
	TObjectPtr<UWorld> World = nullptr;

	EStateTreeRunStatus LastRunStatus = EStateTreeRunStatus::Unset;
};
