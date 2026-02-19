// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

// ---------------------------------------------------------------------------
// Macro: Implements a per-condition trait's BuildTemplate
// ---------------------------------------------------------------------------

#define ARC_IMPLEMENT_CONDITION_TRAIT(Name) \
	void UArc##Name##ConditionTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const \
	{ \
		BuildContext.AddFragment<FArc##Name##ConditionFragment>(); \
		\
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World); \
		const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(ConditionConfig); \
		BuildContext.AddConstSharedFragment(ConfigFragment); \
	}

ARC_IMPLEMENT_CONDITION_TRAIT(Burning)
ARC_IMPLEMENT_CONDITION_TRAIT(Bleeding)
ARC_IMPLEMENT_CONDITION_TRAIT(Chilled)
ARC_IMPLEMENT_CONDITION_TRAIT(Shocked)
ARC_IMPLEMENT_CONDITION_TRAIT(Poisoned)
ARC_IMPLEMENT_CONDITION_TRAIT(Diseased)
ARC_IMPLEMENT_CONDITION_TRAIT(Weakened)
ARC_IMPLEMENT_CONDITION_TRAIT(Oiled)
ARC_IMPLEMENT_CONDITION_TRAIT(Wet)
ARC_IMPLEMENT_CONDITION_TRAIT(Corroded)
ARC_IMPLEMENT_CONDITION_TRAIT(Blinded)
ARC_IMPLEMENT_CONDITION_TRAIT(Suffocating)
ARC_IMPLEMENT_CONDITION_TRAIT(Exhausted)

#undef ARC_IMPLEMENT_CONDITION_TRAIT

// ---------------------------------------------------------------------------
// Composite Trait — adds all 13 conditions
// ---------------------------------------------------------------------------

void UArcAllConditionsTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

#define ARC_ADD_CONDITION(Name, ConfigVar) \
	BuildContext.AddFragment<FArc##Name##ConditionFragment>(); \
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(ConfigVar));

	ARC_ADD_CONDITION(Burning,     BurningConfig)
	ARC_ADD_CONDITION(Bleeding,    BleedingConfig)
	ARC_ADD_CONDITION(Chilled,     ChilledConfig)
	ARC_ADD_CONDITION(Shocked,     ShockedConfig)
	ARC_ADD_CONDITION(Poisoned,    PoisonedConfig)
	ARC_ADD_CONDITION(Diseased,    DiseasedConfig)
	ARC_ADD_CONDITION(Weakened,    WeakenedConfig)
	ARC_ADD_CONDITION(Oiled,       OiledConfig)
	ARC_ADD_CONDITION(Wet,         WetConfig)
	ARC_ADD_CONDITION(Corroded,    CorrodedConfig)
	ARC_ADD_CONDITION(Blinded,     BlindedConfig)
	ARC_ADD_CONDITION(Suffocating, SuffocatingConfig)
	ARC_ADD_CONDITION(Exhausted,   ExhaustedConfig)

#undef ARC_ADD_CONDITION
}
