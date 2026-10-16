// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectComp_GrantImmunity.h"
#include "Effects/ArcEffectContext.h"
#include "Fragments/ArcImmunityFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"

void FArcEffectComp_GrantImmunity::OnApplied(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager || !Context.EntityManager->IsEntityValid(Context.TargetEntity))
	{
		return;
	}

	FMassEntityView View(*Context.EntityManager, Context.TargetEntity);
	FArcImmunityFragment* ImmFrag = View.GetSparseFragmentDataPtr<FArcImmunityFragment>();
	if (ImmFrag)
	{
		ImmFrag->ImmunityTags.AddTags(GrantedImmunityTags);
	}
}

void FArcEffectComp_GrantImmunity::OnRemoved(const FArcEffectContext& Context, EArcEffectRemovalReason Reason) const
{
	if (!Context.EntityManager || !Context.EntityManager->IsEntityValid(Context.TargetEntity))
	{
		return;
	}

	FMassEntityView View(*Context.EntityManager, Context.TargetEntity);
	FArcImmunityFragment* ImmFrag = View.GetSparseFragmentDataPtr<FArcImmunityFragment>();
	if (ImmFrag)
	{
		ImmFrag->ImmunityTags.RemoveTags(GrantedImmunityTags);
	}
}
