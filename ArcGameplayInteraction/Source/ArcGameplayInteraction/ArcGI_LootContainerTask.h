// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayInteractionsTypes.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "Items/ArcItemsStoreComponent.h"

#include "ArcGI_LootContainerTask.generated.h"

USTRUCT()
struct FArcGI_LootContainerTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Context")
	FMassEntityHandle SmartObjectEntity;
};

USTRUCT(meta = (DisplayName = "Loot Container Transfer", Category = "Gameplay Interactions"))
struct ARCGAMEPLAYINTERACTION_API FArcGI_LootContainerTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGI_LootContainerTaskInstanceData;

	FArcGI_LootContainerTask();

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSubclassOf<UArcItemsStoreComponent> ItemsStoreClass;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
