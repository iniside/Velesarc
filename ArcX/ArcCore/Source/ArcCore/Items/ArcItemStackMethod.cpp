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



#include "ArcItemStackMethod.h"

#include "GameFramework/Actor.h"
#include "ArcItemsComponent.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemDefinition.h"

#include "Items/ArcItemsStoreComponent.h"

FArcItemId FArcItemStackMethod_StackEnum::StackCheck(UArcItemsStoreComponent* Owner
													 , const FArcItemSpec& InSpec) const
{
	const UArcItemDefinition* NewItem = InSpec.GetItemDefinition();
	if (StackingType == EArcItemStackType::StackByDefinition)
	{
		const FArcItemData* Exists = Owner->GetItemByDefinition(NewItem);

		if (Exists != nullptr)
		{
			return Exists->GetItemId();
		}
	}

	if (StackingType == EArcItemStackType::Unique)
	{
		const FArcItemData* Exists = Owner->GetItemByDefinition(NewItem);
		if (Exists != nullptr)
		{
			return Exists->GetItemId();
		}
	}

	return FArcItemId::InvalidId;
}

bool FArcItemStackMethod_StackEnum::CanStack() const
{
	if (StackingType == EArcItemStackType::StackByDefinition)
	{
		return true;
	}

	return false;
}

bool FArcItemStackMethod_StackEnum::IsUnique() const
{
	return StackingType == EArcItemStackType::Unique;
}
