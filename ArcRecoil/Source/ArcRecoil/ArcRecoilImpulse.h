// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

struct FArcRecoilBakedLayerKick;

namespace ArcRecoil
{
	ARCRECOIL_API FVector2f ComputeShotImpulse(const FArcRecoilBakedLayerKick& Layer,
		int32 SuppressionShots,
		float ShotIndex,
		float WalkShots,
		FRandomStream& Stream);

	ARCRECOIL_API FVector PerturbAimDirection(const FVector& AimDirection,
		const FVector2f& AngularOffsetDeg,
		float SpreadHalfAngleDeg,
		float SpreadExponent,
		FRandomStream& Stream);
}
