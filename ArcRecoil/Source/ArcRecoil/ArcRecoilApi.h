// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Math/RandomStream.h"

struct FArcRecoilStats;
struct FMassEntityManager;
class UArcRecoilPreset;

namespace ArcRecoil
{
	ARCRECOIL_API void SeedStreamForEntity(FRandomStream& Stream, FMassEntityHandle Entity);

	ARCRECOIL_API void NotifyShotFired(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		int32 ShotCount = 1);

	ARCRECOIL_API FVector2f ConsumePendingCameraKick(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity);

	ARCRECOIL_API bool GetAimPerturbation(const FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FVector2f& OutAngularOffsetDeg,
		float& OutSpreadHalfAngleDeg,
		float& OutSpreadExponent);

	ARCRECOIL_API bool ApplyProfile(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UArcRecoilPreset& Preset,
		const FArcRecoilStats& Stats);
}
