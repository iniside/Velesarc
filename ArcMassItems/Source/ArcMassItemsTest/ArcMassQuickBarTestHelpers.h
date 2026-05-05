// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Templates/Function.h"

class UArcMassQuickBarConfig;
class UArcMassQuickBarAutoSelectProcessor;
struct FActorTestSpawner;
struct FMassEntityManager;
struct FInstancedStruct;
struct FArcAbilityHandle;

namespace ArcMassItems::QuickBar::TestHelpers
{
    UArcMassQuickBarAutoSelectProcessor* RegisterQuickBarProcessor(FActorTestSpawner& Spawner, FMassEntityManager& EntityManager);

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
