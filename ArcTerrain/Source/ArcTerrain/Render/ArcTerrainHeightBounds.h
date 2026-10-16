// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ArcTerrain
{
	inline constexpr float DefaultConservativeMinHeightCm = -1000000.0f;
	inline constexpr float DefaultConservativeMaxHeightCm = 1000000.0f;

	/** One contract for shader culling, primitive bounds and baked-content validation. Invalid authored
	 *  values fall back to the conservative default; finite reversed endpoints are ordered. */
	inline FVector2f ResolveConservativeHeightRangeCm(float AuthoredMinHeightCm, float AuthoredMaxHeightCm)
	{
		if (!FMath::IsFinite(AuthoredMinHeightCm) || !FMath::IsFinite(AuthoredMaxHeightCm))
		{
			return FVector2f(DefaultConservativeMinHeightCm, DefaultConservativeMaxHeightCm);
		}
		return FVector2f(FMath::Min(AuthoredMinHeightCm, AuthoredMaxHeightCm),
			FMath::Max(AuthoredMinHeightCm, AuthoredMaxHeightCm));
	}
}
