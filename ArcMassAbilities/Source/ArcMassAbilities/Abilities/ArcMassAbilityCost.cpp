// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/ArcMassAbilityCost.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectFunctions.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Mass/EntityManager.h"

bool FArcMassAbilityCost_ApplyEffect::CheckCost(const FArcMassAbilityCostContext& Context, FArcCostCheckResult* OutResult) const
{
    const bool bCanAfford = (CostEffect != nullptr);
    if (OutResult)
    {
        OutResult->bCanAfford = bCanAfford;
        if (!bCanAfford)
        {
            OutResult->Reason = EArcCostFailReason::Unsatisfied;
        }
    }
    return bCanAfford;
}

void FArcMassAbilityCost_ApplyEffect::ApplyCost(const FArcMassAbilityCostContext& Context) const
{
    if (CostEffect)
    {
		const FArcEffectContext EffectContext = ArcAbilities::MakeAbilityEffectContext(
			Context.EntityManager, Context.Entity, Context.AbilityHandle);
		ArcEffects::TryApplyEffect(Context.EntityManager, Context.Entity, CostEffect, EffectContext);
    }
}
