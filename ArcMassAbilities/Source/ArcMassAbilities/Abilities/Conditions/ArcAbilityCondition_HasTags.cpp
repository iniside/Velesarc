// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Conditions/ArcAbilityCondition_HasTags.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "MassEntityView.h"

bool FArcAbilityCondition_HasTags::TestCondition(FStateTreeExecutionContext& Context) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();

    const FMassEntityView EntityView(EntityManager, Entity);
    const FArcOwnedTagsFragment& OwnedTags = EntityView.GetFragmentData<FArcOwnedTagsFragment>();

    return OwnedTags.Tags.HasAll(RequiredTags);
}
