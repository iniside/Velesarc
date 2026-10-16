// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcStatusTestTypes.h"

#include "StateTreeExecutionContext.h"

namespace ArcStatusTestTags
{
	UE_DEFINE_GAMEPLAY_TAG(Tier1, "Test.MassMechanics.Status.Tier1");
	UE_DEFINE_GAMEPLAY_TAG(Tier2, "Test.MassMechanics.Status.Tier2");
	UE_DEFINE_GAMEPLAY_TAG(Tier3, "Test.MassMechanics.Status.Tier3");
	UE_DEFINE_GAMEPLAY_TAG(DamageFire, "Test.MassMechanics.Damage.Fire");
	UE_DEFINE_GAMEPLAY_TAG(DamageCold, "Test.MassMechanics.Damage.Cold");
	UE_DEFINE_GAMEPLAY_TAG(Group, "Test.MassMechanics.Status.Group");
	UE_DEFINE_GAMEPLAY_TAG(OtherGroup, "Test.MassMechanics.Status.OtherGroup");
	UE_DEFINE_GAMEPLAY_TAG(Marker, "Test.MassMechanics.Status.Marker");
	UE_DEFINE_GAMEPLAY_TAG(InputA, "Test.MassMechanics.Status.Input.A");
	UE_DEFINE_GAMEPLAY_TAG(InputB, "Test.MassMechanics.Status.Input.B");
	UE_DEFINE_GAMEPLAY_TAG(InputC, "Test.MassMechanics.Status.Input.C");
	UE_DEFINE_GAMEPLAY_TAG(AbilityMelee, "Test.MassMechanics.Status.Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(AbilitySpell, "Test.MassMechanics.Status.Ability.Spell");
	UE_DEFINE_GAMEPLAY_TAG(AbilityPassive, "Test.MassMechanics.Status.Ability.Passive");
	UE_DEFINE_GAMEPLAY_TAG(Elite, "Test.MassMechanics.Status.Rank.Elite");
	UE_DEFINE_GAMEPLAY_TAG(Boss, "Test.MassMechanics.Status.Rank.Boss");
}

EStateTreeRunStatus FArcMechTest_RunForeverTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}
