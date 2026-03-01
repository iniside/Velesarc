// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ArcAreaGetSlotCountsPropertyFunction.generated.h"

USTRUCT()
struct FArcAreaGetSlotCountsInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	int32 TotalSlots = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 VacantCount = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 AssignedCount = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 ActiveCount = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 DisabledCount = 0;
};

/**
 * Reads the current Area entity's slot occupancy counts from UArcAreaSubsystem.
 * Requires FArcAreaFragment on the entity.
 */
USTRUCT(meta = (DisplayName = "Arc Get Area Slot Counts", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaGetSlotCountsPropertyFunction : public FMassStateTreePropertyFunctionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaGetSlotCountsInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};
