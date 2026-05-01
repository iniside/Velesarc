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
	// Settlement radius circles
	for (const FArcEconomyDebugDrawData::FSettlementMarker& Marker : Data.SettlementMarkers)
	{
		float Thickness = Marker.bSelected ? 3.0f : 1.0f;
		FColor CircleColor = Marker.bSelected ? FColor::White : Marker.Color;

		StoredCircles.Add(FArcDebugCircle(
			Marker.Location + FVector(0, 0, 100.f),
			Marker.Radius,
			64,
			FVector::XAxisVector,
			FVector::YAxisVector,
			CircleColor,
			Thickness));

		if (Data.bDrawLabels)
		{
			StoredTexts.Add(FDebugRenderSceneProxy::FText3d(
				Marker.Label,
				Marker.Location + FVector(0, 0, 300.f),
				Marker.Color));
		}
	}

	// Building markers
	static const FColor BuildingColor(0, 200, 200);
	for (const FArcEconomyDebugDrawData::FBuildingMarker& Marker : Data.BuildingMarkers)
	{
		StoredPoints.Add(FArcDebugPoint(Marker.Location + FVector(0, 0, 50.f), 15.f, BuildingColor));

		if (Data.bDrawLabels)
		{
			StoredTexts.Add(FDebugRenderSceneProxy::FText3d(
				Marker.Label,
				Marker.Location + FVector(0, 0, 200.f),
				BuildingColor));
		}
	}

	// NPC markers
	for (const FArcEconomyDebugDrawData::FNPCMarker& Marker : Data.NPCMarkers)
	{
		StoredPoints.Add(FArcDebugPoint(Marker.Location + FVector(0, 0, 50.f), 8.f, Marker.Color));
	}

	// Links (worker->building, transporter->target)
	for (const FArcEconomyDebugDrawData::FLink& Link : Data.Links)
	{
		StoredLines.Add(FDebugRenderSceneProxy::FDebugLine(
			Link.Start + FVector(0, 0, 50.f),
			Link.End + FVector(0, 0, 50.f),
			Link.Color));
	}

	// SO slot markers — circles at slot world positions
	for (const FArcEconomyDebugDrawData::FSOSlotMarker& Marker : Data.SOSlotMarkers)
	{
		StoredCircles.Add(FArcDebugCircle(
			Marker.Location + FVector(0, 0, 30.f),
			75.f, 16,
			FVector::XAxisVector, FVector::YAxisVector,
			Marker.Color, 2.0f));

		if (Data.bDrawLabels && !Marker.Label.IsEmpty())
		{
			StoredTexts.Add(FDebugRenderSceneProxy::FText3d(
				Marker.Label,
				Marker.Location + FVector(0, 0, 120.f),
				Marker.Color));
		}
	}

	// Resource markers
	for (const FArcEconomyDebugDrawData::FResourceMarker& Marker : Data.ResourceMarkers)
	{
		StoredCircles.Add(FArcDebugCircle(
			Marker.Location + FVector(0, 0, 30.f),
			100.f, 8,
			FVector::XAxisVector, FVector::YAxisVector,
			Marker.Color, 2.0f));

		if (Data.bDrawLabels && !Marker.Label.IsEmpty())
		{
			StoredTexts.Add(FDebugRenderSceneProxy::FText3d(
				Marker.Label,
				Marker.Location + FVector(0, 0, 150.f),
				Marker.Color));
		}
	}

	// Selected building highlight
	if (Data.bHasSelectedBuilding)
	{
		StoredCircles.Add(FArcDebugCircle(
			Data.SelectedBuildingLocation + FVector(0, 0, 50.f),
			200.f, 32,
			FVector::XAxisVector, FVector::YAxisVector,
			FColor::Cyan, 3.0f));

		if (Data.SelectedBuildingSearchRadius > 0.f)
		{
			StoredCircles.Add(FArcDebugCircle(
				Data.SelectedBuildingLocation + FVector(0, 0, 50.f),
				Data.SelectedBuildingSearchRadius,
				48,
				FVector::XAxisVector, FVector::YAxisVector,
				FColor(100, 200, 255, 128), 1.0f));
		}

		StoredLines.Add(FDebugRenderSceneProxy::FDebugLine(
			Data.PlayerLocation + FVector(0, 0, 100.f),
			Data.SelectedBuildingLocation + FVector(0, 0, 100.f),
			FColor::Cyan));
	}

	// Selected NPC highlight
	if (Data.bHasSelectedNPC)
	{
		StoredCircles.Add(FArcDebugCircle(
			Data.SelectedNPCLocation + FVector(0, 0, 50.f),
			100.f, 24,
			FVector::XAxisVector, FVector::YAxisVector,
			FColor::Magenta, 3.0f));

		StoredLines.Add(FDebugRenderSceneProxy::FDebugLine(
			Data.PlayerLocation + FVector(0, 0, 100.f),
			Data.SelectedNPCLocation + FVector(0, 0, 100.f),
			FColor::Magenta));
	}
}
