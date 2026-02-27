/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcCore/Commands/ArcDropItemCommand.h"

#include "ArcCore/Items/ArcItemData.h"
#include "ArcCore/Items/ArcItemDefinition.h"
#include "ArcCore/Items/ArcItemsArray.h"
#include "ArcCore/Items/ArcItemsStoreComponent.h"
#include "ArcCore/Items/ArcItemSpec.h"
#include "ArcCore/Items/Fragments/ArcItemFragment_Droppable.h"
#include "ArcCore/Items/Loot/ArcLootFragments.h"

#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassSpawnerSubsystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcDropItem, Log, All);

bool FArcDropItemCommand::CanSendCommand() const
{
	if (!ItemsStoreComponent)
	{
		return false;
	}

	const FArcItemData* ItemData = ItemsStoreComponent->GetItemPtr(ItemId);
	if (!ItemData)
	{
		return false;
	}

	if (ItemsStoreComponent->IsPending(ItemId))
	{
		return false;
	}

	const UArcItemDefinition* Def = ItemData->GetItemDefinition();
	if (!Def)
	{
		return false;
	}

	const FArcItemFragment_Droppable* DroppableFrag = Def->FindFragment<FArcItemFragment_Droppable>();
	if (!DroppableFrag || !DroppableFrag->DropEntityConfig)
	{
		return false;
	}

	return true;
}

void FArcDropItemCommand::PreSendCommand()
{
	if (ItemsStoreComponent)
	{
		TArray<FArcItemId> Items;
		GetPendingItems(Items);
		ItemsStoreComponent->AddPendingItems(Items);
		CaptureExpectedVersions(ItemsStoreComponent);
	}
}

void FArcDropItemCommand::GetPendingItems(TArray<FArcItemId>& OutItems) const
{
	OutItems.Add(ItemId);
}

bool FArcDropItemCommand::Execute()
{
	if (!ItemsStoreComponent)
	{
		UE_LOG(LogArcDropItem, Warning, TEXT("Execute: ItemsStoreComponent is null."));
		return false;
	}

	if (!ValidateVersions(ItemsStoreComponent))
	{
		UE_LOG(LogArcDropItem, Warning, TEXT("Execute: Version mismatch, item was modified."));
		return false;
	}

	const FArcItemData* ItemData = ItemsStoreComponent->GetItemPtr(ItemId);
	if (!ItemData)
	{
		UE_LOG(LogArcDropItem, Warning, TEXT("Execute: Item not found in store."));
		return false;
	}

	const UArcItemDefinition* Def = ItemData->GetItemDefinition();
	if (!Def)
	{
		UE_LOG(LogArcDropItem, Warning, TEXT("Execute: Item has no definition."));
		return false;
	}

	const FArcItemFragment_Droppable* DroppableFrag = Def->FindFragment<FArcItemFragment_Droppable>();
	if (!DroppableFrag || !DroppableFrag->DropEntityConfig)
	{
		UE_LOG(LogArcDropItem, Warning, TEXT("Execute: Item is not droppable or has no DropEntityConfig."));
		return false;
	}

	// Determine how many stacks to drop.
	const int32 CurrentStacks = ItemData->GetStacks();
	const int32 ActualStacksToDrop = (StacksToDrop <= 0 || StacksToDrop >= CurrentStacks)
		? CurrentStacks
		: StacksToDrop;

	// Build FArcItemSpec from current item state, preserving persistent instance data.
	FArcItemSpec DropSpec = FArcItemCopyContainerHelper::ToSpec(ItemData);
	DropSpec.ItemId = FArcItemId::Generate();
	DropSpec.Amount = ActualStacksToDrop;

	// Spawn Mass entity.
	UWorld* World = ItemsStoreComponent->GetWorld();
	if (!World)
	{
		UE_LOG(LogArcDropItem, Warning, TEXT("Execute: No world available."));
		return false;
	}

	UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
	if (!SpawnerSubsystem)
	{
		UE_LOG(LogArcDropItem, Warning, TEXT("Execute: MassSpawnerSubsystem not available."));
		return false;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	const FMassEntityTemplate& EntityTemplate = DroppableFrag->DropEntityConfig->GetOrCreateEntityTemplate(*World);
	if (!EntityTemplate.IsValid())
	{
		UE_LOG(LogArcDropItem, Warning, TEXT("Execute: Invalid entity template from DropEntityConfig."));
		return false;
	}

	TArray<FMassEntityHandle> BatchEntities;
	TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
		SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, BatchEntities);

	if (BatchEntities.IsEmpty())
	{
		UE_LOG(LogArcDropItem, Warning, TEXT("Execute: Failed to spawn drop entity."));
		return false;
	}

	const FMassEntityHandle Entity = BatchEntities[0];

	// Set transform.
	if (FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
	{
		TransformFragment->SetTransform(SpawnTransform);
	}

	// Populate loot container fragment with the dropped item spec.
	if (FArcLootContainerFragment* LootFragment = EntityManager.GetFragmentDataPtr<FArcLootContainerFragment>(Entity))
	{
		LootFragment->Items.Add(MoveTemp(DropSpec));
	}

	// Remove from store.
	const bool bRemoveEntirely = (ActualStacksToDrop >= CurrentStacks);
	ItemsStoreComponent->RemoveItem(ItemId, ActualStacksToDrop, bRemoveEntirely);

	UE_LOG(LogArcDropItem, Log, TEXT("Dropped %d stacks of item %s."), ActualStacksToDrop, *ItemId.ToString());

	return true;
}
