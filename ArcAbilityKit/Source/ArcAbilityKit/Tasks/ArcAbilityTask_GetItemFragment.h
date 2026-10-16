// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcAbilityTask_GetItemFragment.generated.h"

USTRUCT()
struct ARCABILITYKIT_API FArcAbilityTask_GetItemFragment_InstanceData
{
	GENERATED_BODY()

	// Parameter — author picks an FArcItemFragment subclass; the picked ScriptStruct
	// selects which fragment to extract from the item referenced by the active ability's
	// FArcAbilitySource_Item custom data.  At runtime the task overwrites this struct's
	// bytes with the matched fragment; downstream tasks bind their FInstancedStruct
	// inputs directly to this property.
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcItemFragment"))
	FInstancedStruct ItemFragment;
};

USTRUCT(meta = (DisplayName = "Get Item Fragment"))
struct ARCABILITYKIT_API FArcAbilityTask_GetItemFragment : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_GetItemFragment_InstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	                                       const FStateTreeTransitionResult& Transition) const override;
};
