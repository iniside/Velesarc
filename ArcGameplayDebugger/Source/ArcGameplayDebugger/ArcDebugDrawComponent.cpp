// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcDebugDrawComponent.h"

#if UE_ENABLE_DEBUG_DRAWING
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "SceneManagement.h"
#endif

// ====================================================================
// Scene Proxy
// ====================================================================

#if UE_ENABLE_DEBUG_DRAWING
class FArcDebugSceneProxy final : public FDebugRenderSceneProxy
{
public:
	explicit FArcDebugSceneProxy(const UPrimitiveComponent& InComponent)
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
		return sizeof(*this) + FDebugRenderSceneProxy::GetAllocatedSize()
			+ Circles.GetAllocatedSize() + Points.GetAllocatedSize();
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

			// Draw points as cross-hairs
			for (const FArcDebugPoint& Point : Points)
			{
				const float Half = Point.Size * 0.5f;
				PDI->DrawLine(Point.Position - FVector(Half, 0, 0), Point.Position + FVector(Half, 0, 0), Point.Color, SDPG_World, 2.f);
				PDI->DrawLine(Point.Position - FVector(0, Half, 0), Point.Position + FVector(0, Half, 0), Point.Color, SDPG_World, 2.f);
				PDI->DrawLine(Point.Position - FVector(0, 0, Half), Point.Position + FVector(0, 0, Half), Point.Color, SDPG_World, 2.f);
			}
		}
	}

	TArray<FArcDebugCircle> Circles;
	TArray<FArcDebugPoint> Points;
};
#endif

// ====================================================================
// Delegate Helper
// ====================================================================

void FArcDebugDrawDelegateHelper::SetupFromProxy(const FDebugRenderSceneProxy* InSceneProxy, bool bInDrawLabels)
{
	Super::InitDelegateHelper(InSceneProxy);
	bDrawLabels = bInDrawLabels;
}

void FArcDebugDrawDelegateHelper::DrawDebugLabels(UCanvas* Canvas, APlayerController* PC)
{
	if (bDrawLabels)
	{
		Super::DrawDebugLabels(Canvas, PC);
	}
}

// ====================================================================
// Base Draw Component
// ====================================================================

UArcDebugDrawComponent::UArcDebugDrawComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsVisualizationComponent(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	bSelectable = false;
}

void UArcDebugDrawComponent::RefreshShapes()
{
	StoredSpheres.Reset();
	StoredLines.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();
	StoredBoxes.Reset();
	StoredCapsules.Reset();
	StoredCircles.Reset();
	StoredPoints.Reset();

	CollectShapes();
	UpdateBounds();
	MarkRenderStateDirty();
}

void UArcDebugDrawComponent::ClearShapes()
{
	StoredSpheres.Reset();
	StoredLines.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();
	StoredBoxes.Reset();
	StoredCapsules.Reset();
	StoredCircles.Reset();
	StoredPoints.Reset();
	MarkRenderStateDirty();
}

FBoxSphereBounds UArcDebugDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(ForceInit);

	for (const FDebugRenderSceneProxy::FSphere& Sphere : StoredSpheres)
	{
		BoundingBox += FBox::BuildAABB(Sphere.Location, FVector(Sphere.Radius));
	}
	for (const FDebugRenderSceneProxy::FDebugBox& Box : StoredBoxes)
	{
		BoundingBox += Box.Box;
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
	for (const FDebugRenderSceneProxy::FCapsule& Capsule : StoredCapsules)
	{
		FVector Extent(Capsule.Radius, Capsule.Radius, Capsule.HalfHeight + Capsule.Radius);
		BoundingBox += FBox::BuildAABB(Capsule.Base, Extent);
	}
	for (const FArcDebugCircle& Circle : StoredCircles)
	{
		BoundingBox += FBox::BuildAABB(Circle.Center, FVector(Circle.Radius));
	}
	for (const FArcDebugPoint& Point : StoredPoints)
	{
		BoundingBox += Point.Position;
	}

	if (!BoundingBox.IsValid)
	{
		BoundingBox = FBox(FVector(-5000.0f), FVector(5000.0f));
	}
	return FBoxSphereBounds(BoundingBox);
}

#if UE_ENABLE_DEBUG_DRAWING
FDebugRenderSceneProxy* UArcDebugDrawComponent::CreateDebugSceneProxy()
{
	FArcDebugSceneProxy* Proxy = new FArcDebugSceneProxy(*this);

	Proxy->Spheres = StoredSpheres;
	Proxy->Lines = StoredLines;
	Proxy->Texts = StoredTexts;
	Proxy->ArrowLines = StoredArrows;
	Proxy->Boxes = StoredBoxes;
	Proxy->Capsules = StoredCapsules;
	Proxy->Circles = StoredCircles;
	Proxy->Points = StoredPoints;

	DebugDrawDelegateHelper.Reset();
	DebugDrawDelegateHelper.SetupFromProxy(Proxy, bStoredDrawLabels);

	return Proxy;
}
#endif
