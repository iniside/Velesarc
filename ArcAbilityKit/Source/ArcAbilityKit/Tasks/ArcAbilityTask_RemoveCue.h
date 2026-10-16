// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcCues/ArcCueTypes.h"
#include "ArcAbilityTask_RemoveCue.generated.h"

USTRUCT()
struct FArcAbilityTask_RemoveCueInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FArcCueHandle Handle;
};

USTRUCT(DisplayName = "Remove Cue", Category = "Arc|Ability|Cues")
struct ARCABILITYKIT_API FArcAbilityTask_RemoveCue : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_RemoveCueInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
