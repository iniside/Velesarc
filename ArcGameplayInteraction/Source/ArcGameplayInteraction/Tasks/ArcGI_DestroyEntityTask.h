// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayInteractionsTypes.h"
#include "Mass/EntityHandle.h"

#include "ArcGI_DestroyEntityTask.generated.h"

USTRUCT()
struct FArcGI_DestroyEntityTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	FMassEntityHandle SmartObjectEntity;
};

USTRUCT(meta = (DisplayName = "Destroy Entity", Category = "Gameplay Interactions"))
struct ARCGAMEPLAYINTERACTION_API FArcGI_DestroyEntityTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGI_DestroyEntityTaskInstanceData;

	FArcGI_DestroyEntityTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
