// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayInteractionStateTreeSchema.h"
#include "GameplayInteractionsTypes.h"
#include "Mass/EntityHandle.h"
#include "MassStateTreeDependency.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeInstanceData.h"
#include "StateTreeReference.h"
#include "ArcGameplayInteractionContext.generated.h"

struct FGameplayTag;
struct FStateTreeEvent;
struct FStateTreeExecutionContext;
struct FMassExecutionContext;

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
	
	void SetOwner(UObject* InOwner)
	{
		Owner = InOwner;
	}

	void SetContextActor(AActor* InContextActor)
	{
		ContextActor = InContextActor;
	}

	void SetSmartObjectActor(AActor* InSmartObjectActor)
	{
		SmartObjectActor = InSmartObjectActor;
	}

	void SetSlotTransform(const FTransform& InSlotTransform)
	{
		SlotTransform = InSlotTransform;
	}

	void SetAbortContext(const FGameplayInteractionAbortContext& InAbortContext)
	{
		AbortContext = InAbortContext;
	}

	/** Returns the effective owner — explicit Owner if set, otherwise ContextActor. */
	UObject* GetOwner() const
	{
		return Owner ? Owner.Get() : ContextActor.Get();
	}

	bool IsValid() const
	{
		return ClaimedHandle.IsValid() && GetOwner() != nullptr;
	}

	EStateTreeRunStatus GetLastRunStatus() const
	{
		return LastRunStatus;
	};

	const FStateTreeInstanceData& GetStateTreeInstanceData() const { return StateTreeInstanceData; }
	const UArcGameplayInteractionSmartObjectBehaviorDefinition* GetDefinition() const { return Definition; }

	/**
	 * Prepares the StateTree execution context using provided Definition then starts the underlying StateTree.
	 * @param MassContext Optional Mass execution context — when provided, creates FMassStateTreeExecutionContext
	 *                    enabling Mass-specific nodes and entity fragment binding in the inner tree.
	 * @return True if interaction has been properly initialized and ready to be ticked.
	 */
	bool Activate(const UArcGameplayInteractionSmartObjectBehaviorDefinition& Definition, FMassExecutionContext* MassContext = nullptr);

	/**
	 * Updates the underlying StateTree.
	 * @param MassContext Optional Mass execution context for fragment resolution.
	 * @return True if still requires to be ticked, false if done.
	 */
	bool Tick(float DeltaTime, FMassExecutionContext* MassContext = nullptr);

	/**
	 * Stops the underlying StateTree.
	 * @param MassContext Optional Mass execution context for fragment resolution.
	 */
	void Deactivate(FMassExecutionContext* MassContext = nullptr);

	/** Sends event for the StateTree. Will be received on the next tick by the StateTree. */
	void SendEvent(const FGameplayTag Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName());

protected:	
	/**
	 * Updates all external data views from the provided interaction context.
	 * @return True if all external data views are valid, false otherwise.
	 */
	bool SetContextRequirements(FStateTreeExecutionContext& StateTreeContext, FMassExecutionContext* MassContext = nullptr);

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
	FTransform SlotTransform;

	/** UObject owner for FStateTreeExecutionContext. When set, used instead of ContextActor. */
	UPROPERTY()
	TObjectPtr<UObject> Owner = nullptr;

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

	const TArray<FMassStateTreeDependency>& GetDependencies() const { return Dependencies; }

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
	virtual bool Link(FStateTreeLinker& Linker) override;

	UPROPERTY(Transient)
	TArray<FMassStateTreeDependency> Dependencies;
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

