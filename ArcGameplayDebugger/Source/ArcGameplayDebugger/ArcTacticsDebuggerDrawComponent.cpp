// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsDebuggerDrawComponent.h"

namespace ArcTacticsDebuggerDrawComponentPrivate
{
	constexpr float EntityLabelOffset = 40.0f;
	constexpr float PointLabelOffset = 20.0f;
}

void UArcTacticsDebuggerDrawComponent::SetShapes(const FArcTacticsDebugDrawData& InData)
{
	Data = InData;
	if (Data.IsEmpty())
	{
		ClearShapes();
		return;
	}
	RefreshShapes();
}

void UArcTacticsDebuggerDrawComponent::CollectShapes()
{
	using namespace ArcTacticsDebuggerDrawComponentPrivate;

	for (const FArcTacticsDebugDrawEntity& Entity : Data.Entities)
	{
		StoredSpheres.Emplace(Entity.Radius, Entity.Location, Entity.Color);
		if (!Entity.Label.IsEmpty())
		{
			StoredTexts.Emplace(Entity.Label, Entity.Location + FVector(0.0f, 0.0f, Entity.Radius + EntityLabelOffset), Entity.Color);
		}
	}

	for (const FArcTacticsDebugDrawLine& Line : Data.Lines)
	{
		StoredLines.Emplace(Line.Start, Line.End, Line.Color, Line.Thickness);
	}

	for (const FArcTacticsDebugDrawPolyline& Polyline : Data.Polylines)
	{
		for (int32 PointIndex = 1; PointIndex < Polyline.Points.Num(); ++PointIndex)
		{
			StoredLines.Emplace(Polyline.Points[PointIndex - 1], Polyline.Points[PointIndex], Polyline.Color, Polyline.Thickness);
		}
	}

	for (const FArcTacticsDebugDrawPoint& Point : Data.Points)
	{
		StoredPoints.Emplace(Point.Location, Point.Size, Point.Color);
		if (!Point.Label.IsEmpty())
		{
			StoredTexts.Emplace(Point.Label, Point.Location + FVector(0.0f, 0.0f, Point.Size * 0.5f + PointLabelOffset), Point.Color);
		}
	}
}
