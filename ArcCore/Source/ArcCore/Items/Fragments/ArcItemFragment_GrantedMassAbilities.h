#pragma once

#include "ArcCore/Items/Fragments/ArcItemFragment.h"
#include "Items/ArcItemInstance.h"
#include "Abilities/ArcAbilityHandle.h"
#include "GameplayTagContainer.h"
#include "ArcItemFragment_GrantedMassAbilities.generated.h"

class UArcAbilityDefinition;

USTRUCT(meta = (ToolTip = "A single Mass ability entry with definition and optional input binding tag."))
struct ARCCORE_API FArcMassAbilityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	TObjectPtr<UArcAbilityDefinition> AbilityDefinition;

	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

USTRUCT(meta = (ToolTip = "Mutable instance data tracking Mass ability handles granted by the item."))
struct ARCCORE_API FArcItemInstance_GrantedMassAbilities : public FArcItemInstance_ItemData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcAbilityHandle> GrantedAbilities;

	virtual bool Equals(const FArcItemInstance& Other) const override
	{
		const FArcItemInstance_GrantedMassAbilities& Instance =
			static_cast<const FArcItemInstance_GrantedMassAbilities&>(Other);
		return GrantedAbilities.Num() == Instance.GrantedAbilities.Num();
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Granted Mass Abilities", Category = "Mass Abilities",
	ToolTip = "Grants Mass abilities to the owning actor's Mass entity when the item is equipped to a slot, and removes them when unequipped."))
struct ARCCORE_API FArcItemFragment_GrantedMassAbilities : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (ForceInlineRow, ShowOnlyInnerProperties, TitleProperty = "AbilityDefinition"))
	TArray<FArcMassAbilityEntry> Abilities;

	virtual ~FArcItemFragment_GrantedMassAbilities() override = default;

	virtual UScriptStruct* GetItemInstanceType() const override
	{
		return FArcItemInstance_GrantedMassAbilities::StaticStruct();
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemFragment_GrantedMassAbilities::StaticStruct();
	}

	virtual void OnItemAddedToSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;
	virtual void OnItemRemovedFromSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;

#if WITH_EDITOR
	virtual FArcFragmentDescription GetDescription(const UScriptStruct* InStruct) const override;
#endif
};
