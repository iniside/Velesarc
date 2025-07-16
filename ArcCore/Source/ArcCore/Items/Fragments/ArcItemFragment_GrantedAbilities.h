/**
 * This file is part of ArcX.
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
#include "Items/ArcItemInstance.h"
#include "Templates/SubclassOf.h"
#include "GameplayAbilitySpec.h"
#include "ArcItemFragment_GrantedAbilities.generated.h"

class UGameplayAbility;
class UArcItemsStoreComponent;
class UArcCoreAbilitySystemComponent;

USTRUCT()
struct ARCCORE_API FArcAbilitySpecCustomData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UArcItemDefinition> SourceItemDef;

	UPROPERTY()
	FArcItemId SourceItemId;
};

USTRUCT()
struct ARCCORE_API FArcItemInstance_GrantedAbilities : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

	friend struct FArcItemFragment_GrantedAbilities;
	
protected:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> GrantedAbilities;

	UPROPERTY(NotReplicated)
	TArray<FGameplayAbilitySpecHandle> PendingAbilities;

	FDelegateHandle PendingAbilitiesHandle;

	UArcItemsStoreComponent* ItemsStore = nullptr;
	UArcCoreAbilitySystemComponent* ArcASC = nullptr;
	
	const FArcItemData* Item = nullptr;
	FGameplayTag SlotId;
	
public:
	virtual TSharedPtr<FArcItemInstance> Duplicate() const override
	{
		void* Allocated = FMemory::Malloc(GetScriptStruct()->GetCppStructOps()->GetSize(), GetScriptStruct()->GetCppStructOps()->GetAlignment());
		GetScriptStruct()->GetCppStructOps()->Construct(Allocated);
		TSharedPtr<FArcItemInstance> SharedPtr = MakeShareable(static_cast<FArcItemInstance*>(Allocated));
		
		return SharedPtr;
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemInstance_GrantedAbilities::StaticStruct();
	}

	const TArray<FGameplayAbilitySpecHandle> GetGrantedAbilities() const
	{
		return GrantedAbilities;
	}

	// This should prevent replication at all, since it is always true;
	virtual bool Equals(const FArcItemInstance& Other) const override
	{
		const FArcItemInstance_GrantedAbilities& Instance = static_cast<const FArcItemInstance_GrantedAbilities&>(Other);
		const bool bValue = GrantedAbilities.Num() == Instance.GrantedAbilities.Num();
		return bValue;
	}

protected:
	//void HandleOnAbilityGiven(FGameplayAbilitySpec& AbilitySpec);
	//void UpdatePendingAbility();
};


USTRUCT()
struct FArcAbilityEntry
{
	GENERATED_BODY()

public:
	/*
	 * True if InputTag should be automatically added from either struct or from slot on
	 * which this ability end up.
	 * If you want to add tag later change to false.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Data", Meta = (Categories = "InputTag"))
	bool bAddInputTag = true;

	UPROPERTY(EditDefaultsOnly, Category = "Data", Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TSubclassOf<UGameplayAbility> GrantedAbility;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Granted Abilities"))
struct ARCCORE_API FArcItemFragment_GrantedAbilities : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (ForceInlineRow, ShowOnlyInnerProperties, TitleProperty = "GrantedAbility"))
	TArray<FArcAbilityEntry> Abilities;

	virtual ~FArcItemFragment_GrantedAbilities() override = default;

	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcItemInstance_GrantedAbilities::StaticStruct();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemFragment_GrantedAbilities::StaticStruct();
	}
	
public:
	virtual void OnItemAddedToSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;
	virtual void OnItemRemovedFromSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;
	
protected:
	void HandleOnAbilityGiven(FGameplayAbilitySpec& AbilitySpec, const FArcItemData* InItem) const;
	void UpdatePendingAbility(const FArcItemData* InItem) const;
};
