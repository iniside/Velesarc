// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayInteractionStateTreeSchema.h"
#include "GameplayInteractionsTypes.h"
#include "MassEntityHandle.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeInstanceData.h"
#include "StateTreeReference.h"
#include "ArcGameplayInteractionContext.generated.h"

/**
 * 
 */

struct FGameplayTag;
struct FStateTreeEvent;
struct FStateTreeExecutionContext;

class UArcGameplayInteractionSmartObjectBehaviorDefinition;

/**
 * Struct that holds data required to perform the interaction
 * and wraps StateTree execution
 */
USTRUCT()
struct ARCGAMEPLAYINTERACTION_API FArcGameplayInteractionContext
{
	GENERATED_BODY()

public:
	const FSmartObjectClaimHandle& GetClaimedHandle() const
	{
		return ClaimedHandle;
	}

	void SetContextEntity(const FMassEntityHandle& InHandle)
	{
		ContextEntity = InHandle;
	}
	
	void SetSmartObjectEntity(const FMassEntityHandle& InHandle)
	{
		SmartObjectEntity = InHandle;
	}
	
	void SetClaimedHandle(const FSmartObjectClaimHandle& InClaimedHandle)
	{
		ClaimedHandle = InClaimedHandle;
	}

	void SetSlotEntranceHandle(const FSmartObjectSlotEntranceHandle InSlotEntranceHandle)
	{
		SlotEntranceHandle = InSlotEntranceHandle;
	}
	
	void SetContextActor(AActor* InContextActor)
	{
		ContextActor = InContextActor;
	}

	void SetSmartObjectActor(AActor* InSmartObjectActor)
	{
		SmartObjectActor = InSmartObjectActor;
	}

	void SetAbortContext(const FGameplayInteractionAbortContext& InAbortContext)
	{
		AbortContext = InAbortContext;
	}

	bool IsValid() const
	{
		return ClaimedHandle.IsValid() && ContextActor != nullptr && SmartObjectActor != nullptr;
	}

	EStateTreeRunStatus GetLastRunStatus() const
	{
		return LastRunStatus;
	};

	/**
	 * Prepares the StateTree execution context using provided Definition then starts the underlying StateTree 
	 * @return True if interaction has been properly initialized and ready to be ticked.
	 */
	bool Activate(const UArcGameplayInteractionSmartObjectBehaviorDefinition& Definition);

	/**
	 * Updates the underlying StateTree
	 * @return True if still requires to be ticked, false if done.
	 */
	bool Tick(const float DeltaTime);
	
	/**
	 * Stops the underlying StateTree
	 */
	void Deactivate();

	/** Sends event for the StateTree. Will be received on the next tick by the StateTree. */
	void SendEvent(const FGameplayTag Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName());

protected:	
	/**
	 * Updates all external data views from the provided interaction context.  
	 * @return True if all external data views are valid, false otherwise.
	 */
	bool SetContextRequirements(FStateTreeExecutionContext& StateTreeContext);

	/** @return true of the ContextActor and SmartObjectActor match the ones set in schema. */
	bool ValidateSchema(const FStateTreeExecutionContext& StateTreeContext) const;
	
	UPROPERTY()
	FStateTreeInstanceData StateTreeInstanceData;
    
    UPROPERTY()
    FSmartObjectClaimHandle ClaimedHandle;

	UPROPERTY()
	FSmartObjectSlotEntranceHandle SlotEntranceHandle;

	UPROPERTY()
	FGameplayInteractionAbortContext AbortContext;

	UPROPERTY()
	FMassEntityHandle ContextEntity;
	
	UPROPERTY()
	FMassEntityHandle SmartObjectEntity;
	
	UPROPERTY()
    TObjectPtr<AActor> ContextActor = nullptr;
    
    UPROPERTY()
    TObjectPtr<AActor> SmartObjectActor = nullptr;

	UPROPERTY()
	TObjectPtr<const UArcGameplayInteractionSmartObjectBehaviorDefinition> Definition = nullptr;

	EStateTreeRunStatus LastRunStatus = EStateTreeRunStatus::Unset;
	
private:
	FStateTreeExecutionContext* CurrentlyRunningExecContext = nullptr;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Gameplay Interactions"))
class ARCGAMEPLAYINTERACTION_API UArcGameplayInteractionStateTreeSchema : public UGameplayInteractionStateTreeSchema
{
	GENERATED_BODY()

public:
	UArcGameplayInteractionStateTreeSchema();
};

UCLASS(BlueprintType)
class UArcGameplayInteractionSmartObjectBehaviorDefinition : public USmartObjectBehaviorDefinition
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SmartObject", meta=(Schema="/Script/ArcGameplayInteraction.ArcGameplayInteractionStateTreeSchema"))
	FStateTreeReference StateTreeReference;

	UFUNCTION(BlueprintCallable, Category = "StateTree")
	void SetStateTree(UStateTree* NewStateTree)
	{
		StateTreeReference.SetStateTree(NewStateTree);
	}

	UFUNCTION(BlueprintPure, Category = "StateTree")
	const UStateTree* GetStateTree() const
	{
		return StateTreeReference.GetStateTree();
	}

public:
#if WITH_EDITOR
	//~ UObject
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
#endif //WITH_EDITOR
};

UCLASS(BlueprintType)
class UArcAIGameplayInteractionSmartObjectBehaviorDefinition : public UArcGameplayInteractionSmartObjectBehaviorDefinition
{
	GENERATED_BODY()
};

UCLASS(BlueprintType)
class UArcInteractGameplayInteractionSmartObjectBehaviorDefinition : public UArcGameplayInteractionSmartObjectBehaviorDefinition
{
	GENERATED_BODY()
};

