// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Conditions/ArcAbilityCondition_IsInputHeld.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "MassEntityView.h"

bool FArcAbilityCondition_IsInputHeld::TestCondition(FStateTreeExecutionContext& Context) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();
    const FMassEntityView EntityView(EntityManager, Entity);
    const FArcAbilityInputFragment& Input = EntityView.GetFragmentData<FArcAbilityInputFragment>();
    return Input.HeldInputs.HasTag(InputTag);
}
