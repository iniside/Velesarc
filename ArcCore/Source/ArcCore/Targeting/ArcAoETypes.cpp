// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAoETypes.h"

void FArcAoEShapeData::ComputeBoxTransform(const FVector& SourceLocation, const FVector& Direction, FVector& OutCenter, FQuat& OutRotation) const
{
	FVector FlatDirection = Direction;
	FlatDirection.Z = 0.0f;

	if (FlatDirection.IsNearlyZero())
	{
		OutCenter = SourceLocation;
		OutRotation = FQuat::Identity;
		return;
	}

	FlatDirection.Normalize();

	FRotator YawRotation = FlatDirection.Rotation();
	OutRotation = YawRotation.Quaternion();
	OutCenter = SourceLocation + (FlatDirection * BoxExtent.X);
}
