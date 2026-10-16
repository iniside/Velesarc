// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayInteractionsTypes.h"
#include "Mass/EntityHandle.h"

#include "ArcGI_TransferLootContainerToBagTask.generated.h"

USTRUCT()
struct FArcGI_TransferLootContainerToBagTaskInstanceData
{
	GENERATED_BODY()

	/** Entity owning FArcLootContainerFragment (source). */
	UPROPERTY(EditAnywhere, Category = "Context")
	FMassEntityHandle SmartObjectEntity;

	/** Entity owning the destination bag overlay (FArcMassBagStoreFragment). The target item store is
	 *  derived from the bag overlay's TargetStoreType — not specified here. */
	UPROPERTY(EditAnywhere, Category = "Context")
	FMassEntityHandle ContextEntity;
};

USTRUCT(meta = (DisplayName = "Transfer Loot Container Items To Bag", Category = "Gameplay Interactions"))
struct ARCGAMEPLAYINTERACTION_API FArcGI_TransferLootContainerToBagTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGI_TransferLootContainerToBagTaskInstanceData;

	FArcGI_TransferLootContainerToBagTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
