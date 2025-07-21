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
#include "ArcCore/Items/Fragments/ArcItemFragment.h"
#include "GameplayTagContainer.h"

#include "AssetRegistry/AssetData.h"

#include "ArcItemFragment_Tags.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Item Tags", Category = "Tags"))
struct ARCCORE_API FArcItemFragment_Tags : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer ItemTags;

	/*
	 * These Tags are not used internally, only indexed by asset registry;
	 * Ie. Can be used filter out item family for random spawning.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer AssetTags;

	/*
	 * Tags applied to owner of this item. When Item is equiped (not when it is mererly in
	 * inventory). Applied at the same as attribute trough default effect.
	 *
	 * Indexed by asset registy.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer GrantedTags;

	/* All of those tags must be present on owner for this item to be equipped on slot. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer RequiredTags;

	/* None of those tags must be present on owner for this item to be equipped on slot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer DenyTags;

	virtual ~FArcItemFragment_Tags() override = default;

	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override
	{
		{
			FString Tags;

			int32 Num = ItemTags.Num();
			int32 Idx = 0;
			for (const FGameplayTag& Tag : ItemTags)
			{
				Tags += Tag.ToString();
				Idx++;
				if (Num > Idx)
				{
					Tags += ",";
				}
			}
			Context.AddTag(UObject::FAssetRegistryTag(TEXT("ItemTags")
				, Tags
				, UObject::FAssetRegistryTag::TT_Hidden));
		}

		{
			FString Tags;
			int32 Num = AssetTags.Num();
			int32 Idx = 0;
			for (const FGameplayTag& Tag : AssetTags)
			{
				Tags += Tag.ToString();
				Idx++;
				if (Num > Idx)
				{
					Tags += ",";
				}
			}
			Context.AddTag(UObject::FAssetRegistryTag(TEXT("AssetTags")
				, Tags
				, UObject::FAssetRegistryTag::TT_Hidden));
		}

		{
			FString Tags;
			int32 Num = GrantedTags.Num();
			int32 Idx = 0;
			for (const FGameplayTag& Tag : GrantedTags)
			{
				Tags += Tag.ToString();
				Idx++;
				if (Num > Idx)
				{
					Tags += ",";
				}
			}
			Context.AddTag(UObject::FAssetRegistryTag(TEXT("GrantedTags")
				, Tags
				, UObject::FAssetRegistryTag::TT_Hidden));
		}

		{
			FString Tags;
			int32 Num = RequiredTags.Num();
			int32 Idx = 0;
			for (const FGameplayTag& Tag : RequiredTags)
			{
				Tags += Tag.ToString();
				Idx++;
				if (Num > Idx)
				{
					Tags += ",";
				}
			}
			Context.AddTag(UObject::FAssetRegistryTag(TEXT("RequiredTags")
				, Tags
				, UObject::FAssetRegistryTag::TT_Hidden));
		}

		{
			FString Tags;
			int32 Num = DenyTags.Num();
			int32 Idx = 0;
			for (const FGameplayTag& Tag : DenyTags)
			{
				Tags += Tag.ToString();
				Idx++;
				if (Num > Idx)
				{
					Tags += ",";
				}
			}
			Context.AddTag(UObject::FAssetRegistryTag(TEXT("DenyTags")
				, Tags
				, UObject::FAssetRegistryTag::TT_Hidden));
		}
	}
};