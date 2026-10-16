// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcAbilityTask_GetCustomData.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityTask_GetCustomData_InstanceData
{
	GENERATED_BODY()

	// Parameter — author picks an FArcAbilityCustomDataBase subclass; the picked
	// ScriptStruct selects which CustomData entry to extract from the active ability.
	// At runtime the task overwrites this struct's bytes with the matched entry;
	// downstream tasks bind their FInstancedStruct inputs directly to this property.
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (BaseStruct = "/Script/ArcMassAbilities.ArcAbilityCustomDataBase"))
	FInstancedStruct CustomData;
};

USTRUCT(meta = (DisplayName = "Get Ability Custom Data"))
struct ARCMASSABILITIES_API FArcAbilityTask_GetCustomData : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_GetCustomData_InstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	                                       const FStateTreeTransitionResult& Transition) const override;
};
