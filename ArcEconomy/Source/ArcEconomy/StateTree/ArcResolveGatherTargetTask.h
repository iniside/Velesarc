// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "MassStateTreeTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "MassSmartObjectRequest.h"
#include "GameplayTagContainer.h"
#include "ArcResolveGatherTargetTask.generated.h"

USTRUCT()
struct FArcResolveGatherTargetTaskInstanceData
{
    GENERATED_BODY()

    // Output — resource target
    UPROPERTY(VisibleAnywhere, Category = Output)
    FMassEntityHandle TargetResourceHandle;

    UPROPERTY(VisibleAnywhere, Category = Output)
    FVector TargetLocation = FVector::ZeroVector;

    // Output — SmartObject slots on the resource entity
    UPROPERTY(VisibleAnywhere, Category = Output)
    FMassSmartObjectCandidateSlots ResourceCandidateSlots;

    UPROPERTY(VisibleAnywhere, Category = Output)
    bool bHasResourceSlots = false;

    // Output — SmartObject slots on the gathering building (for return trip)
    UPROPERTY(VisibleAnywhere, Category = Output)
    FMassSmartObjectCandidateSlots BuildingCandidateSlots;

    UPROPERTY(VisibleAnywhere, Category = Output)
    bool bHasBuildingSlots = false;

    UPROPERTY(VisibleAnywhere, Category = Output)
    bool bHasAssignment = false;
};

USTRUCT(meta = (DisplayName = "Arc Resolve Gather Target", Category = "Economy"))
struct ARCECONOMY_API FArcResolveGatherTargetTask : public FMassStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcResolveGatherTargetTaskInstanceData;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool Link(FStateTreeLinker& Linker) override;
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
    virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Task"); }
    virtual FColor GetIconColor() const override { return FColor(60, 160, 100); }
#endif

protected:
    TStateTreeExternalDataHandle<FArcGathererFragment> GathererFragmentHandle;

    UPROPERTY(EditAnywhere, Category = Parameter)
    FGameplayTagQuery ResourceSlotRequirements;

    UPROPERTY(EditAnywhere, Category = Parameter)
    FGameplayTagQuery BuildingSlotRequirements;
};
