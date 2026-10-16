// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/Handlers/ArcRPGSkillCapHandler.h"

#include "Mass/EntityManager.h"
#include "Modifiers/ArcModifierFunctions.h"

void FArcRPGAttributeHandler_SkillCap::Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const
{
	float Cap = 0.f;
	if (!ComputeCap(Context.EntityManager, Context.Entity, Cap))
	{
		return;
	}

	Attribute.InitValue(Cap);

	FArcAttribute* SkillAttribute = Skill.IsValid() ? ArcAttributes::ResolveAttribute(Context.EntityManager, Context.Entity, Skill) : nullptr;
	if (SkillAttribute && SkillAttribute->GetBaseValue() > Cap)
	{
		SkillAttribute->InitValue(Cap);
	}
}

void FArcRPGAttributeHandler_SkillCap::OnObservedAttributeChanged(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute,
	const FArcAttributeRef& ChangedRef, float OldValue, float NewValue) const
{
	float Cap = 0.f;
	if (!ComputeCap(Context.EntityManager, Context.Entity, Cap))
	{
		return;
	}

	if (Cap != Attribute.GetBaseValue() && Context.Attribute.IsValid())
	{
		ArcAttributes::QueueSetValue(Context.EntityManager, Context.Entity, Context.Attribute, Cap);
	}

	const FArcAttribute* SkillAttribute = Skill.IsValid() ? ArcAttributes::ResolveAttribute(Context.EntityManager, Context.Entity, Skill) : nullptr;
	if (SkillAttribute && SkillAttribute->GetBaseValue() > Cap)
	{
		ArcAttributes::QueueSetValue(Context.EntityManager, Context.Entity, Skill, Cap);
	}
}

void FArcRPGAttributeHandler_SkillCap::GatherInputs(TArray<FArcAttributeRef>& OutInputs) const
{
	for (const FArcAttributeRef& Ref : CapAttributes)
	{
		if (Ref.IsValid())
		{
			OutInputs.AddUnique(Ref);
		}
	}
}

bool FArcRPGAttributeHandler_SkillCap::ComputeCap(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float& OutCap) const
{
	if (CapAttributes.IsEmpty())
	{
		return false;
	}

	float Lowest = TNumericLimits<float>::Max();
	for (const FArcAttributeRef& Ref : CapAttributes)
	{
		const FArcAttribute* Input = Ref.IsValid() ? ArcAttributes::ResolveAttribute(EntityManager, Entity, Ref) : nullptr;
		if (!Input)
		{
			return false;
		}
		Lowest = FMath::Min(Lowest, Input->GetCurrentValue());
	}

	OutCap = Lowest * PointsPerAttribute;
	return true;
}
