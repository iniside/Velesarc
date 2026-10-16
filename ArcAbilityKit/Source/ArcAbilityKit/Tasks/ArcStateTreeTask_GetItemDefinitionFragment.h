// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcStateTreeTask_GetItemDefinitionFragment.generated.h"

USTRUCT()
struct ARCABILITYKIT_API FArcStateTreeTask_GetItemDefinitionFragment_InstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcItemFragment"))
	FInstancedStruct ItemFragment;
};

USTRUCT(meta = (DisplayName = "Get Item Definition Fragment"))
struct ARCABILITYKIT_API FArcStateTreeTask_GetItemDefinitionFragment : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FArcStateTreeTask_GetItemDefinitionFragment()
	{
		bShouldStateChangeOnReselect = false;
		bShouldCallTick = false;
#if WITH_EDITORONLY_DATA
		bConsideredForCompletion = false;
		bCanEditConsideredForCompletion = false;
#endif
	}

	using FInstanceDataType = FArcStateTreeTask_GetItemDefinitionFragment_InstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
