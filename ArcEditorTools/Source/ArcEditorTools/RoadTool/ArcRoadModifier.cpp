// Copyright Lukasz Baran. All Rights Reserved.

#include "RoadTool/ArcRoadModifier.h"
#include "RoadTool/ArcTerrainPathfinder.h"
#include "RoadTool/ArcPathSmoother.h"
#include "MeshPartitionMeshView.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "PrimitiveDrawInterface.h"
#include "ZoneShapeActor.h"
#include "ZoneShapeComponent.h"
#include "Engine/World.h"
#include "Async/ParallelFor.h"

using namespace UE::MeshPartition;

namespace RoadModifierInternal
{

struct FNearestPolylineResult
{
    FVector NearestPoint = FVector::ZeroVector;
    float Distance = TNumericLimits<float>::Max();
    int32 SegmentIndex = INDEX_NONE;
    float SegmentAlpha = 0.0f;
};

FNearestPolylineResult FindNearestPointOnPolyline(
    const FVector& Point,
    TConstArrayView<FVector> Polyline)
{
    FNearestPolylineResult Result;

    for (int32 Idx = 0; Idx < Polyline.Num() - 1; ++Idx)
    {
        FVector SegStart = Polyline[Idx];
        FVector SegEnd = Polyline[Idx + 1];
        FVector SegDir = SegEnd - SegStart;
        float SegLengthSqr = SegDir.SizeSquared();

        float Alpha = 0.0f;
        if (SegLengthSqr > KINDA_SMALL_NUMBER)
        {
            Alpha = FMath::Clamp(
                FVector::DotProduct(Point - SegStart, SegDir) / SegLengthSqr,
                0.0f, 1.0f);
        }

        FVector Closest = FMath::Lerp(SegStart, SegEnd, Alpha);
        float Dist = FVector::Dist(Point, Closest);

        if (Dist < Result.Distance)
        {
            Result.NearestPoint = Closest;
            Result.Distance = Dist;
            Result.SegmentIndex = Idx;
            Result.SegmentAlpha = Alpha;
        }
    }

    return Result;
}

} // namespace RoadModifierInternal

// ---- FBackgroundOp ----

class UArcRoadModifier::FBackgroundOp : public IModifierBackgroundOp
{
public:
    FBackgroundOp(const FName& InOperationName)
        : IModifierBackgroundOp(InOperationName)
    {
    }

    FTransform ComponentTransform;
    FBox WorldBounds;

    FVector StartPoint;
    FVector EndPoint;
    float RoadHalfWidth = 250.0f;
    float FalloffDistance = 200.0f;

    ArcRoadTool::FArcTerrainPathParams PathParams;
    ArcRoadTool::FArcSmoothParams SmoothParams;

    TSharedPtr<TArray<FVector>> SharedSmoothedPath;

    virtual void GetInstancesInBounds(
        const FBox& InBounds,
        TArray<FInstanceInfo>& OutInstanceInfos) const override
    {
        if (!WorldBounds.Intersect(InBounds))
        {
            return;
        }

        FInstanceInfo Info;
        Info.Bounds = WorldBounds;
        Info.InstanceID = 0;
        Info.ReadViewComponents = EMeshViewComponents::DynamicSubmesh;
        Info.WriteViewComponents = EMeshViewComponents::VertexPos;
        OutInstanceInfos.Add(Info);
    }

    virtual void ApplyModifications(
        FMeshView& InMeshView,
        const FTransform3d& InTransform,
        const FInstanceInfo& InInstanceInfo) const override
    {
        // Phase 1: Pathfind on submesh
        const UE::Geometry::FDynamicMesh3& Submesh = InMeshView.GetSubmesh();

        FVector LocalStart = FVector(InTransform.InverseTransformPosition(FVector3d(StartPoint)));
        FVector LocalEnd = FVector(InTransform.InverseTransformPosition(FVector3d(EndPoint)));

        ArcRoadTool::FArcTerrainPathResult PathResult =
            ArcRoadTool::FindTerrainPath(Submesh, LocalStart, LocalEnd, PathParams);

        if (!PathResult.bSuccess || PathResult.PathPoints.Num() < 2)
        {
            return;
        }

        // Smooth in local space
        TArray<FVector> SmoothedLocal = ArcRoadTool::SmoothPath(PathResult.PathPoints, SmoothParams);

        if (SmoothedLocal.Num() < 2)
        {
            return;
        }

        // Store world-space path for ZoneShape generation
        TArray<FVector> SmoothedWorld;
        SmoothedWorld.Reserve(SmoothedLocal.Num());
        for (const FVector& LocalPt : SmoothedLocal)
        {
            SmoothedWorld.Add(FVector(InTransform.TransformPosition(FVector3d(LocalPt))));
        }

        if (SharedSmoothedPath.IsValid())
        {
            *SharedSmoothedPath = MoveTemp(SmoothedWorld);
        }

        // Phase 2: Flatten vertices (local space)
        int32 VertexCount = InMeshView.VertexCount();

        ParallelFor(VertexCount, [&](int32 VertexIndex)
        {
            FVector3d VertexPos = InMeshView.GetVertexPos(VertexIndex);

            RoadModifierInternal::FNearestPolylineResult Nearest =
                RoadModifierInternal::FindNearestPointOnPolyline(
                    FVector(VertexPos), SmoothedLocal);

            if (Nearest.Distance > RoadHalfWidth + FalloffDistance)
            {
                return;
            }

            float RoadSurfaceZ = Nearest.NearestPoint.Z;

            FVector3d NewPos = VertexPos;
            if (Nearest.Distance <= RoadHalfWidth)
            {
                NewPos.Z = RoadSurfaceZ;
            }
            else
            {
                float Alpha = 1.0f - (Nearest.Distance - RoadHalfWidth) / FMath::Max(FalloffDistance, KINDA_SMALL_NUMBER);
                Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
                NewPos.Z = FMath::Lerp(VertexPos.Z, static_cast<double>(RoadSurfaceZ), static_cast<double>(Alpha));
            }

            InMeshView.SetVertexPos(VertexIndex, NewPos);
        });
    }

    static FGuid GetCodeVersionKey()
    {
        static FGuid VersionKey(TEXT("a7b3e1f2-4c89-4d56-b012-f9e8d7c6a543"));
        return VersionKey;
    }

    virtual bool DisableDDCWrite() const override
    {
        return false;
    }
};

// ---- UArcRoadModifier ----

UArcRoadModifier::UArcRoadModifier()
{
    SetType(FName(TEXT("Modifier")));
    SetPriority(100.0);
}

TSharedPtr<const IModifierBackgroundOp> UArcRoadModifier::CreateBackgroundOp(
    const EBuildType InBuildType) const
{
    if (StartPoint.Equals(EndPoint, 1.0f))
    {
        return nullptr;
    }

    TSharedPtr<FBackgroundOp> Op = AllocateBackgroundOp<FBackgroundOp>(GetFName());

    Op->ComponentTransform = GetComponentToWorld();
    Op->WorldBounds = ComputeBounds()[0];

    Op->StartPoint = StartPoint;
    Op->EndPoint = EndPoint;
    Op->RoadHalfWidth = RoadWidth * 0.5f;
    Op->FalloffDistance = FalloffDistance;

    Op->PathParams.MaxSlopeAngleDegrees = MaxSlopeAngleDegrees;
    Op->PathParams.SteepSlopeThresholdDegrees = SteepSlopeThresholdDegrees;
    Op->PathParams.SlopeCostMultiplier = SlopeCostMultiplier;
    Op->PathParams.CurvatureCostMultiplier = CurvatureCostMultiplier;
    Op->PathParams.UphillBias = UphillBias;

    Op->SmoothParams.Method = static_cast<ArcRoadTool::EArcSmoothingMethod>(SmoothingMethod);
    Op->SmoothParams.ChaikinIterations = ChaikinIterations;
    Op->SmoothParams.ResampleSpacing = ResampleSpacing;

    Op->SharedSmoothedPath = SharedSmoothedPath;

    return Op;
}

TArray<FBox> UArcRoadModifier::ComputeBounds() const
{
    FBox Bounds(ForceInit);
    Bounds += FVector3d(StartPoint);
    Bounds += FVector3d(EndPoint);

    float Inflation = RoadWidth * 0.5f + FalloffDistance + 1000.0f;
    Bounds = Bounds.ExpandBy(Inflation);

    return { Bounds };
}

void UArcRoadModifier::DrawVisualization(
    const FSceneView* View,
    FPrimitiveDrawInterface* PDI) const
{
    PDI->DrawPoint(StartPoint, FLinearColor::Green, 15.0f, SDPG_Foreground);
    PDI->DrawPoint(EndPoint, FLinearColor::Red, 15.0f, SDPG_Foreground);

    if (SharedSmoothedPath.IsValid() && SharedSmoothedPath->Num() >= 2)
    {
        const TArray<FVector>& Path = *SharedSmoothedPath;
        for (int32 Idx = 0; Idx < Path.Num() - 1; ++Idx)
        {
            PDI->DrawLine(Path[Idx], Path[Idx + 1], FLinearColor::Yellow, SDPG_Foreground);
        }
    }
}

FGuid UArcRoadModifier::GetCodeVersionKey() const
{
    return FBackgroundOp::GetCodeVersionKey();
}

void UArcRoadModifier::GatherDependencies(IDependencyInterface& Dependencies) const
{
    Dependencies += StartPoint;
    Dependencies += EndPoint;
    Dependencies += RoadWidth;
    Dependencies += FalloffDistance;
    Dependencies += MaxSlopeAngleDegrees;
    Dependencies += SteepSlopeThresholdDegrees;
    Dependencies += SlopeCostMultiplier;
    Dependencies += UphillBias;
    Dependencies += CurvatureCostMultiplier;
    int32 SmoothingMethodInt = static_cast<int32>(SmoothingMethod);
    Dependencies += SmoothingMethodInt;
    Dependencies += ChaikinIterations;
    Dependencies += ResampleSpacing;
}

#if WITH_EDITOR
void UArcRoadModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    TArray<FBox> Bounds = ComputeBounds();
    OnChanged(Bounds, EChangeType::StateChange);
}
#endif

void UArcRoadModifier::GenerateZoneShape()
{
    if (!SharedSmoothedPath.IsValid() || SharedSmoothedPath->Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("UArcRoadModifier: No path available. Trigger a mesh build first."));
        return;
    }

    TArray<FVector> ZonePoints = ArcRoadTool::ResamplePathUniform(*SharedSmoothedPath, 800.0f);

    if (ZonePoints.Num() < 2)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    AZoneShape* ZoneShapeActor = GeneratedZoneShape.Get();

    if (!ZoneShapeActor)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *FString::Printf(TEXT("Road_%s"), *GetFName().ToString());
        ZoneShapeActor = World->SpawnActor<AZoneShape>(
            AZoneShape::StaticClass(), FTransform::Identity, SpawnParams);

        if (!ZoneShapeActor)
        {
            return;
        }

        GeneratedZoneShape = ZoneShapeActor;
    }

    UZoneShapeComponent* ShapeComp = const_cast<UZoneShapeComponent*>(ZoneShapeActor->GetShape());
    if (!ShapeComp)
    {
        return;
    }

    FTransform ActorTransform = ZoneShapeActor->GetActorTransform();
    TArray<FZoneShapePoint>& Points = ShapeComp->GetMutablePoints();
    Points.Reset();
    Points.Reserve(ZonePoints.Num());

    for (int32 Idx = 0; Idx < ZonePoints.Num(); ++Idx)
    {
        FZoneShapePoint ShapePoint;
        ShapePoint.Position = ActorTransform.InverseTransformPosition(ZonePoints[Idx]);
        ShapePoint.Type = FZoneShapePointType::AutoBezier;

        FVector Tangent = FVector::ForwardVector;
        if (Idx < ZonePoints.Num() - 1)
        {
            Tangent = (ZonePoints[Idx + 1] - ZonePoints[Idx]).GetSafeNormal();
        }
        else if (Idx > 0)
        {
            Tangent = (ZonePoints[Idx] - ZonePoints[Idx - 1]).GetSafeNormal();
        }

        ShapePoint.SetRotationFromForwardAndUp(Tangent, FVector::UpVector);

        if (Idx < ZonePoints.Num() - 1)
        {
            ShapePoint.TangentLength = FVector::Dist(ZonePoints[Idx], ZonePoints[Idx + 1]) / 3.0f;
        }
        else if (Idx > 0)
        {
            ShapePoint.TangentLength = FVector::Dist(ZonePoints[Idx - 1], ZonePoints[Idx]) / 3.0f;
        }

        Points.Add(ShapePoint);
    }

    ShapeComp->SetCommonLaneProfile(ZoneShapeLaneProfile);
    ShapeComp->UpdateShape();

    UE_LOG(LogTemp, Log, TEXT("UArcRoadModifier: Generated ZoneShape with %d points"), ZonePoints.Num());
}
