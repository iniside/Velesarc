// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "ArcRPGSkillCapHandler.generated.h"

USTRUCT(meta = (DisplayName = "RPG Skill Cap", ToolTip = "Assign to a skill maximum of FArcAttributeSet_RPGSkills (e.g. MaxOneHanded). Writes the lowest Cap Attributes value * Points Per Attribute as the base value, and lowers Skill to it when Skill is above. Physical and hybrid skills list their two governing attributes (skills.yaml cap); craft and domain skills list only Magic (magic.yaml skill_cap, domain_skill_cap). Writes nothing while any Cap Attribute is missing on the entity."))
struct ARCRPGMECHANICS_API FArcRPGAttributeHandler_SkillCap : public FArcAttributeHandler
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Cap", meta = (ToolTip = "The skill this maximum caps, on the same entity."))
	FArcAttributeRef Skill;

	UPROPERTY(EditAnywhere, Category = "Cap", meta = (TitleProperty = "PropertyName", ToolTip = "Attributes on the same entity; the lowest one sets the maximum. Two governing attributes for physical and hybrid skills, Magic alone for craft and domain skills."))
	TArray<FArcAttributeRef> CapAttributes;

	UPROPERTY(EditAnywhere, Category = "Cap", meta = (ClampMin = "0", ToolTip = "skills.yaml cap.per_point; magic.yaml skill_cap.per_point and domain_skill_cap.per_point."))
	float PointsPerAttribute = 5.f;

	virtual void Initialize(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float NewValue) const override;

	virtual void OnObservedAttributeChanged(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, const FArcAttributeRef& ChangedRef,
		float OldValue, float NewValue) const override;

	virtual void GatherInputs(TArray<FArcAttributeRef>& OutInputs) const override;

private:
	bool ComputeCap(FMassEntityManager& EntityManager, FMassEntityHandle Entity, float& OutCap) const;
};
