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
