// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSContextProvider_PerceivedEntities.h"
#include "Perception/ArcMassSightPerception.h"
#include "Perception/ArcMassHearingPerception.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"

void FArcTQSContextProvider_PerceivedEntities::GenerateContextLocations(
	const FArcTQSQueryContext& QueryContext,
	TArray<FVector>& OutLocations) const
{
	FMassEntityManager* EntityManager = QueryContext.EntityManager;
	if (!EntityManager || !QueryContext.QuerierEntity.IsValid())
	{
		return;
	}

	if (!EntityManager->IsEntityActive(QueryContext.QuerierEntity))
	{
		return;
	}

	TSet<FMassEntityHandle> SeenEntities;

	// Helper lambda to add locations from a perception result fragment
	auto AddPerceivedLocations = [&](const FArcMassPerceptionResultFragmentBase& Result)
	{
		for (const FArcPerceivedEntity& Perceived : Result.PerceivedEntities)
		{
			if (!Perceived.Entity.IsValid() || !EntityManager->IsEntityActive(Perceived.Entity))
			{
				continue;
			}

			bool bAlreadyInSet = false;
			SeenEntities.Add(Perceived.Entity, &bAlreadyInSet);
			if (bAlreadyInSet)
			{
				continue;
			}

			if (bUseLastKnownLocation)
			{
				OutLocations.Add(Perceived.LastKnownLocation);
			}
			else
			{
				const FMassEntityView EntityView(*EntityManager, Perceived.Entity);
				if (const FTransformFragment* Transform = EntityView.GetFragmentDataPtr<FTransformFragment>())
				{
					OutLocations.Add(Transform->GetTransform().GetLocation());
				}
				else
				{
					OutLocations.Add(Perceived.LastKnownLocation);
				}
			}
		}
	};

	const FMassEntityView QuerierView(*EntityManager, QueryContext.QuerierEntity);

	// Sight
	if (Sense == EArcTQSPerceptionSense::Sight || Sense == EArcTQSPerceptionSense::Both)
	{
		if (const FArcMassSightPerceptionResult* SightResult = QuerierView.GetFragmentDataPtr<FArcMassSightPerceptionResult>())
		{
			AddPerceivedLocations(*SightResult);
		}
	}

	// Hearing
	if (Sense == EArcTQSPerceptionSense::Hearing || Sense == EArcTQSPerceptionSense::Both)
	{
		if (const FArcMassHearingPerceptionResult* HearingResult = QuerierView.GetFragmentDataPtr<FArcMassHearingPerceptionResult>())
		{
			AddPerceivedLocations(*HearingResult);
		}
	}
}
