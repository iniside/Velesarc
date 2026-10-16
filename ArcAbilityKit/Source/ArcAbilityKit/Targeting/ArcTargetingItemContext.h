// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemId.h"
#include "ArcTargetingItemContext.generated.h"

struct FArcItemData;

/**
 * ArcAbilityKit-side payload for FArcTargetingContext::ExtraData. ArcTargeting itself
 * doesn't know about items; this struct lives in ArcAbilityKit so targeting phase ops
 * that need item context can read it via Ctx.FindExtra<FArcTargetingItemContext>().
 *
 * Renamed from FArcTargetingArcCoreContext (ArcCore/Targeting/ArcTargetingArcCoreContext.h).
 * The original struct in ArcCore stays in place until Phase 2 swap is complete.
 */
USTRUCT()
struct ARCABILITYKIT_API FArcTargetingItemContext
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FArcItemId SourceItemId;

	/** Raw ptr — request-lifetime only. Caller guarantees the FArcItemData outlives the request. */
	FArcItemData* SourceItemPtr = nullptr;
};
