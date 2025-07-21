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
#include "Templates/SubclassOf.h"
#include "ArcCore/Items/Fragments/ArcItemFragment.h"
#include "Items/ArcItemInstance.h"
#include "GameplayAbilitySpec.h"
#include "ArcItemFragment_GrantedPassiveAbilities.generated.h"

class UGameplayAbility;
class UArcItemsStoreComponent;
class UArcCoreAbilitySystemComponent;

USTRUCT()
struct ARCCORE_API FArcItemInstance_GrantedPassiveAbilities : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

	friend struct FArcItemFragment_GrantedPassiveAbilities;
	
protected:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> GrantedPassiveAbilities;

	UPROPERTY(NotReplicated)
	TArray<FGameplayAbilitySpecHandle> PendingAbilities;

	FDelegateHandle PendingAbilitiesHandle;

	UArcItemsStoreComponent* ItemsStore = nullptr;
	UArcCoreAbilitySystemComponent* ArcASC = nullptr;
	
	const FArcItemData* Item = nullptr;
	FGameplayTag SlotId;
	
public:
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemInstance_GrantedPassiveAbilities::StaticStruct();
	}
	
	const TArray<FGameplayAbilitySpecHandle>& GetGrantedPassiveAbilities() const
	{
		return GrantedPassiveAbilities;
	}
	
	UArcCoreAbilitySystemComponent* GetASC() const
	{
		return ArcASC;
	}
	
	// This should prevent replication at all, since it is always true;
	virtual bool Equals(const FArcItemInstance& Other) const override
	{
		const FArcItemInstance_GrantedPassiveAbilities& Instance = static_cast<const FArcItemInstance_GrantedPassiveAbilities&>(Other);
		return GrantedPassiveAbilities.Num() == Instance.GrantedPassiveAbilities.Num();
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Granted Passive Abilities"))
struct ARCCORE_API FArcItemFragment_GrantedPassiveAbilities : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data", meta = (TitleProperty = "Abilities"))
	TArray<TSubclassOf<UGameplayAbility>> Abilities;

	virtual ~FArcItemFragment_GrantedPassiveAbilities() override = default;

	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcItemInstance_GrantedPassiveAbilities::StaticStruct();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemInstance_GrantedPassiveAbilities::StaticStruct();
	}
	
	virtual void OnItemAddedToSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;
	virtual void OnItemRemovedFromSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;

protected:
	void HandleOnAbilityGiven(FGameplayAbilitySpec& AbilitySpec, const FArcItemData* InItem) const;
	void UpdatePendingAbility(const FArcItemData* InItem) const;
};
