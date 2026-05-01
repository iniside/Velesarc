// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassSpawnOrigin.generated.h"

UENUM(BlueprintType)
enum class EArcMassSpawnOrigin : uint8
{
	HitPoint,
	Origin,
	EntityLocation,
	Custom
};
