// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectContext.h"

struct FArcGameEvent;
struct FMassEntityManager;

namespace ArcGameEvents
{
	/** Fills SourceTags and TargetTags from the registered effect tag sources: Context with Instigator as source and Target as target. */
	ARCMASSABILITIES_API void GatherEnvelopeTags(FMassEntityManager& EntityManager, FArcGameEvent& Event,
	                                             const FArcEffectContext& Context = FArcEffectContext());
}
