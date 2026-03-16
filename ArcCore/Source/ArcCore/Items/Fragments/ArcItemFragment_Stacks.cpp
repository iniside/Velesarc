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



#include "ArcItemFragment_Stacks.h"

#include "Items/ArcItemsHelpers.h"

void FArcItemFragment_Stacks::OnItemAdded(const FArcItemData* InItem) const
{
	FArcItemInstance_Stacks* Instance = ArcItemsHelper::FindMutableInstance<FArcItemInstance_Stacks>(InItem);

	Instance->Stacks = InitialStacks;
}

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_Stacks::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment_ItemInstanceBase::GetDescription(InStruct);
	Desc.CommonPairings = {
		FName(TEXT("FArcItemFragment_RequiredItems"))
	};
	Desc.UsageNotes = TEXT(
		"Enables item stacking. InitialStacks sets the count when the item is first created. "
		"Requires the item definition to use FArcItemStackMethod_StackByType as its StackMethod. "
		"Creates mutable FArcItemInstance_Stacks for runtime stack tracking.");
	return Desc;
}
#endif // WITH_EDITOR
