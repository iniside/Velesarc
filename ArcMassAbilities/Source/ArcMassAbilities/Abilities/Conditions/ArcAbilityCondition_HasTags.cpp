// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Conditions/ArcAbilityCondition_HasTags.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/EntityView.h"

bool FArcAbilityCondition_HasTags::TestCondition(FStateTreeExecutionContext& Context) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();

    const FMassEntityView EntityView(EntityManager, Entity);
    const FArcOwnedTagsFragment* OwnedTagsPtr = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
    if (!OwnedTagsPtr)
    {
        return RequiredTags.IsEmpty();
    }

    return OwnedTagsPtr->Tags.HasAll(RequiredTags);
}
