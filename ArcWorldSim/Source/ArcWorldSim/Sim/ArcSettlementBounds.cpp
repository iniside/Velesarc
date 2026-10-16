// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcSettlementBounds.h"

namespace ArcWorldSim
{
bool IsInsideSettlement(const FTransform& CenterTransform, const FVector& Extent, const FVector& WorldLocation)
{
	const FVector Local = CenterTransform.InverseTransformPosition(WorldLocation);
	return FMath::Abs(Local.X) <= Extent.X
		&& FMath::Abs(Local.Y) <= Extent.Y
		&& FMath::Abs(Local.Z) <= Extent.Z;
}

FMassEntityHandle FindSettlementAt(
	const FVector& WorldLocation,
	TConstArrayView<FSettlementBound> Centers)
{
	FMassEntityHandle Nearest;
	FVector::FReal BestDistSq = TNumericLimits<FVector::FReal>::Max();
	for (const FSettlementBound& Center : Centers)
	{
		if (!IsInsideSettlement(Center.Transform, Center.Extent, WorldLocation))
		{
			continue;
		}

		const FVector::FReal DistSq = FVector::DistSquared(Center.Transform.GetLocation(), WorldLocation);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Nearest = Center.Entity;
		}
	}
	return Nearest;
}
}
