#include "Items/Fragments/ArcItemFragment_GrantedMassEffects.h"
#include "Items/Fragments/ArcItemMassHelpers.h"
#include "Items/ArcItemsHelpers.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "MassEntityManager.h"

void FArcItemFragment_GrantedMassEffects::OnItemAddedToSlot(const FArcItemData* InItem,
                                                             const FGameplayTag& InSlotId) const
{
    FMassEntityManager* EntityManager = nullptr;
    FMassEntityHandle Entity;
    if (!ArcItemMassHelpers::GetMassEntity(InItem, EntityManager, Entity))
    {
        return;
    }

    FArcItemInstance_GrantedMassEffects* Instance =
        ArcItemsHelper::FindMutableInstance<FArcItemInstance_GrantedMassEffects>(InItem);
    Instance->SourceEntity = Entity;

    for (UArcEffectDefinition* Effect : Effects)
    {
        if (!Effect)
        {
            continue;
        }

        ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, Entity);
    }
}

void FArcItemFragment_GrantedMassEffects::OnItemRemovedFromSlot(const FArcItemData* InItem,
                                                                  const FGameplayTag& InSlotId) const
{
    FMassEntityManager* EntityManager = nullptr;
    FMassEntityHandle Entity;
    if (!ArcItemMassHelpers::GetMassEntity(InItem, EntityManager, Entity))
    {
        return;
    }

    FArcItemInstance_GrantedMassEffects* Instance =
        ArcItemsHelper::FindMutableInstance<FArcItemInstance_GrantedMassEffects>(InItem);

    for (UArcEffectDefinition* Effect : Effects)
    {
        if (!Effect)
        {
            continue;
        }

        ArcEffects::RemoveEffectBySource(*EntityManager, Entity, Effect, Instance->SourceEntity);
    }
}

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_GrantedMassEffects::GetDescription(const UScriptStruct* InStruct) const
{
    FArcFragmentDescription Desc = FArcItemFragment_ItemInstanceBase::GetDescription(InStruct);
    Desc.CommonPairings = {
        FName(TEXT("FArcItemFragment_GrantedMassAbilities")),
        FName(TEXT("FArcItemFragment_MassAttributeModifier"))
    };
    Desc.Prerequisites = { FName(TEXT("UArcCoreMassAgentComponent")) };
    Desc.UsageNotes = TEXT(
        "Applies persistent Mass effects while the item is equipped. "
        "Effects are applied on slot entry and removed on slot exit. "
        "Requires the owning actor to have a UArcCoreMassAgentComponent with a valid Mass entity.");
    return Desc;
}
#endif
