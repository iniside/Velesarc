// Copyright Lukasz Baran. All Rights Reserved.

#include "RoadTool/ArcTerrainPathfinder.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

using namespace UE::Geometry;

namespace ArcRoadTool
{

namespace PathfinderInternal
{

int32 FindNearestVertex(const FDynamicMesh3& Mesh, const FDynamicMeshAABBTree3& AABBTree, const FVector& Location)
{
    double NearestDistSqr = 0.0;
    int32 NearestTriID = AABBTree.FindNearestTriangle(FVector3d(Location), NearestDistSqr);
    if (NearestTriID < 0)
    {
        return INDEX_NONE;
    }

    FIndex3i TriVerts = Mesh.GetTriangle(NearestTriID);
    int32 BestVert = TriVerts.A;
    double BestDist = FVector3d::DistSquared(FVector3d(Location), Mesh.GetVertex(TriVerts.A));

    for (int32 Idx = 1; Idx < 3; ++Idx)
    {
        int32 Vid = TriVerts[Idx];
        double Dist = FVector3d::DistSquared(FVector3d(Location), Mesh.GetVertex(Vid));
        if (Dist < BestDist)
        {
            BestDist = Dist;
            BestVert = Vid;
        }
    }

    return BestVert;
}

float ComputeEdgeCost(
    const FVector3d& FromPos,
    const FVector3d& ToPos,
    const FVector3d& PrevDirection,
    const FArcTerrainPathParams& Params)
{
    FVector3d EdgeDir = ToPos - FromPos;
    float EdgeLength = static_cast<float>(EdgeDir.Length());
    if (EdgeLength < KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    FVector3d EdgeDirNorm = EdgeDir / EdgeLength;

    float HeightDiff = static_cast<float>(ToPos.Z - FromPos.Z);
    float SlopeAngleDeg = FMath::RadiansToDegrees(
        FMath::Asin(FMath::Clamp(HeightDiff / EdgeLength, -1.0f, 1.0f)));

    if (FMath::Abs(SlopeAngleDeg) > Params.MaxSlopeAngleDegrees)
    {
        return TNumericLimits<float>::Max();
    }

    float SlopeFactor = 1.0f;
    if (FMath::Abs(SlopeAngleDeg) > Params.SteepSlopeThresholdDegrees)
    {
        float Excess = FMath::Abs(SlopeAngleDeg) - Params.SteepSlopeThresholdDegrees;
        float Range = Params.MaxSlopeAngleDegrees - Params.SteepSlopeThresholdDegrees;
        SlopeFactor = 1.0f + (Excess / FMath::Max(Range, 1.0f)) * Params.SlopeCostMultiplier;
    }

    if (HeightDiff > 0.0f)
    {
        SlopeFactor *= Params.UphillBias;
    }

    float CurvatureFactor = 1.0f;
    if (!PrevDirection.IsNearlyZero())
    {
        float DotVal = static_cast<float>(
            FVector3d::DotProduct(PrevDirection.GetSafeNormal(), EdgeDirNorm));
        float TurnAngle = FMath::Acos(FMath::Clamp(DotVal, -1.0f, 1.0f));
        CurvatureFactor = 1.0f + (TurnAngle / UE_PI) * Params.CurvatureCostMultiplier;
    }

    return EdgeLength * SlopeFactor * CurvatureFactor;
}

} // namespace PathfinderInternal

FArcTerrainPathResult FindTerrainPath(
    const FDynamicMesh3& Mesh,
    const FVector& StartLocation,
    const FVector& EndLocation,
    const FArcTerrainPathParams& Params)
{
    FArcTerrainPathResult Result;

    FDynamicMeshAABBTree3 AABBTree;
    AABBTree.SetMesh(&Mesh, true);

    int32 StartVert = PathfinderInternal::FindNearestVertex(Mesh, AABBTree, StartLocation);
    int32 EndVert = PathfinderInternal::FindNearestVertex(Mesh, AABBTree, EndLocation);

    if (StartVert == INDEX_NONE || EndVert == INDEX_NONE)
    {
        return Result;
    }

    if (StartVert == EndVert)
    {
        Result.PathPoints.Add(FVector(Mesh.GetVertex(StartVert)));
        Result.PathVertexIDs.Add(StartVert);
        Result.bSuccess = true;
        return Result;
    }

    int32 MaxVertexID = Mesh.MaxVertexID();

    TArray<float> Cost;
    TArray<int32> Parent;
    TArray<FVector3d> IncomingDir;
    Cost.SetNumUninitialized(MaxVertexID);
    Parent.SetNumUninitialized(MaxVertexID);
    IncomingDir.SetNumUninitialized(MaxVertexID);

    for (int32 Idx = 0; Idx < MaxVertexID; ++Idx)
    {
        Cost[Idx] = TNumericLimits<float>::Max();
        Parent[Idx] = INDEX_NONE;
        IncomingDir[Idx] = FVector3d::ZeroVector;
    }

    Cost[StartVert] = 0.0f;

    struct FQueueEntry
    {
        float CostValue;
        int32 VertexID;
        bool operator<(const FQueueEntry& Other) const { return CostValue < Other.CostValue; }
    };

    TArray<FQueueEntry> OpenSet;
    OpenSet.HeapPush(FQueueEntry{0.0f, StartVert});

    TSet<int32> ClosedSet;
    FVector3d EndPos = Mesh.GetVertex(EndVert);

    while (OpenSet.Num() > 0)
    {
        FQueueEntry Current;
        OpenSet.HeapPop(Current);

        if (ClosedSet.Contains(Current.VertexID))
        {
            continue;
        }
        ClosedSet.Add(Current.VertexID);

        if (Current.VertexID == EndVert)
        {
            break;
        }

        FVector3d CurrentPos = Mesh.GetVertex(Current.VertexID);
        FVector3d PrevDir = IncomingDir[Current.VertexID];

        Mesh.EnumerateVertexEdges(Current.VertexID, [&](int32 EdgeID)
        {
            FIndex2i EdgeVerts = Mesh.GetEdgeV(EdgeID);
            int32 NeighborVert = (EdgeVerts.A == Current.VertexID) ? EdgeVerts.B : EdgeVerts.A;

            if (ClosedSet.Contains(NeighborVert))
            {
                return;
            }

            FVector3d NeighborPos = Mesh.GetVertex(NeighborVert);
            float EdgeCost = PathfinderInternal::ComputeEdgeCost(
                CurrentPos, NeighborPos, PrevDir, Params);

            if (EdgeCost >= TNumericLimits<float>::Max())
            {
                return;
            }

            float NewCost = Cost[Current.VertexID] + EdgeCost;
            if (NewCost < Cost[NeighborVert])
            {
                Cost[NeighborVert] = NewCost;
                Parent[NeighborVert] = Current.VertexID;
                IncomingDir[NeighborVert] = NeighborPos - CurrentPos;

                float Heuristic = static_cast<float>(FVector3d::Dist(NeighborPos, EndPos));
                OpenSet.HeapPush(FQueueEntry{NewCost + Heuristic, NeighborVert});
            }
        });
    }

    if (Parent[EndVert] == INDEX_NONE && StartVert != EndVert)
    {
        return Result;
    }

    TArray<FVector> ReversePath;
    TArray<int32> ReverseIDs;
    int32 CurrentVert = EndVert;
    while (CurrentVert != INDEX_NONE)
    {
        ReversePath.Add(FVector(Mesh.GetVertex(CurrentVert)));
        ReverseIDs.Add(CurrentVert);
        CurrentVert = Parent[CurrentVert];
    }

    Result.PathPoints.Reserve(ReversePath.Num());
    Result.PathVertexIDs.Reserve(ReverseIDs.Num());
    for (int32 Idx = ReversePath.Num() - 1; Idx >= 0; --Idx)
    {
        Result.PathPoints.Add(ReversePath[Idx]);
        Result.PathVertexIDs.Add(ReverseIDs[Idx]);
    }

    Result.TotalCost = Cost[EndVert];
    Result.bSuccess = true;
    return Result;
}

} // namespace ArcRoadTool
