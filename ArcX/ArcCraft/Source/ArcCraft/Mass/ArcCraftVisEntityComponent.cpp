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

#include "ArcCraft/Mass/ArcCraftVisEntityComponent.h"

#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "Items/ArcItemsArray.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/ArcItemData.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcCraftVis, Log, All);
DEFINE_LOG_CATEGORY(LogArcCraftVis);

UArcCraftVisEntityComponent::UArcCraftVisEntityComponent()
{
}

void UArcCraftVisEntityComponent::NotifyVisActorCreated(FMassEntityHandle InEntityHandle)
{
	Super::NotifyVisActorCreated(InEntityHandle);

	SyncEntityToStores();
}

void UArcCraftVisEntityComponent::NotifyVisActorPreDestroy()
{
	SyncStoresToEntity();

	Super::NotifyVisActorPreDestroy();
}

void UArcCraftVisEntityComponent::SyncEntityToStores()
{
	const FMassEntityHandle Entity = GetEntityHandle();
	if (!Entity.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Sync output items → output store
	if (OutputStoreClass)
	{
		UArcItemsStoreComponent* OutputStore = FindStore(OutputStoreClass);
		if (OutputStore)
		{
			const FArcCraftOutputFragment* OutputFrag = EntityManager.GetFragmentDataPtr<FArcCraftOutputFragment>(Entity);
			if (OutputFrag)
			{
				for (const FArcItemSpec& Spec : OutputFrag->OutputItems)
				{
					OutputStore->AddItem(Spec, FArcItemId());
				}
			}
		}
	}

	// Sync input items → input store
	if (InputStoreClass)
	{
		UArcItemsStoreComponent* InputStore = FindStore(InputStoreClass);
		if (InputStore)
		{
			const FArcCraftInputFragment* InputFrag = EntityManager.GetFragmentDataPtr<FArcCraftInputFragment>(Entity);
			if (InputFrag)
			{
				for (const FArcItemSpec& Spec : InputFrag->InputItems)
				{
					InputStore->AddItem(Spec, FArcItemId());
				}
			}
		}
	}

	UE_LOG(LogArcCraftVis, Verbose,
		TEXT("Synced entity data to stores on %s"), *GetOwner()->GetName());
}

void UArcCraftVisEntityComponent::SyncStoresToEntity()
{
	const FMassEntityHandle Entity = GetEntityHandle();
	if (!Entity.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Sync output store → entity output fragment
	if (OutputStoreClass)
	{
		UArcItemsStoreComponent* OutputStore = FindStore(OutputStoreClass);
		if (OutputStore)
		{
			FArcCraftOutputFragment* OutputFrag = EntityManager.GetFragmentDataPtr<FArcCraftOutputFragment>(Entity);
			if (OutputFrag)
			{
				OutputFrag->OutputItems.Reset();
				TArray<const FArcItemData*> Items = OutputStore->GetItems();
				for (const FArcItemData* Item : Items)
				{
					if (Item)
					{
						OutputFrag->OutputItems.Add(FArcItemCopyContainerHelper::ToSpec(Item));
					}
				}
			}
		}
	}

	// Sync input store → entity input fragment
	if (InputStoreClass)
	{
		UArcItemsStoreComponent* InputStore = FindStore(InputStoreClass);
		if (InputStore)
		{
			FArcCraftInputFragment* InputFrag = EntityManager.GetFragmentDataPtr<FArcCraftInputFragment>(Entity);
			if (InputFrag)
			{
				InputFrag->InputItems.Reset();
				TArray<const FArcItemData*> Items = InputStore->GetItems();
				for (const FArcItemData* Item : Items)
				{
					if (Item)
					{
						InputFrag->InputItems.Add(FArcItemCopyContainerHelper::ToSpec(Item));
					}
				}
			}
		}
	}

	UE_LOG(LogArcCraftVis, Verbose,
		TEXT("Synced store data to entity on %s"), *GetOwner()->GetName());
}

UArcItemsStoreComponent* UArcCraftVisEntityComponent::FindStore(
	TSubclassOf<UArcItemsStoreComponent> StoreClass) const
{
	if (!StoreClass || !GetOwner())
	{
		return nullptr;
	}

	TArray<UArcItemsStoreComponent*> Stores;
	GetOwner()->GetComponents<UArcItemsStoreComponent>(Stores);
	for (UArcItemsStoreComponent* Store : Stores)
	{
		if (Store && Store->IsA(StoreClass))
		{
			return Store;
		}
	}

	return nullptr;
}
