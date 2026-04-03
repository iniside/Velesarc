// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayInteractionsTypes.h"
#include "Mass/EntityHandle.h"

#include "ArcGI_TransferLootContainerTask.generated.h"

USTRUCT()
struct FArcGI_TransferLootContainerTaskInstanceData
{
	GENERATED_BODY()

	/** Entity owning FArcLootContainerFragment (source). */
	UPROPERTY(EditAnywhere, Category = "Context")
	FMassEntityHandle SmartObjectEntity;

	/** Entity owning FArcMassItemSpecArrayFragment (destination). */
	UPROPERTY(EditAnywhere, Category = "Context")
	FMassEntityHandle ContextEntity;
};

USTRUCT(meta = (DisplayName = "Transfer Loot Container Items", Category = "Gameplay Interactions"))
struct ARCGAMEPLAYINTERACTION_API FArcGI_TransferLootContainerTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGI_TransferLootContainerTaskInstanceData;

	FArcGI_TransferLootContainerTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
