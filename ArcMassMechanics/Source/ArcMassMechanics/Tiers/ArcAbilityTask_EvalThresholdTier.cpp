// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Tiers/ArcAbilityTask_EvalThresholdTier.h"

#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Mass/EntityManager.h"

EStateTreeRunStatus FArcAbilityTask_EvalThresholdTier::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
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

	float Magnitude = Value->GetCurrentValue();
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

	UArcEffectDefinition* Effect = InstanceData.Table->FindEffect(InstanceData.Key, InstanceData.Tier);
	if (!Effect)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const FArcEffectContext EffectContext = ArcAbilities::MakeAbilityEffectContext(EntityManager, Entity, Handle);
	ArcEffects::TryApplyEffect(EntityManager, Entity, Effect, EffectContext);

	return EStateTreeRunStatus::Succeeded;
}
