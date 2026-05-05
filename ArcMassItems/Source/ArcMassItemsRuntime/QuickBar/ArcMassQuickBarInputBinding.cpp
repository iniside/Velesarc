// Copyright Lukasz Baran. All Rights Reserved.

#include "QuickBar/ArcMassQuickBarInputBinding.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Fragments/ArcItemFragment_GrantedMassAbilities.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "ArcMassAbilities/Fragments/ArcAbilityCollectionFragment.h"

namespace ArcMassItems::QuickBar::InputBinding
{

bool BindInputForItem(
    FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    const FArcItemData* ItemData,
    FGameplayTag InputTag)
{
    if (!EntityManager.IsEntityValid(Entity) || !ItemData || !InputTag.IsValid())
    {
        return false;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityCollectionFragment* Collection = EntityView.GetFragmentDataPtr<FArcAbilityCollectionFragment>();
    if (!Collection)
    {
        return false;
    }

    const FArcItemInstance_GrantedMassAbilities* GrantedAbilities =
        ArcItemsHelper::FindInstance<FArcItemInstance_GrantedMassAbilities>(ItemData);
    if (!GrantedAbilities)
    {
        return false;
    }

    for (const FArcAbilityHandle& Handle : GrantedAbilities->GrantedAbilities)
    {
        for (FArcActiveAbility& Ability : Collection->Abilities)
        {
            if (Ability.Handle == Handle)
            {
                Ability.InputTag = InputTag;
                break;
            }
        }
    }

    return true;
}

bool UnbindInputForItem(
    FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    const FArcItemData* ItemData,
    FGameplayTag InputTag)
{
    if (!EntityManager.IsEntityValid(Entity) || !ItemData)
    {
        return false;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityCollectionFragment* Collection = EntityView.GetFragmentDataPtr<FArcAbilityCollectionFragment>();
    if (!Collection)
    {
        return false;
    }

    const FArcItemInstance_GrantedMassAbilities* GrantedAbilities =
        ArcItemsHelper::FindInstance<FArcItemInstance_GrantedMassAbilities>(ItemData);
    if (!GrantedAbilities)
    {
        return false;
    }

    for (const FArcAbilityHandle& Handle : GrantedAbilities->GrantedAbilities)
    {
        for (FArcActiveAbility& Ability : Collection->Abilities)
        {
            if (Ability.Handle == Handle && Ability.InputTag == InputTag)
            {
                Ability.InputTag = FGameplayTag();
                break;
            }
        }
    }

    return true;
}

} // namespace ArcMassItems::QuickBar::InputBinding
