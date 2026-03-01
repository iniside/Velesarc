// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcKnowledgePayload.generated.h"

/**
 * Base struct for knowledge entry payloads.
 * Derive from this to create typed payloads constrained via ExcludeBaseStruct.
 *
 * Examples:
 * - Game code: FArcSettlementPayload with DisplayName, BoundingRadius, OwnerFaction
 * - ArcArea: FArcAreaVacancyPayload with AreaHandle, SlotIndex, RoleTag
 */
USTRUCT(BlueprintType)
struct ARCKNOWLEDGE_API FArcKnowledgePayload
{
	GENERATED_BODY()

	virtual ~FArcKnowledgePayload() {}
};
