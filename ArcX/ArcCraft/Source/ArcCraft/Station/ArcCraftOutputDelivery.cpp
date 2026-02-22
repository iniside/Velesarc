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
#include "ArcCraft/Station/ArcCraftStationComponent.h"
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
