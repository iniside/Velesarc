// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebuggerDrawComponent.h"

void UArcEconomyDebuggerDrawComponent::UpdateData(const FArcEconomyDebugDrawData& InData)
{
	Data = InData;
	SetDrawLabels(Data.bDrawLabels);
	RefreshShapes();
}

void UArcEconomyDebuggerDrawComponent::CollectShapes()
{
	static const FColor BuildingColor(0, 200, 200);
	for (const FArcEconomyDebugDrawData::FBuildingMarker& Marker : Data.BuildingMarkers)
	{
		StoredPoints.Add(FArcDebugPoint(Marker.Location + FVector(0, 0, 50.f), Marker.bSelected ? 25.f : 15.f, BuildingColor));

		if (Marker.bSelected)
		{
			StoredCircles.Add(FArcDebugCircle(
				Marker.Location + FVector(0, 0, 50.f),
				200.f, 32,
				FVector::XAxisVector, FVector::YAxisVector,
				FColor::Cyan, 3.0f));
		}

		if (Data.bDrawLabels)
		{
			StoredTexts.Add(FDebugRenderSceneProxy::FText3d(
				Marker.Label,
				Marker.Location + FVector(0, 0, 200.f),
				BuildingColor));
		}
	}

	for (const FArcEconomyDebugDrawData::FNPCMarker& Marker : Data.NPCMarkers)
	{
		StoredPoints.Add(FArcDebugPoint(Marker.Location + FVector(0, 0, 50.f), Marker.bSelected ? 16.f : 8.f, Marker.Color));

		if (Marker.bSelected)
		{
			StoredCircles.Add(FArcDebugCircle(
				Marker.Location + FVector(0, 0, 50.f),
				100.f, 24,
				FVector::XAxisVector, FVector::YAxisVector,
				FColor::Magenta, 3.0f));
		}
	}

	for (const FArcEconomyDebugDrawData::FLink& Link : Data.Links)
	{
		StoredLines.Add(FDebugRenderSceneProxy::FDebugLine(
			Link.Start + FVector(0, 0, 50.f),
			Link.End + FVector(0, 0, 50.f),
			Link.Color));
	}
}
