// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcCues/ArcCueTypes.h"
#include "ArcAbilityTask_AddCue.generated.h"

class UArcCueDefinition;

USTRUCT()
struct FArcAbilityTask_AddCueInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UArcCueDefinition> Definition = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle OwnerEntity;

	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle TargetEntity;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FArcCueHandle Handle;
};

USTRUCT(DisplayName = "Add Cue", Category = "Arc|Ability|Cues")
struct ARCABILITYKIT_API FArcAbilityTask_AddCue : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	FArcAbilityTask_AddCue()
	{
		bShouldStateChangeOnReselect = false;
		bShouldCallTick = false;
#if WITH_EDITORONLY_DATA
		bConsideredForCompletion = false;
		bCanEditConsideredForCompletion = false;
#endif
	}

	using FInstanceDataType = FArcAbilityTask_AddCueInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
