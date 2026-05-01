// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

struct FMassEntityManager;

namespace ArcConditionTestHelpers
{
    /** Creates an entity with all 13 condition fragments and their default const shared configs. */
    FMassEntityHandle CreateConditionEntity(FMassEntityManager& EntityManager);

    /** Creates a condition entity and adds ArcMassAbilities fragments needed for effect testing. */
    FMassEntityHandle CreateConditionEntityWithEffects(FMassEntityManager& EntityManager);

    /** Creates a full condition entity with effect fragments and an attribute handler config mapping all 13 saturation attributes. */
    FMassEntityHandle CreateFullConditionEntity(FMassEntityManager& EntityManager);
}
