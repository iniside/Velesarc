// Copyright Lukasz Baran. All Rights Reserved.

#include "QuickBar/ArcMassQuickBarInputBinding_Ability.h"

#include "Abilities/ArcAbilityDefinition.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "ItemFragments/ArcItemFragment_GrantedMassAbilities.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"

namespace ArcAbilityKit::QuickBar::Internal
{
    bool PassesAbilityTagFilter(const FArcActiveAbility& Ability, const FGameplayTagContainer& RequiredTags)
    {
        if (RequiredTags.IsEmpty())
        {
            return true;
        }
        return Ability.Definition && Ability.Definition->AbilityTags.HasAll(RequiredTags);
    }
}

void FArcMassQuickBarInputBinding_Ability::Bind(
    FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    const FArcItemData* ItemData) const
{
    if (!EntityManager.IsEntityValid(Entity) || !ItemData || !InputTag.IsValid())
    {
        return;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityCollectionFragment* Collection = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
    if (!Collection)
    {
        return;
    }

    const FArcItemInstance_GrantedMassAbilities* GrantedAbilities =
        ArcItemsHelper::FindInstance<FArcItemInstance_GrantedMassAbilities>(ItemData);
    if (!GrantedAbilities)
    {
        return;
    }

    for (const FArcAbilityHandle& Handle : GrantedAbilities->GrantedAbilities)
    {
        for (FArcActiveAbility& Ability : Collection->Abilities)
        {
            if (Ability.Handle != Handle)
            {
                continue;
            }
            if (!ArcAbilityKit::QuickBar::Internal::PassesAbilityTagFilter(Ability, BindingRequiredTags))
            {
                break;
            }
            Ability.InputTag = InputTag;
            break;
        }
    }
}

void FArcMassQuickBarInputBinding_Ability::Unbind(
    FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    const FArcItemData* ItemData) const
{
    if (!EntityManager.IsEntityValid(Entity) || !ItemData)
    {
        return;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityCollectionFragment* Collection = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
    if (!Collection)
    {
        return;
    }

    const FArcItemInstance_GrantedMassAbilities* GrantedAbilities =
        ArcItemsHelper::FindInstance<FArcItemInstance_GrantedMassAbilities>(ItemData);
    if (!GrantedAbilities)
    {
        return;
    }

    for (const FArcAbilityHandle& Handle : GrantedAbilities->GrantedAbilities)
    {
        for (FArcActiveAbility& Ability : Collection->Abilities)
        {
            if (Ability.Handle != Handle)
            {
                continue;
            }
            if (Ability.InputTag != InputTag)
            {
                break;
            }
            if (!ArcAbilityKit::QuickBar::Internal::PassesAbilityTagFilter(Ability, BindingRequiredTags))
            {
                break;
            }
            Ability.InputTag = FGameplayTag();
            break;
        }
    }
}
