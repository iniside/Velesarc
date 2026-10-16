// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectTagSource.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectComponent.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Misc/ScopeRWLock.h"

namespace ArcEffectTagSourceRegistry
{
	FRWLock Lock;
	TArray<TSharedRef<const FArcEffectTagSource>> Sources;

	void AppendOwnedTags(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FGameplayTagContainer& OutTags)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			return;
		}

		const FMassEntityView View(EntityManager, Entity);
		const FArcOwnedTagsFragment* TagsFragment = View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		if (TagsFragment)
		{
			OutTags.AppendTags(TagsFragment->Tags.GetTagContainer());
		}
	}
}

void FArcEffectTagSource_OwnedTags::AppendTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
                                               FGameplayTagContainer& OutTargetTags) const
{
	if (!Context.EntityManager)
	{
		return;
	}

	ArcEffectTagSourceRegistry::AppendOwnedTags(*Context.EntityManager, Context.SourceEntity, OutSourceTags);
	ArcEffectTagSourceRegistry::AppendOwnedTags(*Context.EntityManager, Context.TargetEntity, OutTargetTags);
}

void FArcEffectTagSource_Definition::AppendTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
                                                FGameplayTagContainer& OutTargetTags) const
{
	OutSourceTags.AppendTags(ArcEffectHelpers::GetEffectTags(Context.EffectDefinition));
}

void FArcEffectTagSource_Ability::AppendTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
                                             FGameplayTagContainer& OutTargetTags) const
{
	if (Context.SourceAbility)
	{
		OutSourceTags.AppendTags(Context.SourceAbility->AbilityTags);
	}
}

void FArcEffectTagSource_Origin::AppendTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
                                            FGameplayTagContainer& OutTargetTags) const
{
	OutSourceTags.AppendTags(Context.OriginTags);
}

namespace ArcEffectTagSources
{

void Register(const TSharedRef<const FArcEffectTagSource>& Source)
{
	FWriteScopeLock WriteLock(ArcEffectTagSourceRegistry::Lock);
	ArcEffectTagSourceRegistry::Sources.AddUnique(Source);
}

void Unregister(const TSharedRef<const FArcEffectTagSource>& Source)
{
	FWriteScopeLock WriteLock(ArcEffectTagSourceRegistry::Lock);
	ArcEffectTagSourceRegistry::Sources.Remove(Source);
}

void GatherTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
                FGameplayTagContainer& OutTargetTags)
{
	FReadScopeLock ReadLock(ArcEffectTagSourceRegistry::Lock);
	for (const TSharedRef<const FArcEffectTagSource>& Source : ArcEffectTagSourceRegistry::Sources)
	{
		Source->AppendTags(Context, OutSourceTags, OutTargetTags);
	}
}

}
