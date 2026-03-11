// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ArcAreaTypes.h"
#include "ArcAreaSlotManagementTasks.generated.h"

// ------------------------------------------------------------------
// Disable Slot
// ------------------------------------------------------------------

USTRUCT()
struct FArcAreaDisableSlotTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	int32 SlotIndex = INDEX_NONE;
};

/**
 * Disables an area slot (prevents assignment). Reads AreaHandle from the entity.
 */
USTRUCT(meta = (DisplayName = "Arc Disable Area Slot", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaDisableSlotTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaDisableSlotTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(60, 160, 220); }
#endif
};

// ------------------------------------------------------------------
// Enable Slot
// ------------------------------------------------------------------

USTRUCT()
struct FArcAreaEnableSlotTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	int32 SlotIndex = INDEX_NONE;
};

/**
 * Re-enables a disabled area slot (returns it to Vacant).
 */
USTRUCT(meta = (DisplayName = "Arc Enable Area Slot", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaEnableSlotTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaEnableSlotTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
	virtual FColor GetIconColor() const override { return FColor(60, 160, 220); }
#endif
};
