// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Combat/ArcMechCombatSlots.h"

float FArcValueScaling::Power(float Skill, float SkillMax) const
{
	return Curve(SkillMax > 0.f ? FMath::Clamp(Skill / SkillMax, 0.f, 1.f) : 0.f);
}

float FArcValueScaling::Evaluate(float Base, float Scale, float Skill, float SkillMax) const
{
	return Base * (1.f + Scale * Power(Skill, SkillMax));
}

float FArcValueScaling::Curve(float Power) const
{
	return Power;
}

float FArcArmorCurve::Mitigate(float Damage, float Armor, float K) const
{
	if (Damage <= 0.f || Armor <= 0.f || K <= 0.f)
	{
		return Damage;
	}

	return Damage * (1.f - Armor / (Armor + K * Damage));
}
