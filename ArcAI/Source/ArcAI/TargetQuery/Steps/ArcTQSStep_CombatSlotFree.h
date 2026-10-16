// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSStep_CombatSlotFree.generated.h"

USTRUCT(meta = (DisplayName = "Combat Slot Free", ToolTip = "Rejects items whose angle around Slot Owner is within Min Separation Degrees of another attacker's live combat slot. Passes every item when Slot Owner is not an active entity."))
struct ARCAI_API FArcTQSStep_CombatSlotFree : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_CombatSlotFree()
	{
		StepType = EArcTQSStepType::Filter;
	}

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ArcTQSRequiredBinding))
	FMassEntityHandle Claimant;

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ArcTQSRequiredBinding))
	FMassEntityHandle SlotOwner;

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees", ToolTip = "Smallest angle in degrees around Slot Owner between this item and another attacker's claimed slot."))
	float MinSeparationDegrees = 45.0f;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
