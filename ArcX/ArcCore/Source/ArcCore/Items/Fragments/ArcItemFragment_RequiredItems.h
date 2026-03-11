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


#pragma once
#include "ArcItemFragment.h"
#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"

#include "ArcItemFragment_RequiredItems.generated.h"

USTRUCT(BlueprintType, meta = (ToolTip = "Pairs an item definition with a required count. Used to specify exact item types and quantities needed for crafting, use, or other requirement checks."))
struct ARCCORE_API FArcItemDefCount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FArcNamedPrimaryAssetId ItemDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Count = 1;
};

USTRUCT(BlueprintType, meta = (ToolTip = "Pairs a set of required gameplay tags with a count. Items matching all required tags satisfy this requirement. Used for tag-based crafting or use prerequisites."))
struct ARCCORE_API FArcItemTagCount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Count = 1;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Required Items", Category = "Requirements", ToolTip = "Defines items required to use or craft this item. RequiredItemDefs specifies exact item definitions with counts, RequiredItemsWithTags matches items by tag criteria."))
struct ARCCORE_API FArcItemFragment_RequiredItems : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game"))
	TArray<FArcItemDefCount> RequiredItemDefs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FArcItemTagCount> RequiredItemsWithTags;
};