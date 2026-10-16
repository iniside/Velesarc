// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayInteractionsTypes.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcGI_GrantLootFromTableTask.generated.h"

USTRUCT()
struct FArcGI_GrantLootFromTableTaskInstanceData
{
	GENERATED_BODY()

	/** Entity carrying FArcLootTableConfigFragment (const shared, added by UArcLootTableTrait). */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle SmartObjectEntity;

	/** Entity that receives the rolled items (must own FArcMassItemStoreFragment or the chosen sub-store). */
	UPROPERTY(EditAnywhere, Category = "Input")
	FMassEntityHandle ContextEntity;

	/** Store sub-type on the recipient. Must derive from FArcMassItemStoreFragment.
	 *  Defaults to FArcMassItemStoreFragment::StaticStruct() at runtime when null. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TObjectPtr<UScriptStruct> StoreType = nullptr;

	/** Optional roll context. Base struct: FArcLootContext (or derived). Empty = no context. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (BaseStruct = "/Script/ArcMassItemsRuntime.ArcLootContext"))
	FInstancedStruct LootContext;
};

USTRUCT(meta = (DisplayName = "Grant Loot From Table", Category = "Gameplay Interactions"))
struct ARCGAMEPLAYINTERACTION_API FArcGI_GrantLootFromTableTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGI_GrantLootFromTableTaskInstanceData;

	FArcGI_GrantLootFromTableTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
