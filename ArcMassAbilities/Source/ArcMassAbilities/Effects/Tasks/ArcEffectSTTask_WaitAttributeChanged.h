// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/Tasks/ArcEffectTaskBase.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectStateTreeContext.h"
#include "Mass/EntityHandle.h"
#include "ArcEffectSTTask_WaitAttributeChanged.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectSTTask_WaitAttributeChangedInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FArcAttributeRef Attribute;

	FDelegateHandle DelegateHandle;
};

USTRUCT(DisplayName = "Wait Attribute Changed", Category = "Arc|Effect")
struct ARCMASSABILITIES_API FArcEffectSTTask_WaitAttributeChanged : public FArcEffectTaskBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FArcEffectSTTask_WaitAttributeChangedInstanceData::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	TStateTreeExternalDataHandle<FArcEffectStateTreeContext> EffectContextHandle;
	TStateTreeExternalDataHandle<FMassEntityHandle> OwnerEntityHandle;
};
