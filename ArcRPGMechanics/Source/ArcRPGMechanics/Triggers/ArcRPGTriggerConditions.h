// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassMechanics/Triggers/ArcTriggerTypes.h"
#include "ArcRPGTriggerConditions.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Same Source Item", ToolTip = "If only. Met when the hit comes from the same item that granted the trigger's host effect, so the item's conditional bonus applies only to hits made with that item. Example: a sword with If target HP < 50%: DamageDoneMultiplier +0.3 plus Same Source Item boosts only that sword's hits, not hits made with a second weapon or a spell. Not met when the hit or the host effect has no source item. Not available on On or While."))
struct ARCRPGMECHANICS_API FArcRPGTriggerCondition_SameSourceItem : public FArcTriggerCondition
{
	GENERATED_BODY()

	virtual bool IsMet(const FArcTriggerContext& Context) const override;
	virtual void ValidateInOn(TArray<FText>& OutErrors) const override;
	virtual void ValidateInWhile(TArray<FText>& OutErrors) const override;
};
