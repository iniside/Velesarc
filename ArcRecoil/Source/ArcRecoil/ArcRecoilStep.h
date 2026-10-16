// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FArcRecoilProfileFragment;
struct FArcRecoilStateFragment;
struct FArcRecoilStats;

namespace ArcRecoil
{
	struct FArcRecoilShotImpulse
	{
		FVector2f CameraKickDeg = FVector2f::ZeroVector;
		FVector2f AimOffsetKickDeg = FVector2f::ZeroVector;
		float ShotIndexBefore = 0.0f;
		bool bSuppressed = false;
	};

	struct FArcRecoilStepReport
	{
		FVector2f PreZeroOffsetDeg = FVector2f::ZeroVector;
		FVector2f PreZeroVelDegSec = FVector2f::ZeroVector;
		bool bClamped = false;
		bool bDiverged = false;
	};

	ARCRECOIL_API FArcRecoilStats ClampStats(const FArcRecoilStats& Stats);

	ARCRECOIL_API FArcRecoilShotImpulse ApplyShot(FArcRecoilStateFragment& State,
		const FArcRecoilProfileFragment& Profile);

	ARCRECOIL_API FArcRecoilStepReport AdvanceState(FArcRecoilStateFragment& State,
		const FArcRecoilProfileFragment& Profile,
		float DeltaTime);

	ARCRECOIL_API float GetSpreadHalfAngleDeg(const FArcRecoilStateFragment& State,
		const FArcRecoilProfileFragment& Profile);
}
