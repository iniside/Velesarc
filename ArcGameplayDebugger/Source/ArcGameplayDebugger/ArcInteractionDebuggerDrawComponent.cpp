// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInteractionDebuggerDrawComponent.h"

void UArcInteractionDebuggerDrawComponent::UpdateInteractionTarget(const FVector& InOrigin, float InRadius)
{
	TargetOrigin = InOrigin;
	TargetRadius = InRadius;
	bHasTarget = true;
	RefreshShapes();
}

void UArcInteractionDebuggerDrawComponent::CollectShapes()
{
	if (!bHasTarget)
	{
		return;
	}
	StoredSpheres.Emplace(TargetRadius, TargetOrigin, FColor::Green);
}
