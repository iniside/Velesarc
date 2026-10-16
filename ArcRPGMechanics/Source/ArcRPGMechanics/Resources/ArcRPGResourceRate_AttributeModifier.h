// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "ArcRPGResourceRate_AttributeModifier.generated.h"

USTRUCT(meta = (DisplayName = "Attribute Modifier Rate (RPG)", ToolTip = "Units per second toward the target: max(0, BasePerSecond * (1 + ModifierScale * modifier(Attribute))), with modifier = floor((Attribute - 10) / 2). Attribute is read live every update. 0 when Attribute is unset or missing on the entity."))
struct ARCRPGMECHANICS_API FArcRPGResourceRate_AttributeModifier : public FArcResourceRate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Rate", meta = (ToolTip = "Core stat whose modifier scales the rate, for example Willpower for entropy regen."))
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere, Category = "Rate", meta = (ClampMin = "0.0", ToolTip = "Units per second at a modifier of 0."))
	float BasePerSecond = 3.f;

	UPROPERTY(EditAnywhere, Category = "Rate", meta = (ToolTip = "Fractional rate change per point of modifier."))
	float ModifierScale = 0.1f;

	virtual float GetRate(const FArcResourceContext& Context) const override;
};
