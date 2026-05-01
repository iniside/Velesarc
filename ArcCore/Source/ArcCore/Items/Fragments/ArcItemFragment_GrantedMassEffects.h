#pragma once

#include "ArcCore/Items/Fragments/ArcItemFragment.h"
#include "Items/ArcItemInstance.h"
#include "Mass/EntityHandle.h"
#include "ArcItemFragment_GrantedMassEffects.generated.h"

class UArcEffectDefinition;

USTRUCT(meta = (ToolTip = "Mutable instance data tracking the source entity for Mass effect removal."))
struct ARCCORE_API FArcItemInstance_GrantedMassEffects : public FArcItemInstance_ItemData
{
    GENERATED_BODY()

    FMassEntityHandle SourceEntity;

    virtual bool Equals(const FArcItemInstance& Other) const override
    {
        return true;
    }
};

USTRUCT(BlueprintType, meta = (DisplayName = "Granted Mass Effects", Category = "Mass Abilities",
    ToolTip = "Applies Mass effects to the owning actor's Mass entity when equipped, removes them on unequip."))
struct ARCCORE_API FArcItemFragment_GrantedMassEffects : public FArcItemFragment_ItemInstanceBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Data")
    TArray<TObjectPtr<UArcEffectDefinition>> Effects;

    virtual ~FArcItemFragment_GrantedMassEffects() override = default;

    virtual UScriptStruct* GetItemInstanceType() const override
    {
        return FArcItemInstance_GrantedMassEffects::StaticStruct();
    }

    virtual UScriptStruct* GetScriptStruct() const override
    {
        return FArcItemFragment_GrantedMassEffects::StaticStruct();
    }

    virtual void OnItemAddedToSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;
    virtual void OnItemRemovedFromSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const override;

#if WITH_EDITOR
    virtual FArcFragmentDescription GetDescription(const UScriptStruct* InStruct) const override;
#endif
};
