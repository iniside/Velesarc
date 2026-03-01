// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcAdvertisementInstruction.generated.h"

/**
 * Base struct for advertisement behavioral instructions.
 * Derive from this to create typed instructions constrained via ExcludeBaseStruct.
 *
 * An advertisement instruction tells the claiming entity HOW to fulfill
 * the advertisement — what behavior to execute.
 *
 * Examples:
 * - FArcAdvertisementInstruction_StateTree — run a StateTree behavior
 */
USTRUCT(BlueprintType)
struct ARCKNOWLEDGE_API FArcAdvertisementInstruction
{
	GENERATED_BODY()

	virtual ~FArcAdvertisementInstruction() {}
};
