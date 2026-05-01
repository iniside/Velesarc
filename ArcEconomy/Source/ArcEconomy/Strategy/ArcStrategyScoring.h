// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UtilityAI/ArcUtilityTypes.h"
#include "Strategy/ArcStrategyTypes.h"

struct FArcUtilityContext;
class UArcStrategyActionSet;

namespace ArcStrategy::Scoring
{
    /**
     * Score all actions in the action set and select a winner.
     * Context must have QuerierEntity, EntityManager, and World populated.
     */
    ARCECONOMY_API FArcStrategyDecision EvaluateActionSet(
        const UArcStrategyActionSet& ActionSet,
        FArcUtilityContext& Context);
}
