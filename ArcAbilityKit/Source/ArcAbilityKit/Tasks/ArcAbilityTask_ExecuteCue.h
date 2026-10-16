// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcCues/ArcCueTypes.h"
#include "ArcAbilityTask_ExecuteCue.generated.h"

class UArcCueDefinition;

USTRUCT()
struct FArcAbilityTask_ExecuteCueInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UArcCueDefinition> Definition = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle OwnerEntity;

	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle TargetEntity;
};

USTRUCT(DisplayName = "Execute Cue", Category = "Arc|Ability|Cues")
struct ARCABILITYKIT_API FArcAbilityTask_ExecuteCue : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_ExecuteCueInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
