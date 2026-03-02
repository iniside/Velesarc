// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_AreaSlotRoleFilter.generated.h"

/**
 * Filters TQS items by area slot role tag.
 * Reads AreaHandle + SlotIndex from ItemData, checks the slot definition's RoleTag.
 * Designed for items from the Area Vacant Slots generator.
 */
USTRUCT(DisplayName = "Area Slot Role Filter")
struct ARCAREA_API FArcTQSStep_AreaSlotRoleFilter : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_AreaSlotRoleFilter() { StepType = EArcTQSStepType::Filter; }

	/** Role tag query the slot's RoleTag must match. */
	UPROPERTY(EditAnywhere, Category = "Step")
	FGameplayTagQuery RoleTagQuery;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
