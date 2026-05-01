// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/ArcMassAbilityCost.h"
#include "Effects/ArcEffectFunctions.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Attributes/ArcAttribute.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"

bool FArcMassAbilityCost_ApplyEffect::CheckCost(const FArcMassAbilityCostContext& Context) const
{
    return CostEffect != nullptr;
}

void FArcMassAbilityCost_ApplyEffect::ApplyCost(const FArcMassAbilityCostContext& Context) const
{
    if (CostEffect)
    {
        ArcEffects::TryApplyEffect(Context.EntityManager, Context.Entity, CostEffect, Context.Entity);
    }
}

bool FArcMassAbilityCost_InstantModifier::CheckCost(const FArcMassAbilityCostContext& Context) const
{
    if (!Attribute.IsValid())
    {
        return true;
    }

    FProperty* Prop = Attribute.GetCachedProperty();
    if (!Prop)
    {
        return false;
    }

    FMassEntityView View(Context.EntityManager, Context.Entity);
    FStructView FragView = View.GetFragmentDataStruct(Attribute.FragmentType);
    uint8* Memory = FragView.GetMemory();
    if (!Memory)
    {
        return false;
    }

    const FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(Memory);
    return Attr->BaseValue >= Amount.GetValue();
}

void FArcMassAbilityCost_InstantModifier::ApplyCost(const FArcMassAbilityCostContext& Context) const
{
    if (!Attribute.IsValid())
    {
        return;
    }

    float CurrentValue = ArcAttributes::GetValue(Context.EntityManager, Context.Entity, Attribute);
    ArcAttributes::SetValue(Context.EntityManager, Context.Entity, Attribute, CurrentValue - Amount.GetValue());
}
