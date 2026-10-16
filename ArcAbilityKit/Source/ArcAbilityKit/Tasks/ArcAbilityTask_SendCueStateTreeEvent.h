// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcCues/ArcCueTypes.h"
#include "StateTreeEvents.h"

#include "ArcAbilityTask_SendCueStateTreeEvent.generated.h"

USTRUCT()
struct FArcAbilityTask_SendCueStateTreeEventInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FArcCueHandle Handle;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FStateTreeEvent Event;
};

USTRUCT(DisplayName = "Send Cue StateTree Event", Category = "Arc|Ability|Cues")
struct ARCABILITYKIT_API FArcAbilityTask_SendCueStateTreeEvent : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_SendCueStateTreeEventInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
