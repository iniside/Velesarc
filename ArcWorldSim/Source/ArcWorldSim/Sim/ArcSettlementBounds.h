// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

namespace ArcWorldSim
{
struct FSettlementBound
{
	FMassEntityHandle Entity;
	FTransform Transform;
	FVector Extent = FVector::ZeroVector;
};

ARCWORLDSIM_API bool IsInsideSettlement(
	const FTransform& CenterTransform,
	const FVector& Extent,
	const FVector& WorldLocation);

ARCWORLDSIM_API FMassEntityHandle FindSettlementAt(
	const FVector& WorldLocation,
	TConstArrayView<FSettlementBound> Centers);
}
