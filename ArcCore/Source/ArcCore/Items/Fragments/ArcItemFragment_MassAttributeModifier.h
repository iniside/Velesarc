#pragma once

#include "ArcCore/Items/Fragments/ArcItemFragment.h"
#include "Items/ArcItemInstance.h"
#include "Modifiers/ArcDirectModifier.h"
#include "Attributes/ArcAggregator.h"
#include "ArcItemFragment_MassAttributeModifier.generated.h"

USTRUCT(meta = (ToolTip = "Mutable instance data tracking applied Mass modifier handles for removal on unequip."))
struct ARCCORE_API FArcItemInstance_MassAttributeModifiers : public FArcItemInstance_ItemData
{
    GENERATED_BODY()

    TArray<FArcModifierHandle> AppliedModifiers;

    virtual bool Equals(const FArcItemInstance& Other) const override
    {
        const FArcItemInstance_MassAttributeModifiers& Instance =
            static_cast<const FArcItemInstance_MassAttributeModifiers&>(Other);
        return AppliedModifiers.Num() == Instance.AppliedModifiers.Num();
    }
};

USTRUCT(BlueprintType, meta = (DisplayName = "Mass Attribute Modifier", Category = "Mass Abilities",
    ToolTip = "Directly modifies Mass entity attributes when equipped. No backing effect required — modifiers are applied as infinite-duration aggregator entries and removed on unequip."))
struct ARCCORE_API FArcItemFragment_MassAttributeModifier : public FArcItemFragment_ItemInstanceBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Data")
    TArray<FArcDirectModifier> Modifiers;

    virtual ~FArcItemFragment_MassAttributeModifier() override = default;

    virtual UScriptStruct* GetItemInstanceType() const override
    {
        return FArcItemInstance_MassAttributeModifiers::StaticStruct();
    }

    virtual UScriptStruct* GetScriptStruct() const override
    {
        return FArcItemFragment_MassAttributeModifier::StaticStruct();
    }

    virtual void OnItemAddedToSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;
    virtual void OnItemRemovedFromSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;

#if WITH_EDITOR
    virtual FArcFragmentDescription GetDescription(const UScriptStruct* InStruct) const override;
#endif
};
