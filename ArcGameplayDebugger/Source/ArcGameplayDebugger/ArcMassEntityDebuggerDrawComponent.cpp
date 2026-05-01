// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityDebuggerDrawComponent.h"

void UArcMassEntityDebuggerDrawComponent::UpdateSelectedEntity(const FVector& InEntityLocation, float InRadius, const FVector* InPawnLocation)
{
	EntityLocation = InEntityLocation;
	SphereRadius = InRadius;
	bHasEntity = true;
	bHasPawnLine = (InPawnLocation != nullptr);
	if (InPawnLocation)
	{
		PawnLocation = *InPawnLocation;
	}
	RefreshShapes();
}

void UArcMassEntityDebuggerDrawComponent::ClearSelectedEntity()
{
	bHasEntity = false;
	bHasPawnLine = false;
	ClearShapes();
}

void UArcMassEntityDebuggerDrawComponent::CollectShapes()
{
	if (!bHasEntity)
	{
		return;
	}
	StoredSpheres.Emplace(SphereRadius, EntityLocation, FColor::Yellow);
	if (bHasPawnLine)
	{
		StoredLines.Emplace(PawnLocation, EntityLocation, FColor::Cyan);
	}
}
