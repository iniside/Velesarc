// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Effects/Tasks/ArcEffectTaskBase.h"
#include "Mass/EntityHandle.h"
#include "Effects/ArcEffectStateTreeContext.h"

#include "ArcEffectSTTask_WaitEffectApplied.generated.h"

class UArcEffectDefinition;

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectSTTask_WaitEffectAppliedInstanceData
{
	GENERATED_BODY()

	// Optional filter — if set, only this specific effect triggers completion
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UArcEffectDefinition> EffectFilter = nullptr;

	// Optional tag filter — if set, the applied effect must match these tags
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTagRequirements TagFilter;

	FDelegateHandle DelegateHandle;
};

USTRUCT(DisplayName = "Wait Effect Applied", Category = "Arc|Effect")
struct ARCMASSABILITIES_API FArcEffectSTTask_WaitEffectApplied : public FArcEffectTaskBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override { return FArcEffectSTTask_WaitEffectAppliedInstanceData::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FArcEffectStateTreeContext> EffectContextHandle;
	TStateTreeExternalDataHandle<FMassEntityHandle> OwnerEntityHandle;
};
