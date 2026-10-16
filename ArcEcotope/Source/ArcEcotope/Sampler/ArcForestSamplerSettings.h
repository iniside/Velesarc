// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcForestSamplerSettings.generated.h"

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestSamplerSettings
{
	GENERATED_BODY()

	/** Spread of stem sizes: 0 puts every stem in one height class, 1 gives the reverse-J of an all-aged stand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sampler", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnevenAgedness = 0.75f;

	/** How close two stems may stand, as a fraction of the placed stem's own crown radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sampler", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InhibitionStrength = 0.70f;
};
