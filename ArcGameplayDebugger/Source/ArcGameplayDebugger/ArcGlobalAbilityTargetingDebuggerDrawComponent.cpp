// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGlobalAbilityTargetingDebuggerDrawComponent.h"

void UArcGlobalAbilityTargetingDebuggerDrawComponent::UpdateTargetingSpheres(TArrayView<const FArcTargetingDebugSphere> InSpheres)
{
	TargetingSpheres.Reset();
	TargetingSpheres.Append(InSpheres.GetData(), InSpheres.Num());
	SetDrawLabels(false);
	RefreshShapes();
}

void UArcGlobalAbilityTargetingDebuggerDrawComponent::CollectShapes()
{
	for (const FArcTargetingDebugSphere& Entry : TargetingSpheres)
	{
		StoredSpheres.Emplace(Entry.Radius, Entry.Location, Entry.Color);
	}
}
