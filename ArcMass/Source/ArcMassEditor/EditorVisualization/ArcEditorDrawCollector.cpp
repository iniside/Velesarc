// Copyright Lukasz Baran. All Rights Reserved.

#include "EditorVisualization/ArcEditorDrawCollector.h"
#include "DebugDraw.h"
#include "Engine/World.h"

void FArcEditorDrawCollector::Submit(const UWorld* World) const
{
	const FSceneInterface* Scene = UE::DebugDraw::GetSceneKey(World);

	for (const FLine& Line : Lines)
	{
		UE::DebugDraw::Line(Scene, Line.Start, Line.End, Line.Color, Line.Thickness);
	}

	for (const FSphere& Sphere : Spheres)
	{
		// Segments is clamped internally by DebugDraw (UE.DebugDraw.SphereMaxSides).
		UE::DebugDraw::Sphere(Scene, Sphere.Center, Sphere.Radius, 16, Sphere.Color);
	}

	for (const FCircle& Circle : Circles)
	{
		// DebugDraw::Circle is parameterised by the plane normal; the collector stores the two
		// in-plane axes, so the normal is their cross product.
		const FVector UpAxis = (Circle.YAxis ^ Circle.ZAxis).GetSafeNormal();
		UE::DebugDraw::Circle(Scene, Circle.Center, UpAxis, Circle.Radius, Circle.Segments, Circle.Color, Circle.Thickness);
	}

	for (const FArrow& Arrow : Arrows)
	{
		UE::DebugDraw::Arrow(Scene, Arrow.Start, Arrow.End, Arrow.ArrowSize, Arrow.Color, Arrow.Thickness);
	}

	for (const FText& Text : Texts)
	{
		UE::DebugDraw::Text(Scene, Text.Location, Text.Text, Text.Color);
	}

	for (const FBox& Box : Boxes)
	{
		UE::DebugDraw::Box(Scene, Box.Center, Box.Extent, Box.Rotation, Box.Color, Box.Thickness);
	}

	for (const FCapsule& Capsule : Capsules)
	{
		UE::DebugDraw::Capsule(Scene, Capsule.Center, Capsule.HalfHeight, Capsule.Radius, Capsule.Rotation, Capsule.Color, Capsule.Thickness);
	}
}
