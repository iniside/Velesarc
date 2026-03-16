// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAoEBPF.h"

FTransform UArcAoEBPF::ComputeAoETransform(const FArcAoEShapeData& ShapeData, const FVector& TargetLocation, const FVector& SourceLocation)
{
	if (ShapeData.Shape == EArcAoEShape::Box)
	{
		FVector Direction = TargetLocation - SourceLocation;
		FVector BoxCenter;
		FQuat BoxRotation;
		ShapeData.ComputeBoxTransform(TargetLocation, Direction, BoxCenter, BoxRotation);
		return FTransform(BoxRotation, BoxCenter);
	}

	return FTransform(FQuat::Identity, TargetLocation);
}
