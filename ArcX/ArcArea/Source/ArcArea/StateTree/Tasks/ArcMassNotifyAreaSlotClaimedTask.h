// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcAreaTypes.h"
#include "ArcMassNotifyAreaSlotClaimedTask.generated.h"

USTRUCT()
struct FArcMassNotifyAreaSlotClaimedTaskInstanceData
{
	GENERATED_BODY()
};

/**
 * Running StateTree task that bridges SmartObject claim lifecycle to the area subsystem.
 *
 * EnterState: reads the entity's FArcAreaAssignmentFragment and calls
 *             UArcAreaSubsystem::NotifySlotClaimed() → Assigned → Active.
 *             Returns Running.
 *
 * ExitState:  calls UArcAreaSubsystem::NotifySlotReleased() → Active → Assigned.
 *
 * Place as sibling/child of the SmartObject claim task in your StateTree.
 */
USTRUCT(meta = (DisplayName = "Arc Notify Area Slot Claimed", Category = "Arc|Area|NPC"))
struct ARCAREA_API FArcMassNotifyAreaSlotClaimedTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassNotifyAreaSlotClaimedTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(220, 160, 60); }
#endif
};
