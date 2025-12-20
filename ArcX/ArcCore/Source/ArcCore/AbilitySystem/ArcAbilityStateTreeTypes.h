// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeInstanceData.h"
#include "StateTreeReference.h"
#include "StateTreeSchema.h"
#include "StateTreeTaskBase.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "ArcAbilityStateTreeTypes.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

USTRUCT(BlueprintType)
struct FArcItemFragment_StateTree : public FArcItemFragment
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FStateTreeReference StateTreeRef;
};

USTRUCT(meta = (Hidden))
struct FArcGameplayAbilityConditionBase : public FStateTreeConditionBase
{
	GENERATED_BODY()
};

USTRUCT(meta = (Hidden))
struct FArcGameplayAbilityTaskBase : public FStateTreeTaskBase
{
	GENERATED_BODY()
};

USTRUCT(Meta=(Hidden))
struct FArcGameplayAbilityEvaluatorBase : public FStateTreeEvaluatorBase
{
	GENERATED_BODY()
};


namespace Arcx::Abilities
{
	const FName AvatarActor = TEXT("AvatarActor");
	const FName OwnerActor = TEXT("OwnerActor");
	const FName Ability = TEXT("OwningAbility");
	const FName AbilitySystem = TEXT("AbilitySystemComponent");
}


UCLASS(MinimalAPI, BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Gameplay Ability"))
class UArcGameplayAbilityStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UArcGameplayAbilityStateTreeSchema();

	//UClass* GetContextActorClass() const { return ContextActorClass; };
	//UClass* GetSmartObjectActorClass() const { return SmartObjectActorClass; };

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InScriptStruct) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;

	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override { return ContextDataDescs; }

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	
	/** Actor class the StateTree is expected to run on. Allows to bind to specific Actor class' properties. */
	UPROPERTY(EditAnywhere, Category="Defaults")
	TSubclassOf<AActor> AvatarActorClass;

	/** Actor class of the SmartObject the StateTree is expected to run with. Allows to bind to specific Actor class' properties. */
	UPROPERTY(EditAnywhere, Category="Defaults")
	TSubclassOf<AActor> OwnerActorClass;

	UPROPERTY(EditAnywhere, Category="Defaults")
	TSubclassOf<UGameplayAbility> OwningAbilityClass;
	
	UPROPERTY(EditAnywhere, Category="Defaults")
	TSubclassOf<UAbilitySystemComponent> AbilitySystemComponentClass;
	
	/** List of named external data required by schema and provided to the state tree through the execution context. */
	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};

USTRUCT()
struct FArcGameplayAbilityStateTreeContext
{
	GENERATED_BODY()

public:
	void SetAvatarActor(AActor* InAvatarActor)
	{
		AvatarActor = InAvatarActor;
	}

	void SetOwnerActor(AActor* InOwnerActor)
	{
		OwnerActor = InOwnerActor;
	}

	void SetOwningAbility(UGameplayAbility* InOwningAbility)
	{
		OwningAbility = InOwningAbility;
	}
	
	void SetAbilitySystemComponent(UAbilitySystemComponent* InAbilitySystem)
	{
		AbilitySystemComponent = InAbilitySystem;
	}

	bool IsValid() const
	{
		return AvatarActor != nullptr && OwnerActor != nullptr && AbilitySystemComponent != nullptr;
	}

	EStateTreeRunStatus GetLastRunStatus() const
	{
		return LastRunStatus;
	};

	/**
	 * Prepares the StateTree execution context using provided Definition then starts the underlying StateTree 
	 * @return True if interaction has been properly initialized and ready to be ticked.
	 */
	bool Activate(const FStateTreeReference& StateTreeRef);

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
    TObjectPtr<AActor> AvatarActor = nullptr;
    
    UPROPERTY()
    TObjectPtr<AActor> OwnerActor = nullptr;

	UPROPERTY()
	TObjectPtr<UGameplayAbility> OwningAbility = nullptr;
	
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;
	
	UPROPERTY()
	TObjectPtr<const UStateTree> Definition = nullptr;

	EStateTreeRunStatus LastRunStatus = EStateTreeRunStatus::Unset;
	
private:
	FStateTreeExecutionContext* CurrentlyRunningExecContext = nullptr;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcWaitAbilityStateTreeDelegate, const FGameplayEventData&, Payload, FGameplayTag, EventTag);

USTRUCT(BlueprintType)
struct FArcGameplayAbilityInputEvent
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGameplayTag InputTag;
};

UCLASS()
class ARCCORE_API UArcAT_WaitAbilityStateTree : public UAbilityTask
{
	GENERATED_BODY()

public:
	UArcAT_WaitAbilityStateTree(const FObjectInitializer& Initializer);
	
	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability|Tasks"
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_WaitAbilityStateTree* WaitAbilityStateTree(UGameplayAbility* OwningAbility
															 , const FStateTreeReference& StateTreeRef
															 , FGameplayTag InputEventTag
															 , FGameplayTagContainer EventTags);

	UPROPERTY(BlueprintAssignable)
	FArcWaitAbilityStateTreeDelegate OnGameplayEvent;
	
	UPROPERTY(BlueprintAssignable)
	FArcWaitAbilityStateTreeDelegate OnStateTreeStopped;
	
	UPROPERTY(BlueprintAssignable)
	FArcWaitAbilityStateTreeDelegate OnStateTreeSucceeded;
	
	UPROPERTY(BlueprintAssignable)
	FArcWaitAbilityStateTreeDelegate OnStateTreeFailed;
	
protected:
	FStateTreeReference StateTreeRef;
	FGameplayTag InputEventTag;
	FGameplayTagContainer EventTags;
	
	FDelegateHandle EventHandle;
	FDelegateHandle InputDelegateHandle;
	
	FArcGameplayAbilityStateTreeContext Context;
	
	void HandleOnInputPressed(FGameplayAbilitySpec& InSpec);
	void HandleOnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);
	
	virtual void OnDestroy(bool AbilityEnded) override;
};

USTRUCT()
struct FArcNotifyEvent
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	float OriginalTime = 0;
	
	UPROPERTY()
	float Time = 0;
	
	UPROPERTY()
	FGameplayTag Tag;
	
	UPROPERTY()
	bool bPlayed;
	
	UPROPERTY()
	bool bResetPlayRate = false;
	
	UPROPERTY()
	bool bChangePlayRate = false;
	
	UPROPERTY()
	float PlayRate = 1.0f;
};

class UAnimMontage;



class UInputAction;
class UInputMappingContext;

UCLASS(const, hidecategories = Object, collapsecategories)
class ARCCORE_API UArcAnimNotify_MarkComboWindow : public UAnimNotify
{
	GENERATED_UCLASS_BODY()

public:
	// Begin UAnimNotify interface
	virtual FString GetNotifyName_Implementation() const override {return "MarkComboWindow"; };

	virtual void Notify(USkeletalMeshComponent* MeshComp
						, UAnimSequenceBase* Animation
						, const FAnimNotifyEventReference& EventReference) override {};

	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload) override {};
	
//#if WITH_EDITOR
//	virtual void ValidateAssociatedAssets() override;
//#endif
};


USTRUCT()
struct FArcGameplayAbilityComboStepTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UAnimMontage> MontageToPlay;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UInputAction> InputAction;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UInputMappingContext> InputMappingContext;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnComboTriggered;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag ComboMissed;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag InputPressedTag;
	
	UPROPERTY()
	int32 InputHandle;
	
	UPROPERTY()
	int32 ReleasedHandle;
	
	UPROPERTY()
	float StartComboTime;
	
	UPROPERTY()
	float EndComboTime;
	
	UPROPERTY()
	bool bIsComboWindowActive;
	
	UPROPERTY()
	float CurrentTime;
	
	UPROPERTY()
	TArray<FArcNotifyEvent> NotifyEvents;
	
	
};

USTRUCT()
struct FArcGameplayAbilityComboStepTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcGameplayAbilityComboStepTaskInstanceData;

	FArcGameplayAbilityComboStepTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	void HandleOnInputTriggered(FStateTreeWeakExecutionContext WeakContext);
	
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FArcGameplayAbilityWaitInputTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UInputAction> InputAction;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UInputMappingContext> InputMappingContext;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnInputTriggered;
	
	UPROPERTY()
	int32 InputHandle;
	
	UPROPERTY()
	int32 ReleasedHandle;
};

USTRUCT()
struct FArcGameplayAbilityWaitInputTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcGameplayAbilityWaitInputTaskInstanceData;

	FArcGameplayAbilityWaitInputTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FArcGameplayAbilityWaitAndSendGameplayEventTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(EditAnywhere, Category = Output)
	bool bWaitTimeOver;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	float WaitTime = 0;
	
	UPROPERTY()
	float CurrentTime = 0;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnWaitEnded;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag EventTag;
};

USTRUCT()
struct FArcGameplayAbilityWaitAndSendGameplayEventTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcGameplayAbilityWaitAndSendGameplayEventTaskInstanceData;

	FArcGameplayAbilityWaitAndSendGameplayEventTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};