// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuildRequirement.h"

#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"

bool FArcBuildRequirement_NearbyStation::CanPlace(const FTransform& InTransform, const FArcBuildRequirementContext& InContext, const UArcBuildingDefinition* InBuildDef) const
{
	if (InContext.World == nullptr)
	{
		return false;
	}

	UArcMassSpatialHashSubsystem* SpatialHash = InContext.World->GetSubsystem<UArcMassSpatialHashSubsystem>();
	if (SpatialHash == nullptr)
	{
		return false;
	}

	TArray<FArcMassEntityInfo> OutEntities;
	SpatialHash->QuerySphere(RequiredStationTag, InTransform.GetLocation(), Radius, OutEntities);

	return OutEntities.Num() > 0;
}
