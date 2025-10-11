// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayInteractionsTypes.h"
#include "StateTreePropertyRef.h"
#include "UObject/Object.h"
#include "ArcGI_IncrementInteger.generated.h"

USTRUCT()
struct FArcGI_IncrementIntegerTaskInstanceData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Parameter)
	TStateTreePropertyRef<int32> Counter;
};
/**
 * 
 */
USTRUCT()
struct ARCGAMEPLAYINTERACTION_API FArcGI_IncrementIntegerTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGI_IncrementIntegerTaskInstanceData;

	FArcGI_IncrementIntegerTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
