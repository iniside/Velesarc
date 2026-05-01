// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWMinimapDebuggerDrawComponent.h"

void UArcIWMinimapDebuggerDrawComponent::UpdateEntities(TArrayView<const FArcIWMinimapDebugDrawEntry> InEntries)
{
	Entries.Reset();
	Entries.Append(InEntries.GetData(), InEntries.Num());
	SetDrawLabels(false);
	RefreshShapes();
}

void UArcIWMinimapDebuggerDrawComponent::CollectShapes()
{
	for (const FArcIWMinimapDebugDrawEntry& Entry : Entries)
	{
		StoredPoints.Emplace(Entry.Position, 8.0f, Entry.Color);
		StoredLines.Emplace(Entry.Position, Entry.Position + FVector(0, 0, 100.0f), Entry.Color, 1.0f);
	}
}
