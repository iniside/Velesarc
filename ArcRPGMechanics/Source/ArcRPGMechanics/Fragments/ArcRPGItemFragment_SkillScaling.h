// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcRPGItemFragment_SkillScaling.generated.h"

namespace ArcRPGSkillScaling
{
	inline constexpr float SkillMax = 100.f;
}

USTRUCT(BlueprintType, meta = (DisplayName = "Skill Scaling", Category = "Combat", ToolTip = "Skill and attribute steps of the abilities this item grants, read by the RPG Damage Execution and the RPG Status Duration when their effect spec is built. Skill power = Scaling's curve of the owner's Skill / 100, clamped to 0-1; a value becomes Base * (1 + Scale * skill power). Damage is then multiplied by max(0, 1 + K * modifier of the owner's Attribute). An unset Skill or Attribute skips its step."))
struct ARCRPGMECHANICS_API FArcRPGItemFragment_SkillScaling : public FArcItemFragment
{
	GENERATED_BODY()

	FArcRPGItemFragment_SkillScaling();

	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ToolTip = "The owner's skill attribute that drives the skill step."))
	FArcAttributeRef Skill;

	UPROPERTY(EditAnywhere, Category = "Skill", meta = (BaseStruct = "/Script/ArcMassMechanics.ArcValueScaling", ToolTip = "Curve of the skill step: skill power from the skill value and the skill max (100)."))
	FInstancedStruct Scaling;

	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (ToolTip = "The owner's attribute whose modifier floor((value - 10) / 2) drives the damage multiplier."))
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (ToolTip = "Damage is multiplied by max(0, 1 + K * attribute modifier)."))
	float K = 0.f;
};
