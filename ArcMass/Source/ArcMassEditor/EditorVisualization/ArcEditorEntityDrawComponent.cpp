// ArcEditorEntityDrawComponent.cpp
#include "EditorVisualization/ArcEditorEntityDrawComponent.h"
#include "PrimitiveDrawingUtils.h"

UArcEditorEntityDrawComponent::UArcEditorEntityDrawComponent()
{
	SetIsVisualizationComponent(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCanEverAffectNavigation(false);
	SetGenerateOverlapEvents(false);
	bSelectable = false;
	bIsEditorOnly = true;
}

void UArcEditorEntityDrawComponent::ClearShapes()
{
	StoredLines.Reset();
	StoredSpheres.Reset();
	StoredTexts.Reset();
	StoredArrows.Reset();
	StoredCircles.Reset();
}

void UArcEditorEntityDrawComponent::AddLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness)
{
	StoredLines.Emplace(Start, End, Color, Thickness);
}

void UArcEditorEntityDrawComponent::AddSphere(const FVector& Center, float Radius, const FColor& Color)
{
	StoredSpheres.Emplace(Radius, Center, Color);
}

void UArcEditorEntityDrawComponent::AddCircle(const FVector& Center, float Radius, const FVector& YAxis, const FVector& ZAxis, const FColor& Color, float Thickness, int32 Segments)
{
	FArcEditorCircle Circle;
	Circle.Center = Center;
	Circle.Radius = Radius;
	Circle.YAxis = YAxis;
	Circle.ZAxis = ZAxis;
	Circle.Color = Color;
	Circle.Thickness = Thickness;
	Circle.Segments = Segments;
	StoredCircles.Add(Circle);
}

void UArcEditorEntityDrawComponent::AddText(const FVector& Location, const FString& Text, const FColor& Color)
{
	StoredTexts.Emplace(Text, Location, FLinearColor(Color));
}

void UArcEditorEntityDrawComponent::AddArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize, float Thickness)
{
	FArcEditorArrow Arrow;
	Arrow.Start = Start;
	Arrow.End = End;
	Arrow.Color = Color;
	Arrow.ArrowSize = ArrowSize;
	Arrow.Thickness = Thickness;
	StoredArrows.Add(Arrow);
}

void UArcEditorEntityDrawComponent::FinishCollecting()
{
	UpdateBounds();
	MarkRenderStateDirty();
}

FBoxSphereBounds UArcEditorEntityDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
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
	for (const FArcEditorArrow& Arrow : StoredArrows)
	{
		BoundingBox += Arrow.Start;
		BoundingBox += Arrow.End;
	}
	for (const FDebugRenderSceneProxy::FText3d& Text : StoredTexts)
	{
		BoundingBox += Text.Location;
	}
	for (const FArcEditorCircle& Circle : StoredCircles)
	{
		BoundingBox += FBox::BuildAABB(Circle.Center, FVector(Circle.Radius));
	}

	if (!BoundingBox.IsValid)
	{
		BoundingBox = FBox(FVector(-5000.0f), FVector(5000.0f));
	}
	return FBoxSphereBounds(BoundingBox);
}

#if UE_ENABLE_DEBUG_DRAWING

namespace ArcEditorEntityDraw
{

class FArcEditorEntitySceneProxy final : public FDebugRenderSceneProxy
{
public:
	explicit FArcEditorEntitySceneProxy(const UPrimitiveComponent* InComponent)
		: FDebugRenderSceneProxy(InComponent)
	{
		DrawType = SolidAndWireMeshes;
		TextWithoutShadowDistance = 1500.0f;

		// Default ViewFlagName is "Game" which is off in editor viewports.
		// Use "StaticMeshes" so the text delegate fires in the level editor.
		ViewFlagName = TEXT("StaticMeshes");
		ViewFlagIndex = uint32(FEngineShowFlags::FindIndexByName(*ViewFlagName));
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
			+ Circles.GetAllocatedSize()
			+ Arrows.GetAllocatedSize();
	}

	TArray<FArcEditorCircle> Circles;
	TArray<FArcEditorArrow> Arrows;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override
	{
		FDebugRenderSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
			{
				continue;
			}

			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			for (const FArcEditorCircle& Circle : Circles)
			{
				const float AngleStep = 2.f * UE_PI / Circle.Segments;
				FVector PrevPoint = Circle.Center + Circle.YAxis * Circle.Radius;

				for (int32 i = 1; i <= Circle.Segments; ++i)
				{
					const float Angle = AngleStep * i;
					const FVector NextPoint = Circle.Center
						+ Circle.YAxis * (Circle.Radius * FMath::Cos(Angle))
						+ Circle.ZAxis * (Circle.Radius * FMath::Sin(Angle));

					PDI->DrawLine(PrevPoint, NextPoint, Circle.Color, SDPG_World, Circle.Thickness);
					PrevPoint = NextPoint;
				}
			}

			for (const FArcEditorArrow& Arrow : Arrows)
			{
				FVector Dir = Arrow.End - Arrow.Start;
				const float DirMag = Dir.Size();
				if (DirMag < KINDA_SMALL_NUMBER)
				{
					continue;
				}
				Dir /= DirMag;
				FVector YAxis, ZAxis;
				Dir.FindBestAxisVectors(YAxis, ZAxis);
				const FMatrix ArrowTM(Dir, YAxis, ZAxis, Arrow.Start);
				DrawDirectionalArrow(PDI, ArrowTM, Arrow.Color, DirMag, Arrow.ArrowSize, SDPG_World, Arrow.Thickness);
			}
		}
	}
};

} // namespace ArcEditorEntityDraw

FDebugRenderSceneProxy* UArcEditorEntityDrawComponent::CreateDebugSceneProxy()
{
	ArcEditorEntityDraw::FArcEditorEntitySceneProxy* Proxy =
		new ArcEditorEntityDraw::FArcEditorEntitySceneProxy(this);

	Proxy->Lines = StoredLines;
	Proxy->Spheres = StoredSpheres;
	Proxy->Texts = StoredTexts;
	Proxy->Arrows = StoredArrows;
	Proxy->Circles = StoredCircles;

	DebugDrawDelegateHelper.Reset();
	DebugDrawDelegateHelper.SetupFromProxy(Proxy);

	return Proxy;
}

#endif // UE_ENABLE_DEBUG_DRAWING
