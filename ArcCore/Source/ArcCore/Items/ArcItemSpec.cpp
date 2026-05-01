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

#include "Items/ArcItemSpec.h"

#include "ArcItemBundleNames.h"
#include "Items/ArcItemDefinition.h"
#include "UObject/SoftObjectPath.h"

#include "Core/ArcCoreAssetManager.h"


DEFINE_LOG_CATEGORY(LogArcItemSpec);

const UArcItemDefinition* FArcItemSpec::GetItemDefinition() const
{
	if (ItemDefinition)
	{
		return ItemDefinition;
	}
	const UArcItemDefinition* Item = UArcCoreAssetManager::Get().GetAsset<UArcItemDefinition>(ItemDefinitionId, false);
	ItemDefinition = Item;
	return Item;
}

FArcItemSpec FArcItemSpec::NewItem(const UArcItemDefinition* NewItem
								   , uint8 Level
								   , int32 Amount)
{
	FArcItemSpec NewEntry;
	NewEntry.ItemId = FArcItemId::Generate();
	NewEntry.Level = Level;
	NewEntry.Amount = Amount;
	if (NewItem)
	{
		NewEntry.ItemDefinition = NewItem;
		NewEntry.ItemDefinitionId = NewItem->GetPrimaryAssetId();
	}
	return NewEntry;
}

FArcItemSpec FArcItemSpec::NewItem(const FPrimaryAssetId& NewItem, uint8 Level, int32 Amount)
{
	FArcItemSpec NewEntry;
	
	NewEntry.ItemId = FArcItemId::Generate();
	NewEntry.Level = Level;
	NewEntry.Amount = Amount;
	NewEntry.SetItemDefinition(NewItem);

	return NewEntry;
}

