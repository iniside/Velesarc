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
#include "Items/Fragments/ArcItemFragment.h"
#include "GameplayTagContainer.h"

#include "AssetRegistry/AssetData.h"

#include "ArcItemFragment_Tags.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Item Tags", Category = "Tags", ToolTip = "Categorizes and filters items using gameplay tags. ItemTags identify the item, AssetTags are matched by tag-based recipe ingredients and settlement hauling, GrantedTags are applied to the owner while the item is active, RequiredTags and DenyTags gate equipping or visibility."))
struct ARCMASSITEMSRUNTIME_API FArcItemFragment_Tags : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer ItemTags;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer AssetTags;

	// Added to the owner's tags while the item is active (ArcMassItems::ActivateItem).
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer GrantedTags;

	/**
	 * Abstract list of tags, required by this item. Does not assume, where the tags come from, or what the item is using them for.
	 * Examples, can be checked when equipping item, or check if player is in correct state to see item.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer RequiredTags;

	/**
	* Abstract list of tags, required by this item to not be present. Does not assume, where the tags come from, or what the item is using them for.
	 * Examples, can be checked when equipping item, or check if player is in correct state to see item.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer DenyTags;

	virtual ~FArcItemFragment_Tags() override = default;

#if WITH_EDITOR
	virtual FArcFragmentDescription GetDescription(const UScriptStruct* InStruct) const override;
#endif

	virtual void GetAssetRegistryTags(FAssetDataTagMap& OutTags) const override;
};