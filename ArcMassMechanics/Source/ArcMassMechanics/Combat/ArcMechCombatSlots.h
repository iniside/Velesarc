// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMechCombatSlots.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Value Scaling", ToolTip = "Power = Curve(Skill / SkillMax), with Skill / SkillMax clamped to 0-1; a value becomes Base * (1 + Scale * Power). Skill and SkillMax are inputs from the caller. The base curve is linear; game modules override Curve."))
struct ARCMASSMECHANICS_API FArcValueScaling
{
	GENERATED_BODY()

	virtual ~FArcValueScaling() = default;

	float Power(float Skill, float SkillMax) const;

	float Evaluate(float Base, float Scale, float Skill, float SkillMax) const;

protected:
	virtual float Curve(float Power) const;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Armor Curve", ToolTip = "Damage * (1 - Armor / (Armor + K * Damage)). K is an input from the Execution; K at or below 0 leaves the damage unchanged. Game modules override Mitigate."))
struct ARCMASSMECHANICS_API FArcArmorCurve
{
	GENERATED_BODY()

	virtual ~FArcArmorCurve() = default;

	virtual float Mitigate(float Damage, float Armor, float K) const;
};
