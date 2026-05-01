// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectComp_GrantImmunity.h"
#include "Effects/ArcEffectContext.h"
#include "Fragments/ArcImmunityFragment.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"

void FArcEffectComp_GrantImmunity::OnApplied(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager || !Context.EntityManager->IsEntityValid(Context.TargetEntity))
	{
		return;
	}

	FMassEntityView View(*Context.EntityManager, Context.TargetEntity);
	FArcImmunityFragment* ImmFrag = View.GetFragmentDataPtr<FArcImmunityFragment>();
	if (ImmFrag)
	{
		ImmFrag->ImmunityTags.AppendTags(GrantedImmunityTags);
	}
}

void FArcEffectComp_GrantImmunity::OnRemoved(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager || !Context.EntityManager->IsEntityValid(Context.TargetEntity))
	{
		return;
	}

	FMassEntityView View(*Context.EntityManager, Context.TargetEntity);
	FArcImmunityFragment* ImmFrag = View.GetFragmentDataPtr<FArcImmunityFragment>();
	if (ImmFrag)
	{
		ImmFrag->ImmunityTags.RemoveTags(GrantedImmunityTags);
	}
}
