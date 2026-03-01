// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "ArcAreaTypes.h"
#include "ArcAreaGetSlotStatePropertyFunction.generated.h"

USTRUCT()
struct FArcAreaGetSlotStateInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 SlotIndex = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	EArcAreaSlotState SlotState = EArcAreaSlotState::Vacant;

	/** The entity assigned to this slot (invalid if Vacant/Disabled). */
	UPROPERTY(EditAnywhere, Category = Output)
	FMassEntityHandle AssignedEntity;
};

/**
 * Gets the state and assigned entity for a specific slot by index.
 */
USTRUCT(meta = (DisplayName = "Arc Get Area Slot State", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaGetSlotStatePropertyFunction : public FMassStateTreePropertyFunctionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaGetSlotStateInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};
