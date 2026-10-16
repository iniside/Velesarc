// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityManager.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"

namespace ArcSmartObjectPlanSensor
{
	/** Sphere query on UArcMassSpatialHashSubsystem with a caller-supplied predicate.
	 *  Both the spatial-hash sensor and the item sensor delegate here, avoiding
	 *  duplicated QueryEntitiesInRadiusFiltered boilerplate.
	 *
	 *  Predicate signature: bool(const FMassEntityManager&, FMassEntityHandle) */
	template <typename TFilterPredicate>
	void QueryNearbyFiltered(
		const FMassEntityManager& EntityManager,
		const UArcMassSpatialHashSubsystem& SpatialHash,
		const FVector& Origin,
		float Radius,
		TFilterPredicate FilterPredicate,
		TArray<FArcMassEntityInfo>& OutEntities)
	{
		SpatialHash.GetSpatialHashGrid().QueryEntitiesInRadiusFiltered(
			Origin,
			Radius,
			[&EntityManager, &FilterPredicate](const FMassEntityHandle& Entity, const FVector& /*Location*/) -> bool
			{
				return FilterPredicate(EntityManager, Entity);
			},
			OutEntities);
	}

} // namespace ArcSmartObjectPlanSensor
