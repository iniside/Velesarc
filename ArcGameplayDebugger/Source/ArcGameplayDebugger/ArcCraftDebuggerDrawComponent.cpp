// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCraftDebuggerDrawComponent.h"

void UArcCraftDebuggerDrawComponent::UpdateData(const FArcCraftDebugDrawData& InData)
{
	Data = InData;
	RefreshShapes();
}

void UArcCraftDebuggerDrawComponent::CollectShapes()
{
	if (!Data.bHasSelectedStation)
	{
		return;
	}

	// Circle around selected station
	StoredCircles.Add(FArcDebugCircle(
		Data.SelectedStationLocation + FVector(0, 0, 50.f),
		200.f, 32,
		FVector::XAxisVector, FVector::YAxisVector,
		FColor::Cyan, 3.0f));

	// Line from player to station
	StoredLines.Add(FDebugRenderSceneProxy::FDebugLine(
		Data.PlayerLocation + FVector(0, 0, 100.f),
		Data.SelectedStationLocation + FVector(0, 0, 100.f),
		FColor::Cyan));
}
