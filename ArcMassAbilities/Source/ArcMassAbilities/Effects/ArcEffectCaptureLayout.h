// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcExtraCapture.h"
#include "ArcEffectCaptureLayout.generated.h"

/** Transient cache — not serialized. Rebuilt in PostLoad / PostDuplicate / PostEditChangeProperty
 *  and by explicit BuildCaptureLayout() calls (e.g. in tests).
 *  UniqueCaptures / UniqueExtras are the deduplicated flat arrays in first-seen iteration order. */
USTRUCT()
struct ARCMASSABILITIES_API FArcEffectCaptureLayout
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcAttributeCaptureDef> UniqueCaptures;

	UPROPERTY()
	TArray<FArcExtraCaptureDef> UniqueExtras;
};
