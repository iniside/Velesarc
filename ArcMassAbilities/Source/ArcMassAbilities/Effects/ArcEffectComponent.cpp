// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcEffectEventSubsystem.h"
#include "Events/ArcPendingEvents.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"

bool FArcEffectComp_TagFilter::OnPreApplication(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager)
	{
		return false;
	}

	const FMassEntityView EntityView(*Context.EntityManager, Context.TargetEntity);
	const FArcOwnedTagsFragment* TagsFragment = EntityView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
	if (!TagsFragment)
	{
		return RequirementFilter.IsEmpty();
	}

	return RequirementFilter.RequirementsMet(TagsFragment->Tags.GetTagContainer());
}

void FArcEffectComp_GrantTags::OnApplied(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager)
	{
		return;
	}

	FMassEntityView EntityView(*Context.EntityManager, Context.TargetEntity);
	FArcOwnedTagsFragment* TagsFragment = EntityView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
	if (TagsFragment)
	{
		TArray<FArcPendingTagEvent> PendingTagEvents;
		TagsFragment->AddTags(GrantedTags, PendingTagEvents, Context.TargetEntity);
		UArcEffectEventSubsystem* Events = Context.EntityManager->GetWorld()->GetSubsystem<UArcEffectEventSubsystem>();
		if (Events)
		{
			for (const FArcPendingTagEvent& Evt : PendingTagEvents)
			{
				Events->BroadcastTagCountChanged(Evt.Entity, Evt.Tag, Evt.OldCount, Evt.NewCount);
			}
		}
	}
}

void FArcEffectComp_GrantTags::OnRemoved(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager)
	{
		return;
	}

	FMassEntityView EntityView(*Context.EntityManager, Context.TargetEntity);
	FArcOwnedTagsFragment* TagsFragment = EntityView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
	if (TagsFragment)
	{
		TArray<FArcPendingTagEvent> PendingTagEvents;
		TagsFragment->RemoveTags(GrantedTags, PendingTagEvents, Context.TargetEntity);
		UArcEffectEventSubsystem* Events = Context.EntityManager->GetWorld()->GetSubsystem<UArcEffectEventSubsystem>();
		if (Events)
		{
			for (const FArcPendingTagEvent& Evt : PendingTagEvents)
			{
				Events->BroadcastTagCountChanged(Evt.Entity, Evt.Tag, Evt.OldCount, Evt.NewCount);
			}
		}
	}
}

void FArcEffectComp_ApplyOtherEffect::OnApplied(const FArcEffectContext& Context) const
{
	if (EffectToApply && Context.EntityManager)
	{
		ArcEffects::TryApplyEffect(*Context.EntityManager, Context.TargetEntity, EffectToApply, Context.SourceEntity, Context.SourceData);
	}
}

void FArcEffectComp_RemoveEffectByTag::OnApplied(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager || !EffectTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer Tags;
	Tags.AddTag(EffectTag);
	ArcEffects::RemoveEffectsByTag(*Context.EntityManager, Context.TargetEntity, Tags);
}

FGameplayTagContainer ArcEffectHelpers::GetEffectTags(const UArcEffectDefinition* Effect)
{
	FGameplayTagContainer Result;
	if (!Effect)
	{
		return Result;
	}
	for (const FInstancedStruct& CompStruct : Effect->Components)
	{
		if (const FArcEffectComp_EffectTags* TagsComp = CompStruct.GetPtr<FArcEffectComp_EffectTags>())
		{
			Result.AppendTags(TagsComp->OwnedTags);
		}
	}
	return Result;
}
