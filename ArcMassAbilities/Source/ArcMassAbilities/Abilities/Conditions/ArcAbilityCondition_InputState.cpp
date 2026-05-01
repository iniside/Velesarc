// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Conditions/ArcAbilityCondition_InputState.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "MassEntityView.h"

bool FArcAbilityCondition_InputState::TestCondition(FStateTreeExecutionContext& Context) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();
    const FMassEntityView EntityView(EntityManager, Entity);
    const FArcAbilityInputFragment& Input = EntityView.GetFragmentData<FArcAbilityInputFragment>();

    switch (RequiredState)
    {
        case EArcInputState::Pressed:
            return Input.PressedThisFrame.Contains(InputTag);
        case EArcInputState::Released:
            return Input.ReleasedThisFrame.Contains(InputTag);
        case EArcInputState::Held:
            return Input.HeldInputs.HasTag(InputTag);
        case EArcInputState::None:
            return !Input.PressedThisFrame.Contains(InputTag)
                && !Input.ReleasedThisFrame.Contains(InputTag)
                && !Input.HeldInputs.HasTag(InputTag);
        default:
            return false;
    }
}
