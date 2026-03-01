// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "GameplayTagContainer.h"
#include "ArcAreaFindVacantSlotPropertyFunction.generated.h"

USTRUCT()
struct FArcAreaFindVacantSlotInstanceData
{
	GENERATED_BODY()

	/** Optional: only consider slots with this role tag. If None, any vacant slot matches. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag RoleTagFilter;

	/** Index of the first vacant slot found, or INDEX_NONE. */
	UPROPERTY(EditAnywhere, Category = Output)
	int32 SlotIndex = INDEX_NONE;

	/** Whether a vacant slot was found. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bFound = false;
};

/**
 * Finds the first vacant slot in the current Area entity.
 * Optionally filters by role tag.
 */
USTRUCT(meta = (DisplayName = "Arc Find Vacant Slot", Category = "Arc|Area|Self"))
struct ARCAREA_API FArcAreaFindVacantSlotPropertyFunction : public FMassStateTreePropertyFunctionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAreaFindVacantSlotInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};
