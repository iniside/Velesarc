// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "ArcAbilityTaskBase.generated.h"

UENUM()
enum class EArcGateHandling : uint8 { HandleInTree = 0, FailState = 1 };

USTRUCT(meta = (Hidden, DisplayName = "Arc Ability Task Base"))
struct ARCMASSABILITIES_API FArcAbilityTaskBase : public FStateTreeTaskBase
{
    GENERATED_BODY()

    /** True if the task drives its own scheduled-tick wakes via AddScheduledTickRequest.
     *  Tasks that override this to return true MUST also set bConsideredForScheduling = false
     *  in their constructor — otherwise the per-state tick aggregation forces every-frame
     *  ticking and defeats the scheduled wake. The manifest lint rule enforces this. */
    virtual bool DrivesScheduledTick() const { return false; }
};
