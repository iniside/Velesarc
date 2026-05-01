// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPathDebuggerDrawComponent.h"
#include "ArcDebugDrawComponent.h" // FArcDebugCircle
#include "DynamicMeshBuilder.h" // FDynamicMeshVertex

#if UE_ENABLE_DEBUG_DRAWING
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialRenderProxy.h"
#include "SceneManagement.h"
#endif

// ====================================================================
// Scene Proxy
// ====================================================================

#if UE_ENABLE_DEBUG_DRAWING
class FArcPathDebuggerSceneProxy final : public FDebugRenderSceneProxy
{
public:
	explicit FArcPathDebuggerSceneProxy(const UPrimitiveComponent& InComponent)
		: FDebugRenderSceneProxy(&InComponent)
	{
		DrawType = SolidAndWireMeshes;
		TextWithoutShadowDistance = 1500.0f;
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = true;
		Result.bDynamicRelevance = true;
		Result.bSeparateTranslucency = Result.bNormalTranslucency = true;
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		uint32 Size = sizeof(*this) + FDebugRenderSceneProxy::GetAllocatedSize()
			+ Circles.GetAllocatedSize() + NavMeshPolys.GetAllocatedSize();
		for (const FArcPathDebugDrawData::FNavMeshPoly& Poly : NavMeshPolys)
		{
			Size += Poly.Verts.GetAllocatedSize();
		}
		return Size;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		FDebugRenderSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
			{
				continue;
			}

			const FSceneView* View = Views[ViewIndex];
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			// Draw circles as line segments
			for (const FArcDebugCircle& Circle : Circles)
			{
				const float AngleStep = 2.f * PI / static_cast<float>(Circle.Segments);
				FVector PrevPoint = Circle.Center + Circle.YAxis * Circle.Radius;

				for (int32 i = 1; i <= Circle.Segments; ++i)
				{
					const float Angle = AngleStep * static_cast<float>(i);
					FVector NextPoint = Circle.Center
						+ Circle.YAxis * (FMath::Cos(Angle) * Circle.Radius)
						+ Circle.ZAxis * (FMath::Sin(Angle) * Circle.Radius);
					PDI->DrawLine(PrevPoint, NextPoint, Circle.Color, SDPG_World, Circle.Thickness);
					PrevPoint = NextPoint;
				}
			}

			// Draw navmesh polygon outlines
			for (const FArcPathDebugDrawData::FNavMeshPoly& Poly : NavMeshPolys)
			{
				const int32 NumVerts = Poly.Verts.Num();
				for (int32 i = 0; i < NumVerts; ++i)
				{
					PDI->DrawLine(Poly.Verts[i], Poly.Verts[(i + 1) % NumVerts], NavMeshOutlineColor, SDPG_World, 1.f);
				}
			}

			// Draw navmesh fill — single batched mesh with backface culling disabled
			if (NavMeshFillVerts.Num() > 0)
			{
				FMaterialRenderProxy* FillMat = &Collector.AllocateOneFrameResource<FColoredMaterialRenderProxy>(
					GEngine->DebugMeshMaterial->GetRenderProxy(), FLinearColor(NavMeshFillColor));

				FDynamicMeshBuilder MeshBuilder(View->GetFeatureLevel());
				MeshBuilder.AddVertices(NavMeshFillVerts);
				MeshBuilder.AddTriangles(NavMeshFillIndices);
				MeshBuilder.GetMesh(FMatrix::Identity, FillMat, SDPG_World, /*bDisableBackfaceCulling=*/ true, false, ViewIndex, Collector);
			}
		}
	}

	TArray<FArcDebugCircle> Circles;
	TArray<FArcPathDebugDrawData::FNavMeshPoly> NavMeshPolys;
	FColor NavMeshOutlineColor = FColor(0, 180, 120, 255);
	FColor NavMeshFillColor = FColor(0, 180, 120, 120);
	TArray<FDynamicMeshVertex> NavMeshFillVerts;
	TArray<uint32> NavMeshFillIndices;
};
#endif

// ====================================================================
// Delegate Helper
// ====================================================================

void FArcPathDebugDrawDelegateHelper::SetupFromProxy(const FDebugRenderSceneProxy* InSceneProxy)
{
	Super::InitDelegateHelper(InSceneProxy);
}

void FArcPathDebugDrawDelegateHelper::DrawDebugLabels(UCanvas* Canvas, APlayerController* PC)
{
	Super::DrawDebugLabels(Canvas, PC);
}

// ====================================================================
// Draw Component
// ====================================================================

UArcPathDebuggerDrawComponent::UArcPathDebuggerDrawComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsVisualizationComponent(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	bSelectable = false;
}

void UArcPathDebuggerDrawComponent::UpdatePathData(const FArcPathDebugDrawData& InData)
{
	Data = InData;

	StoredSpheres.Reset();
	StoredLines.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();

	CollectShapes();
	UpdateBounds();
	MarkRenderStateDirty();
}

void UArcPathDebuggerDrawComponent::ClearShapes()
{
	Data = FArcPathDebugDrawData();
	StoredSpheres.Reset();
	StoredLines.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();
	MarkRenderStateDirty();
}

FBoxSphereBounds UArcPathDebuggerDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(ForceInit);

	for (const FDebugRenderSceneProxy::FSphere& Sphere : StoredSpheres)
	{
		BoundingBox += FBox::BuildAABB(Sphere.Location, FVector(Sphere.Radius));
	}
	for (const FDebugRenderSceneProxy::FDebugLine& Line : StoredLines)
	{
		BoundingBox += Line.Start;
		BoundingBox += Line.End;
	}
	for (const FDebugRenderSceneProxy::FArrowLine& Arrow : StoredArrows)
	{
		BoundingBox += Arrow.Start;
		BoundingBox += Arrow.End;
	}
	for (const FDebugRenderSceneProxy::FText3d& Text : StoredTexts)
	{
		BoundingBox += Text.Location;
	}
	for (const FArcPathDebugDrawData::FNavMeshPoly& Poly : Data.NavMeshPolys)
	{
		for (const FVector& Vert : Poly.Verts)
		{
			BoundingBox += Vert;
		}
	}

	if (!BoundingBox.IsValid)
	{
		BoundingBox = FBox(FVector(-5000.0f), FVector(5000.0f));
	}
	return FBoxSphereBounds(BoundingBox);
}

#if UE_ENABLE_DEBUG_DRAWING
FDebugRenderSceneProxy* UArcPathDebuggerDrawComponent::CreateDebugSceneProxy()
{
	FArcPathDebuggerSceneProxy* Proxy = new FArcPathDebuggerSceneProxy(*this);

	Proxy->Spheres = StoredSpheres;
	Proxy->Lines = StoredLines;
	Proxy->Texts = StoredTexts;
	Proxy->ArrowLines = StoredArrows;

	// Circles for avoidance collider and slack radius
	if (Data.bDrawMoveTarget && Data.MoveTargetSlackRadius > 0.f)
	{
		Proxy->Circles.Emplace(Data.MoveTargetPos, Data.MoveTargetSlackRadius, 24, FVector(0, 1, 0), FVector(1, 0, 0), FColor(255, 0, 200, 80), 1.f);
	}

	if (Data.bDrawAvoidance)
	{
		const FVector CircleYAxis(0, 1, 0);
		const FVector CircleZAxis(1, 0, 0);
		const FVector BasePos = Data.BasePos;
		if (Data.ColliderShape == FArcPathDebugDrawData::EColliderShape::Circle)
		{
			Proxy->Circles.Emplace(BasePos + FVector(0, 0, 1), Data.ColliderRadius, 24, CircleYAxis, CircleZAxis, FColor::Blue, 1.f);
		}
		else
		{
			Proxy->Circles.Emplace(BasePos + Data.EntityForward * Data.PillHalfLength + FVector(0, 0, 1), Data.ColliderRadius, 24, CircleYAxis, CircleZAxis, FColor::Blue, 1.f);
			Proxy->Circles.Emplace(BasePos - Data.EntityForward * Data.PillHalfLength + FVector(0, 0, 1), Data.ColliderRadius, 24, CircleYAxis, CircleZAxis, FColor::Blue, 1.f);
		}
	}

	// NavMesh polys — outlines via PDI, fill via FDynamicMeshBuilder (backface culling disabled)
	if (Data.bDrawNavMesh && Data.NavMeshPolys.Num() > 0)
	{
		Proxy->NavMeshPolys = Data.NavMeshPolys;

		uint32 VertIndex = 0;
		for (const FArcPathDebugDrawData::FNavMeshPoly& Poly : Data.NavMeshPolys)
		{
			const int32 NumVerts = Poly.Verts.Num();
			if (NumVerts < 3)
			{
				continue;
			}

			const uint32 BaseVert = VertIndex;
			for (int32 i = 0; i < NumVerts; ++i)
			{
				FDynamicMeshVertex Vert;
				Vert.Position = FVector3f(Poly.Verts[i]);
				Vert.TangentZ = FVector3f(0.f, 0.f, 1.f);
				Vert.Color = Proxy->NavMeshFillColor;
				Proxy->NavMeshFillVerts.Add(Vert);
				++VertIndex;
			}

			// Fan triangulate convex poly
			for (int32 i = 1; i + 1 < NumVerts; ++i)
			{
				Proxy->NavMeshFillIndices.Add(BaseVert);
				Proxy->NavMeshFillIndices.Add(BaseVert + i);
				Proxy->NavMeshFillIndices.Add(BaseVert + i + 1);
			}
		}
	}

	DebugDrawDelegateHelper.Reset();
	DebugDrawDelegateHelper.SetupFromProxy(Proxy);

	return Proxy;
}
#endif

void UArcPathDebuggerDrawComponent::CollectShapes()
{
	const FVector BasePos = Data.BasePos;

	StoredSpheres.Emplace(25.f, BasePos, FColor::Cyan);
	StoredArrows.Emplace(BasePos, BasePos + Data.EntityForward * 80.f, FColor::Cyan);

	if (Data.bDrawMoveTarget)
	{
		const FVector TargetPos = Data.MoveTargetPos;
		const FColor Magenta(255, 0, 200);
		StoredSpheres.Emplace(20.f, TargetPos, Magenta);
		StoredArrows.Emplace(TargetPos, TargetPos + Data.MoveTargetForward * 60.f, Magenta);
		StoredLines.Emplace(BasePos, TargetPos, FColor(255, 140, 0), 2.f);
		StoredTexts.Emplace(Data.MoveTargetLabel, TargetPos + FVector(0, 0, 30.f), FColor::White);
	}

	if (Data.bDrawVelocity)
	{
		StoredArrows.Emplace(BasePos + FVector(0, 0, 2), BasePos + FVector(0, 0, 2) + Data.Velocity, FColor::Yellow);
	}

	if (Data.bDrawSteering)
	{
		StoredArrows.Emplace(BasePos + FVector(0, 0, 4), BasePos + FVector(0, 0, 4) + Data.DesiredVelocity, FColor(255, 105, 180));
		if (Data.bHasStandTarget)
		{
			StoredSpheres.Emplace(15.f, Data.StandTarget, FColor::Orange);
		}
	}

	if (Data.bDrawForces)
	{
		StoredArrows.Emplace(BasePos + FVector(0, 0, 6), BasePos + FVector(0, 0, 6) + Data.ForceValue, FColor::Red);
	}

	if (Data.bHasGhost)
	{
		const FColor GhostColor(180, 180, 180);
		StoredSpheres.Emplace(15.f, Data.GhostPos, GhostColor);
		if (!Data.GhostVelocity.IsNearlyZero())
		{
			StoredArrows.Emplace(Data.GhostPos, Data.GhostPos + Data.GhostVelocity, GhostColor);
		}
	}

	// Avoidance circles and navmesh polys are passed directly to the scene proxy in CreateDebugSceneProxy

	if (Data.bDrawShortPath)
	{
		const FColor PathColor = FColor::Green;
		const FColor CorridorColor(100, 100, 100);
		const float ZOffset = Data.PathZOffset;

		for (int32 i = 0; i < Data.PathPoints.Num(); ++i)
		{
			const FArcPathDebugNavPoint& Point = Data.PathPoints[i];
			const FVector PointPos = Point.Position;

			if (i + 1 < Data.PathPoints.Num())
			{
				const FVector NextPos = Data.PathPoints[i + 1].Position;
				StoredLines.Emplace(PointPos, NextPos, PathColor, 3.f);
				StoredLines.Emplace(Point.Left + FVector(0, 0, ZOffset), Data.PathPoints[i + 1].Left + FVector(0, 0, ZOffset), CorridorColor, 1.5f);
				StoredLines.Emplace(Point.Right + FVector(0, 0, ZOffset), Data.PathPoints[i + 1].Right + FVector(0, 0, ZOffset), CorridorColor, 1.5f);
			}

			StoredSpheres.Emplace(10.f, PointPos, PathColor);
			if (!Point.Tangent.IsNearlyZero())
			{
				StoredArrows.Emplace(PointPos, PointPos + Point.Tangent * 50.f, FColor(200, 200, 200));
			}
			if (!Point.Label.IsEmpty())
			{
				StoredTexts.Emplace(Point.Label, PointPos + FVector(0, 0, 15.f), FColor::White);
			}
			StoredLines.Emplace(Point.Left + FVector(0, 0, ZOffset), Point.Right + FVector(0, 0, ZOffset), CorridorColor, 1.f);
		}
	}
}
