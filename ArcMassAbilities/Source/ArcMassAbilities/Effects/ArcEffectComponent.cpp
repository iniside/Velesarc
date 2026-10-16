// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectComponent.h"
#include "ArcMassAbilitiesLogs.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectSpec.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Events/ArcGameEventTags.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Mass/EntityManager.h"

void FArcEffectComp_TagFilter::OnPreApplication(const FArcEffectApplyRequest& Request, const FArcEffectSpec& Spec,
                                                FArcEffectPreApplicationResult& Result) const
{
	if (RequirementFilter.RequirementsMet(Spec.CapturedTargetTags))
	{
		return;
	}

	UWorld* World = Spec.Context.EntityManager ? Spec.Context.EntityManager->GetWorld() : nullptr;
	ARCMA_VLOG(World, Warning,
		TEXT("[Effect] TagFilter rejected requirements=%s target_tags=%s target=%s"),
		*RequirementFilter.ToString(),
		*Spec.CapturedTargetTags.ToString(),
		*Spec.Context.TargetEntity.DebugGetDescription());
	Result.Veto(ArcGameEventTags::EffectRejectedGate);
}

void FArcEffectComp_GrantTags::OnApplied(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager)
	{
		return;
	}

	ArcAbilities::AddOwnedTags(*Context.EntityManager, Context.TargetEntity, GrantedTags);
}

void FArcEffectComp_GrantTags::OnRemoved(const FArcEffectContext& Context, EArcEffectRemovalReason Reason) const
{
	if (!Context.EntityManager)
	{
		return;
	}

	ArcAbilities::RemoveOwnedTags(*Context.EntityManager, Context.TargetEntity, GrantedTags);
}

void FArcEffectComp_ApplyOtherEffect::OnApplied(const FArcEffectContext& Context) const
{
	if (EffectToApply && Context.EntityManager)
	{
		const FArcEffectContext ChildContext = ArcEffects::MakeChildContext(Context);
		ArcEffects::TryApplyEffect(*Context.EntityManager, Context.TargetEntity, EffectToApply, ChildContext);
	}
}

void FArcEffectComp_RemoveEffects::OnApplied(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager || TagQuery.IsEmpty())
	{
		return;
	}

	FArcEffectRemovalQuery Query;
	Query.TagQuery = TagQuery;
	Query.ExcludeHandle = Context.EffectHandle;
	Query.MaxCount = MaxCount;
	Query.Order = Order;
	ArcEffects::RemoveEffects(*Context.EntityManager, Context.TargetEntity, Query);
}

void FArcEffectComp_EffectTags::GatherIdentityTags(FGameplayTagContainer& OutTags) const
{
	OutTags.AppendTags(OwnedTags);
}

const FGameplayTagContainer& ArcEffectHelpers::GetEffectTags(const UArcEffectDefinition* Effect)
{
	static const FGameplayTagContainer EmptyTags;
	return Effect ? Effect->GetEffectTags() : EmptyTags;
}
