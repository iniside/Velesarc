// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegates.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityComboStepTask.generated.h"

class UAbilitySystemComponent;
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
};

USTRUCT()
struct FArcGameplayAbilityComboStepTaskInstanceData
{
	GENERATED_BODY()

	/** The ability system component used for handling gameplay events. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** Animation montage containing UArcAnimNotify_MarkComboWindow notifies that define the combo window. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UAnimMontage> MontageToPlay;

	/** Input action that triggers the combo when pressed within the combo window. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UInputAction> InputAction;

	/** Optional input mapping context added at high priority for the combo step duration. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UInputMappingContext> InputMappingContext;

	/** Delegate broadcast when the combo input is successfully triggered within the combo window. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnComboTriggered;

	/** Gameplay tag sent as event when the combo window closes without input. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag ComboMissed;

	/** Gameplay tag sent as gameplay event when the combo input is pressed during the window. */
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

/**
 * Manages a single combo step by tracking a combo window defined by UArcAnimNotify_MarkComboWindow notifies in a montage.
 * Binds an input action and broadcasts OnComboTriggered when the player presses it within the combo window.
 * Also fires gameplay events from any UArcAnimNotify_MarkGameplayEvent notifies found in the montage.
 * Succeeds when the combo window ends. Cleans up input bindings and mapping context on exit.
 */
USTRUCT(meta = (DisplayName = "Combo Step", Tooltip = "Manages a single combo step by tracking a combo window defined by UArcAnimNotify_MarkComboWindow notifies in a montage. Binds an input action and broadcasts OnComboTriggered when the player presses it within the combo window. Also fires gameplay events from any UArcAnimNotify_MarkGameplayEvent notifies found in the montage. Succeeds when the combo window ends. Cleans up input bindings and mapping context on exit."))
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

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};
