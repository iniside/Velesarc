// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcAreaAssignmentCondition.generated.h"

USTRUCT()
struct FArcAreaAssignmentConditionInstanceData
{
	GENERATED_BODY()
};

/**
 * StateTree condition: checks whether the current NPC entity is assigned to an area slot.
 * Reads FArcAreaAssignmentFragment::IsAssigned().
 */
USTRUCT(meta = (DisplayName = "Arc Is Area Assigned", Category = "Arc|Area|NPC"))
struct ARCAREA_API FArcAreaAssignmentCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaAssignmentConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	/** If true, the condition returns true when the NPC is NOT assigned. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
};
