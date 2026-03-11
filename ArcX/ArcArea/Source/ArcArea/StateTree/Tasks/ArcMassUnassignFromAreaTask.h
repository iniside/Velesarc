// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcMassUnassignFromAreaTask.generated.h"

USTRUCT()
struct FArcMassUnassignFromAreaTaskInstanceData
{
	GENERATED_BODY()

	/** Output: whether the entity was previously assigned (and is now unassigned). */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bWasAssigned = false;
};

/**
 * Instant StateTree task that unassigns the current NPC from its area slot.
 * Reads the entity's FArcAreaAssignmentFragment and calls UArcAreaSubsystem::UnassignEntity().
 */
USTRUCT(meta = (DisplayName = "Arc Unassign From Area", Category = "Arc|Area|NPC"))
struct ARCAREA_API FArcMassUnassignFromAreaTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassUnassignFromAreaTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(220, 80, 60); }
#endif
};
