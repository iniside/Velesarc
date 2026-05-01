// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "MassStateTreeTypes.h"
#include "Mass/ArcAreaAssignmentFragments.h"
#include "MassSmartObjectRequest.h"
#include "ArcResolveBuildingInfoTask.generated.h"

USTRUCT()
struct FArcResolveBuildingInfoTaskInstanceData
{
    GENERATED_BODY()

	/** Output: the building entity handle (area owner). */
    UPROPERTY(VisibleAnywhere, Category = "Output")
    FMassEntityHandle BuildingHandle;

    /** Output: the area handle. */
    UPROPERTY(VisibleAnywhere, Category = "Output")
    FArcAreaHandle AreaHandle;

    /** Output: the specific slot handle for this NPC's assignment. */
    UPROPERTY(VisibleAnywhere, Category = "Output")
    FArcAreaSlotHandle SlotHandle;

    /** Output: the area/building location. */
    UPROPERTY(VisibleAnywhere, Category = "Output")
    FVector SlotLocation = FVector::ZeroVector;

    /** Output: up to 4 SmartObject candidate slots from the building's area. */
    UPROPERTY(VisibleAnywhere, Category = "Output")
    FMassSmartObjectCandidateSlots CandidateSlots;
};

/**
 * Reads the NPC's FArcAreaAssignmentFragment to resolve the assigned building
 * (area owner entity), area handle, and location.
 */
USTRUCT(DisplayName = "Arc Resolve Building Info", meta = (Category = "Economy"))
struct ARCECONOMY_API FArcResolveBuildingInfoTask : public FMassStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcResolveBuildingInfoTaskInstanceData;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool Link(FStateTreeLinker& Linker) override;
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

protected:
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagQuery SlotRequirements;
	
    TStateTreeExternalDataHandle<FArcAreaAssignmentFragment> AssignmentFragmentHandle;
};
