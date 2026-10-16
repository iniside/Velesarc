// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Templates/Function.h"

class UArcMassQuickBarConfig;
struct FActorTestSpawner;
struct FMassEntityManager;
struct FInstancedStruct;
struct FArcAbilityHandle;

namespace ArcMassItems::QuickBar::TestHelpers
{
    /** Test driver for the auto-select free functions: reads a COPY of the entity's
     *  FArcMassItemEventFragment::Events and forwards each slot-family event to
     *  ArcMassItems::QuickBar::AutoSelect — same semantics the retired signal
     *  processor had, without signal plumbing. Production uses
     *  UArcMassQuickBarAutoSelectSubsystem (delegate-driven) instead. */
    void RunAutoSelect(FMassEntityManager& EntityManager, FMassEntityHandle Entity);

    UArcMassQuickBarConfig* BuildQuickBarConfig(TFunctionRef<void(UArcMassQuickBarConfig&)> Builder);

    FMassEntityHandle CreateQuickBarEntity(
        FMassEntityManager& EntityManager,
        UArcMassQuickBarConfig* Config,
        TArrayView<const FInstancedStruct> ExtraFragments = {});

    void InjectGrantedAbility(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        FArcAbilityHandle Handle);
}
