// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcRPGAbilityTask_SurgeTier.h"

#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcMassMechanics/Tiers/ArcTierTypes.h"
#include "ArcRPGMechanicsTags.h"
#include "Costs/ArcRPGItemCost.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Mass/EntityManager.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Sources/ArcSourceItemResolver.h"

namespace ArcRPGSurgeTierPrivate
{
	FGameplayTag FindSpellDomain(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle)
	{
		const FArcItemData* Item = ArcAbilityKit::FindSourceItem(EntityManager, Entity, Handle);
		if (!Item || !Item->GetItemDefinition())
		{
			return FGameplayTag();
		}

		const FArcItemFragment_Tags* TagsFragment = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(Item);
		if (!TagsFragment)
		{
			return FGameplayTag();
		}

		const FGameplayTag DomainRoot = ArcRPGMechanicsTags::SpellElement;
		for (const FGameplayTag& Tag : TagsFragment->ItemTags)
		{
			if (Tag != DomainRoot && Tag.MatchesTag(DomainRoot))
			{
				return Tag;
			}
		}
		return FGameplayTag();
	}
}

EStateTreeRunStatus FArcRPGAbilityTask_SurgeTier::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	const FMassEntityHandle Entity = AbilityContext.GetEntity();
	const FArcAbilityHandle Handle = AbilityContext.GetAbilityHandle();

	InstanceData.Magnitude = 0.f;
	InstanceData.Tier = 0;

	const FArcAttribute* Value = ArcAttributes::ResolveAttribute(EntityManager, Entity, InstanceData.ValueAttribute);
	if (!Value)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	float Magnitude = Value->GetCurrentValue() + ArcRPGItemCost::GetEntropyGenerated(EntityManager, Entity, Handle);
	if (InstanceData.ThresholdAttribute.IsValid())
	{
		const FArcAttribute* Threshold = ArcAttributes::ResolveAttribute(EntityManager, Entity, InstanceData.ThresholdAttribute);
		if (!Threshold)
		{
			return EStateTreeRunStatus::Succeeded;
		}
		Magnitude -= Threshold->GetCurrentValue();
	}

	InstanceData.Magnitude = Magnitude;
	InstanceData.Tier = InstanceData.Bands.GetTier(Magnitude);

	if (!InstanceData.Table)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	UArcEffectDefinition* Effect = InstanceData.Table->FindEffect(ArcRPGSurgeTierPrivate::FindSpellDomain(EntityManager, Entity, Handle), InstanceData.Tier);
	if (!Effect)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const FArcEffectContext EffectContext = ArcAbilities::MakeAbilityEffectContext(EntityManager, Entity, Handle);
	ArcEffects::TryApplyEffect(EntityManager, Entity, Effect, EffectContext);

	return EStateTreeRunStatus::Succeeded;
}
