// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Targeting/ArcMassTargetData.h"
#include "ArcAbilityTask_DrawDebugTargeting.generated.h"

USTRUCT()
struct FArcAbilityTask_DrawDebugTargetingInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FArcMassTargetDataHandle TargetDataHandle;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FColor Color = FColor::Green;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Radius = 30.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Duration = 3.f;
};

USTRUCT(DisplayName = "Draw Debug Targeting", Category = "Arc|Ability|Debug")
struct ARCCORE_API FArcAbilityTask_DrawDebugTargeting : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FArcAbilityTask_DrawDebugTargetingInstanceData::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
