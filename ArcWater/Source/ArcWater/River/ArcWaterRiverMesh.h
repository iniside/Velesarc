// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "DynamicMeshBuilder.h"

class UArcWaterRiverComponent;

namespace ArcWaterRiverMesh
{
	/** False when the tangent is within 3 degrees of vertical; the outputs then carry a horizontal fallback frame, never a zero one. */
	bool ResolveFrameAt(const UArcWaterRiverComponent& River, float DistanceAlong,
		ESplineCoordinateSpace::Type Space, FVector& OutCenter, FVector& OutTangent, FVector& OutRight);

	void BuildRibbon(const UArcWaterRiverComponent& River,
		TArray<FDynamicMeshVertex>& OutVertices, TArray<uint32>& OutIndices);

	FBox ComputeLocalBounds(const UArcWaterRiverComponent& River);
}
