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
#include "ActiveGameplayEffectHandle.h"
#include "ArcCore/Items/Fragments/ArcItemFragment.h"
#include "Items/ArcItemInstance.h"
#include "Templates/SubclassOf.h"
#include "ArcItemFragment_GrantedGameplayEffects.generated.h"

class UGameplayEffect;
USTRUCT(meta = (ToolTip = "Mutable instance data tracking active gameplay effect handles granted by the item. Handles are stored for removal when the item is unequipped."))
struct ARCCORE_API FArcItemInstance_GrantedGameplayEffects : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GrantedEffects;
	
};

USTRUCT(BlueprintType, meta = (DisplayName = "Granted Gameplay Effects", Category = "Gameplay Ability", ToolTip = "Applies gameplay effects to the owning character when the item is equipped to a slot, and removes them when unequipped. Use for stat buffs, resistances, or any persistent effect tied to wearing or holding an item."))
struct ARCCORE_API FArcItemFragment_GrantedGameplayEffects : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	static FArcItemFragment_GrantedGameplayEffects& Empty()
	{
		static FArcItemFragment_GrantedGameplayEffects E;
		return E;
	}

	UPROPERTY(EditAnywhere, SaveGame, Category = "Data", meta = (TitleProperty = "Effects"))
	TArray<TSubclassOf<UGameplayEffect>> Effects;

	virtual void OnItemAddedToSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;
	virtual void OnItemRemovedFromSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;
	
	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcItemInstance_GrantedGameplayEffects::StaticStruct();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemFragment_GrantedGameplayEffects::StaticStruct();
	}
	
	virtual ~FArcItemFragment_GrantedGameplayEffects() override
	{
	}

#if WITH_EDITOR
	virtual FArcFragmentDescription GetDescription(const UScriptStruct* InStruct) const override;
#endif
};
