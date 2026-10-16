// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"

#include "ArcGameplayInteractionStateTreeBases.generated.h"

/**
 * Marker base for Mass-aware StateTree tasks that opt into ArcGameplayInteraction
 * trees. Mass tasks that do not derive from this base are filtered out of the
 * ArcGameplayInteraction schema picker.
 */
USTRUCT(meta = (Hidden))
struct ARCGAMEPLAYINTERACTION_API FArcGameplayInteractionMassTaskBase : public FMassStateTreeTaskBase
{
	GENERATED_BODY()
};

USTRUCT(meta = (Hidden))
struct ARCGAMEPLAYINTERACTION_API FArcGameplayInteractionMassConditionBase : public FMassStateTreeConditionBase
{
	GENERATED_BODY()
};

USTRUCT(meta = (Hidden))
struct ARCGAMEPLAYINTERACTION_API FArcGameplayInteractionMassEvaluatorBase : public FMassStateTreeEvaluatorBase
{
	GENERATED_BODY()
};
