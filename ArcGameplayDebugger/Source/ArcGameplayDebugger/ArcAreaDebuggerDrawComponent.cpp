// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaDebuggerDrawComponent.h"

void UArcAreaDebuggerDrawComponent::UpdateAreaData(const FArcAreaDebugDrawData& InData)
{
	Data = InData;
	SetDrawLabels(Data.bDrawLabels);
	RefreshShapes();
}

void UArcAreaDebuggerDrawComponent::CollectShapes()
{
	const float ZOffset = Data.ZOffset;

	for (const FArcAreaDebugDrawEntry& Entry : Data.AreaMarkers)
	{
		StoredPoints.Emplace(Entry.Location + FVector(0, 0, ZOffset), 12.f, Entry.Color);
		if (bStoredDrawLabels)
		{
			StoredTexts.Emplace(Entry.Label, Entry.Location + FVector(0, 0, ZOffset + 30.f), Entry.Color);
		}
	}

	if (Data.bHasSelectedArea)
	{
		const FVector Loc = Data.SelectedLocation;
		StoredSpheres.Emplace(50.f, Loc + FVector(0, 0, ZOffset), Data.SelectedColor);
		StoredTexts.Emplace(Data.SelectedLabel, Loc + FVector(0, 0, ZOffset + 70.f), FColor::White);
		StoredLines.Emplace(Loc, Loc + FVector(0, 0, ZOffset + 50.f), Data.SelectedColor, 2.f);
	}

	// Draw individual slot markers
	static const FColor SelectedSlotColor = FColor::Cyan;

	for (const FArcAreaDebugSlotMarker& Marker : Data.SlotMarkers)
	{
		const FVector SlotWorldPos = Marker.SlotLocation;
		const FColor DrawColor = Marker.bSelected ? SelectedSlotColor : Marker.SlotColor;

		StoredSpheres.Emplace(15.f, SlotWorldPos, DrawColor);

		if (bStoredDrawLabels)
		{
			StoredTexts.Emplace(Marker.SlotLabel, SlotWorldPos + FVector(0, 0, 25.f), DrawColor);
		}

		if (Marker.bHasEntity)
		{
			StoredLines.Emplace(SlotWorldPos, Marker.EntityLocation, Marker.LineColor, 1.5f);
		}
	}
}
