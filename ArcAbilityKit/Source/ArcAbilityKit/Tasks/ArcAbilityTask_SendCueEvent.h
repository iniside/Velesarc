// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcCues/ArcCueTypes.h"
#include "ArcAbilityTask_SendCueEvent.generated.h"

USTRUCT()
struct FArcAbilityTask_SendCueEventInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FArcCueHandle Handle;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FArcCueEvent Event;
};

USTRUCT(DisplayName = "Send Cue Event", Category = "Arc|Ability|Cues")
struct ARCABILITYKIT_API FArcAbilityTask_SendCueEvent : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_SendCueEventInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
