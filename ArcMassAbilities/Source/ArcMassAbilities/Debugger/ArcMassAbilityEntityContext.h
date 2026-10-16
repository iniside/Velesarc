// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassAbilityEntityContext.generated.h"

/**
 * Pure-USTRUCT anchor used exclusively as the type-name match for TRACE_INSTANCE
 * and the Rewind Debugger track creator's GetTargetTypeNameInternal().
 * No fields or methods — it is a label, not a data type.
 */
USTRUCT()
struct ARCMASSABILITIES_API FArcMassAbilityEntityContext
{
	GENERATED_BODY()
};
