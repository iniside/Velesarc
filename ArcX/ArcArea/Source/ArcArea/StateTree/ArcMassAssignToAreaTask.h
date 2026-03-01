// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcAreaTypes.h"
#include "ArcMassAssignToAreaTask.generated.h"

USTRUCT()
struct FArcMassAssignToAreaTaskInstanceData
{
	GENERATED_BODY()

	/** Area to assign the NPC to. */
	UPROPERTY(EditAnywhere, Category = Input)
	FArcAreaHandle AreaHandle;

	/** Slot index within the area. */
	UPROPERTY(EditAnywhere, Category = Input)
	int32 SlotIndex = INDEX_NONE;

	/** Output: whether the assignment succeeded. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bAssignmentSucceeded = false;
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
