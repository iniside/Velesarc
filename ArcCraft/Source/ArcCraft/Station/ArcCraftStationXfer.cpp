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

#include "ArcCraft/Station/ArcCraftStationXfer.h"

#include "Mass/EntityManager.h"

#include "Bags/ArcBagOperations.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemSpec.h"
#include "Operations/ArcMassItemOperations.h"

namespace ArcCraftStationXfer
{
	int32 DeliverSpecToInstigator(FMassEntityManager& Mgr, FMassEntityHandle Instigator, const FArcItemSpec& Spec)
	{
		// Probe the bag overlay the same way FArcCraftOutputDelivery_EntityBag does: the bag store is a
		// sparse fragment, so presence is the validity of its sparse element on the instigator entity.
		const bool bHasBag = Mgr
			.GetMutableSparseElementDataForEntity(FArcMassBagStoreFragment::StaticStruct(), Instigator)
			.IsValid();

		if (bHasBag)
		{
			// Spilling add: fills partials then free slots, returns the count actually seated in bags.
			return ArcMassItems::Bags::AddItemSpilling(Mgr, Instigator, Spec);
		}

		// No bag overlay → degrade to a plain store add. AddItem is all-or-nothing: a valid id means the
		// whole spec landed; InvalidId means it was rejected and nothing was stored — return 0 so the
		// caller never decrements its source for items that were never delivered.
		const FArcItemId NewId = ArcMassItems::AddItem(
			Mgr, Instigator, FArcMassItemStoreFragment::StaticStruct(), Spec);
		if (!NewId.IsValid())
		{
			return 0;
		}

		return static_cast<int32>(Spec.Amount);
	}
}
