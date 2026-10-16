// Copyright Lukasz Baran. All Rights Reserved.

#include "Fragments/ArcRPGItemFragment_SkillScaling.h"

#include "ArcMassMechanics/Combat/ArcMechCombatSlots.h"

FArcRPGItemFragment_SkillScaling::FArcRPGItemFragment_SkillScaling()
{
	Scaling.InitializeAs<FArcValueScaling>();
}
