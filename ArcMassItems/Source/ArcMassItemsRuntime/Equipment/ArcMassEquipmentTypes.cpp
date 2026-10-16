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

#include "Equipment/ArcMassEquipmentTypes.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Tags/ArcTagCountContainer.h"
#include "Mass/EntityView.h"

bool FArcMassEquipmentSlotAvailability_OwnerTags::IsAvailable(FMassEntityHandle OwnerEntity, FMassEntityManager& EntityManager) const
{
	const FArcOwnedTagsFragment* Frag = FMassEntityView(EntityManager, OwnerEntity).GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
	const FGameplayTagContainer& OwnedTags = Frag ? Frag->GetTags().GetTagContainer() : FGameplayTagContainer::EmptyContainer;
	return OwnerTags.IsEmpty() || OwnerTags.RequirementsMet(OwnedTags);
}

bool FArcMassEquipmentSlotAvailability_RequireSlotOccupied::IsAvailable(FMassEntityHandle OwnerEntity, FMassEntityManager& EntityManager) const
{
	const FArcMassEquipmentFragment* Frag = FMassEntityView(EntityManager, OwnerEntity).GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
	return RequiredSlotId.IsValid() && Frag && Frag->EquippedItems.Contains(RequiredSlotId);
}
