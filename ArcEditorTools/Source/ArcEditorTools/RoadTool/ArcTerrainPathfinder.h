// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::Geometry { class FDynamicMesh3; }

namespace ArcRoadTool
{

struct FArcTerrainPathParams
{
    float MaxSlopeAngleDegrees = 45.0f;
    float SteepSlopeThresholdDegrees = 30.0f;
    float SlopeCostMultiplier = 2.0f;
    float CurvatureCostMultiplier = 0.5f;
    float UphillBias = 1.5f;
};

struct FArcTerrainPathResult
{
    TArray<FVector> PathPoints;
    TArray<int32> PathVertexIDs;
    float TotalCost = 0.0f;
    bool bSuccess = false;
};

FArcTerrainPathResult FindTerrainPath(
    const UE::Geometry::FDynamicMesh3& Mesh,
    const FVector& StartLocation,
    const FVector& EndLocation,
    const FArcTerrainPathParams& Params = FArcTerrainPathParams());

} // namespace ArcRoadTool
