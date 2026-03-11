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
#include "ArcItemFragment.h"
#include "Items/ArcItemInstance.h"
#include "Templates/SubclassOf.h"

#include "ArcItemFragment_GrantAttributes.generated.h"

class UGameplayEffect;

USTRUCT(meta = (ToolTip = "A single SetByCaller attribute modifier entry. Pairs a gameplay tag key with a float magnitude, used by the backing GameplayEffect to apply attribute changes."))
struct ARCCORE_API FArcGrantAttributesSetByCaller
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTag SetByCallerTag;

	UPROPERTY(EditAnywhere)
	float Magnitude = 0;
};

USTRUCT(meta = (ToolTip = "Mutable instance data tracking dynamic attribute modifiers and the applied gameplay effect handle. Stores per-item dynamic SetByCaller attribute adjustments at runtime."))
struct ARCCORE_API FArcItemInstance_GrantAttributes : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<FArcGrantAttributesSetByCaller> DynamicAttributes;

	FActiveGameplayEffectHandle AppliedEffectHandle;

	// This should prevent replication at all, since it is always true;
	virtual bool Equals(const FArcItemInstance& Other) const override
	{
		return true;
	}
};

USTRUCT(meta = (DisplayName = "Grant Attributes", Category = "Gameplay Ability", ToolTip = "Modifies gameplay attributes on the owning character when equipped. Uses a backing GameplayEffect with SetByCaller magnitudes for both static and dynamic attribute modifiers. Use for items that add health, strength, or other attribute bonuses."))
struct ARCCORE_API FArcItemFragment_GrantAttributes : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TSubclassOf<UGameplayEffect> BackingGameplayEffect;
	
	UPROPERTY(EditAnywhere)
	TArray<FArcGrantAttributesSetByCaller> StaticAttributes;
	
public:
	virtual void OnItemAddedToSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const;
	virtual void OnItemRemovedFromSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const;
	
	virtual ~FArcItemFragment_GrantAttributes() override = default;

	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcItemInstance_GrantAttributes::StaticStruct();
	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemFragment_GrantAttributes::StaticStruct();
	}

#if WITH_EDITOR
	virtual FArcFragmentDescription GetDescription(const UScriptStruct* InStruct) const override;
#endif
};
