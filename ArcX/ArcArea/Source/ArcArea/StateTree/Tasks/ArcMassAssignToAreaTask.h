// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "MassSmartObjectRequest.h"
#include "ArcAreaTypes.h"
#include "ArcMassAssignToAreaTask.generated.h"

USTRUCT()
struct FArcMassAssignToAreaTaskInstanceData
{
	GENERATED_BODY()

	/** Area slot to assign the NPC to. */
	UPROPERTY(EditAnywhere, Category = Input)
	FArcAreaSlotHandle SlotHandle;

	/** Output: whether the assignment succeeded. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bAssignmentSucceeded = false;

	/** Output: SmartObject candidate slots from the area slot definition (1-4). */
	UPROPERTY(EditAnywhere, Category = Output)
	FMassSmartObjectCandidateSlots CandidateSlots;
};

/**
 * Instant StateTree task that assigns the current NPC to an area slot.
 * Calls UArcAreaSubsystem::AssignToSlot().
 */
USTRUCT(meta = (DisplayName = "Arc Assign To Area", Category = "Arc|Area|NPC"))
struct ARCAREA_API FArcMassAssignToAreaTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassAssignToAreaTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(60, 160, 220); }
#endif
};
