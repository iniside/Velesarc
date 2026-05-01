// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcPerceptionTypes.generated.h"

/**
 * Which perception senses to read perceived entities from.
 */
UENUM(BlueprintType)
enum class EArcTQSPerceptionSense : uint8
{
	Sight,
	Hearing,
	/** Merge both sight and hearing results (deduplicates). */
	Both
};
