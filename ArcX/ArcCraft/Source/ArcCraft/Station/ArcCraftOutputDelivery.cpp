/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "ArcCraft/Station/ArcCraftOutputDelivery.h"

#include "ArcCoreUtils.h"
#include "MassEntitySubsystem.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftVisEntityComponent.h"
#include "ArcCraft/Station/ArcCraftStationComponent.h"
#include "Engine/World.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemsStoreComponent.h"

// -------------------------------------------------------------------
// FArcCraftOutputDelivery (base)
// -------------------------------------------------------------------

bool FArcCraftOutputDelivery::DeliverOutput(
	UArcCraftStationComponent* Station,
	const FArcItemSpec& OutputSpec,
	const UObject* Instigator) const
{
	return false;
}

// -------------------------------------------------------------------
// FArcCraftOutputDelivery_StoreOnStation
// -------------------------------------------------------------------

bool FArcCraftOutputDelivery_StoreOnStation::DeliverOutput(
	UArcCraftStationComponent* Station,
	const FArcItemSpec& OutputSpec,
	const UObject* Instigator) const
{
	if (!Station)
	{
		return false;
	}

	AActor* Owner = Station->GetOwner();
	if (!Owner)
	{
		return false;
	}

	UArcItemsStoreComponent* Store = Arcx::Utils::GetComponent(Owner, OutputStoreClass);
	if (!Store)
	{
		return false;
	}

	Store->AddItem(OutputSpec, FArcItemId::InvalidId);
	return true;
}

// -------------------------------------------------------------------
// FArcCraftOutputDelivery_ToInstigator
// -------------------------------------------------------------------

bool FArcCraftOutputDelivery_ToInstigator::DeliverOutput(
	UArcCraftStationComponent* Station,
	const FArcItemSpec& OutputSpec,
	const UObject* Instigator) const
{
	const AActor* InstigatorActor = Cast<AActor>(Instigator);
	if (!InstigatorActor)
	{
		return false;
	}

	UArcItemsStoreComponent* Store = Arcx::Utils::GetComponent(InstigatorActor, ItemsStoreClass);
	if (!Store)
	{
		return false;
	}

	Store->AddItem(OutputSpec, FArcItemId::InvalidId);
	return true;
}

// -------------------------------------------------------------------
// FArcCraftOutputDelivery_EntityStore
// -------------------------------------------------------------------

UArcCraftVisEntityComponent* FArcCraftOutputDelivery_EntityStore::GetVisComponent(
	const UArcCraftStationComponent* Station) const
{
	if (!Station || !Station->GetOwner())
	{
		return nullptr;
	}
	return Station->GetOwner()->FindComponentByClass<UArcCraftVisEntityComponent>();
}

FArcCraftOutputFragment* FArcCraftOutputDelivery_EntityStore::GetOutputFragment(
	const UArcCraftStationComponent* Station) const
{
	UArcCraftVisEntityComponent* VisComp = GetVisComponent(Station);
	if (!VisComp)
	{
		return nullptr;
	}

	const FMassEntityHandle Entity = VisComp->GetEntityHandle();
	if (!Entity.IsValid())
	{
		return nullptr;
	}

	UWorld* World = Station->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		return nullptr;
	}

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(Entity))
	{
		return nullptr;
	}

	return EntityManager.GetFragmentDataPtr<FArcCraftOutputFragment>(Entity);
}

UArcItemsStoreComponent* FArcCraftOutputDelivery_EntityStore::GetMirrorOutputStore(
	const UArcCraftStationComponent* Station) const
{
	UArcCraftVisEntityComponent* VisComp = GetVisComponent(Station);
	if (!VisComp || !Station->GetOwner())
	{
		return nullptr;
	}

	TArray<UArcItemsStoreComponent*> Stores;
	Station->GetOwner()->GetComponents<UArcItemsStoreComponent>(Stores);
	for (UArcItemsStoreComponent* Store : Stores)
	{
		if (Store && Store->IsA(VisComp->OutputStoreClass))
		{
			return Store;
		}
	}
	return nullptr;
}

bool FArcCraftOutputDelivery_EntityStore::DeliverOutput(
	UArcCraftStationComponent* Station,
	const FArcItemSpec& OutputSpec,
	const UObject* Instigator) const
{
	// Write to entity fragment (source of truth)
	FArcCraftOutputFragment* OutputFrag = GetOutputFragment(Station);
	if (!OutputFrag)
	{
		return false;
	}

	OutputFrag->OutputItems.Add(OutputSpec);

	// Mirror to actor-side store if alive (for UI)
	UArcItemsStoreComponent* MirrorStore = GetMirrorOutputStore(Station);
	if (MirrorStore)
	{
		MirrorStore->AddItem(OutputSpec, FArcItemId::InvalidId);
	}

	return true;
}
