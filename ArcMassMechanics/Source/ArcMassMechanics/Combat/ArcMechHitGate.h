// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectComponent.h"
#include "ArcMechHitGate.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Hit Gate", ToolTip = "On pre-application rolls the attacker's MissChance, then the defender's DodgeChance, ParryChance and BlockChance, in that order. Each chance is read with the spec's captured source and target tags, so tag-qualified modifiers decide when a defense applies; a chance of 0 never rolls. An attack from outside the defender's 90 degree front half-angle skips Parry and Block. The first successful roll vetoes the effect and publishes its ArcMech.Hit.Outcome.<Outcome> event; a Hit publishes nothing here (the damage execution publishes it)."))
struct ARCMASSMECHANICS_API FArcMechHitGate : public FArcEffectComponent
{
	GENERATED_BODY()

	virtual void OnPreApplication(const FArcEffectApplyRequest& Request, const FArcEffectSpec& Spec,
		FArcEffectPreApplicationResult& Result) const override;
};
