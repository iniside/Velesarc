// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassDamageResistanceHandler.h"

#include "ArcConditionEffects/ArcConditionFragments.h"
#include "ArcConditionEffects/ArcConditionTypes.h"
#include "ArcMassDamageTypes.h"
#include "MassEntityView.h"

void FArcMassDamageResistanceHandler::PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue)
{
	if (FMath::IsNearlyZero(NewValue - OldValue))
	{
		return;
	}

	if (!MappingAsset || MappingAsset->Mappings.IsEmpty())
	{
		return;
	}

	FMassEntityView EntityView(Context.EntityManager, Context.Entity);

	FArcConditionStatesFragment* StatesFragment = EntityView.GetFragmentDataPtr<FArcConditionStatesFragment>();
	if (!StatesFragment)
	{
		return;
	}

	const FArcAttributeRef ResistanceRef = FArcMassDamageModifiersFragment::GetDamageResistanceAttribute();

	for (const FArcMassDamageConditionMapping& Mapping : MappingAsset->Mappings)
	{
		if (!Mapping.DamageTypeTag.IsValid()
			|| Mapping.ConditionTypeIndex < 0
			|| Mapping.ConditionTypeIndex >= ArcConditionTypeCount)
		{
			continue;
		}

		FGameplayTagContainer SourceTags;
		SourceTags.AddTag(Mapping.DamageTypeTag);

		const float ResistanceValue = Context.EvaluateWithTags(
			ResistanceRef,
			Attribute.BaseValue,
			SourceTags,
			FGameplayTagContainer());

		StatesFragment->States[Mapping.ConditionTypeIndex].Resistance = FMath::Clamp(ResistanceValue, 0.f, 1.f);
	}
}
