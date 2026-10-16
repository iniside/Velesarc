// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayInteractionsTypes.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcGI_GrantLootFromTableToBagTask.generated.h"

class UMassEntityConfigAsset;

USTRUCT()
struct FArcGI_GrantLootFromTableToBagTaskInstanceData
{
	GENERATED_BODY()

	/** Entity carrying FArcLootTableConfigFragment (const shared, added by UArcLootTableTrait). */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle SmartObjectEntity;

	/** Entity that receives the rolled items via its bag overlay (FArcMassBagStoreFragment). The target
	 *  item store is derived from the bag overlay's TargetStoreType — not specified here. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle ContextEntity;

	/** Optional roll context. Base struct: FArcLootContext (or derived). Empty = no context. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcLootContext"))
	FInstancedStruct LootContext;

	/** Optional spill container. When set, items the bag can't accept spawn into a loot container
	 *  at the source/recipient transform; when unset, overflow items are dropped. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UMassEntityConfigAsset> SpillContainerConfig = nullptr;
};

USTRUCT(meta = (DisplayName = "Grant Loot From Table To Bag", Category = "Gameplay Interactions"))
struct ARCGAMEPLAYINTERACTION_API FArcGI_GrantLootFromTableToBagTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGI_GrantLootFromTableToBagTaskInstanceData;

	FArcGI_GrantLootFromTableToBagTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
