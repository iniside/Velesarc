// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_PerceivedEntities.h"
#include "Perception/ArcMassSightPerception.h"
#include "Perception/ArcMassHearingPerception.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"

void FArcTQSGenerator_PerceivedEntities::GenerateItems(
	const FArcTQSQueryContext& QueryContext,
	TArray<FArcTQSTargetItem>& OutItems) const
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

	// Helper lambda to add perceived entities from a result fragment
	auto AddPerceivedEntities = [&](const FArcMassPerceptionResultFragmentBase& Result)
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

			FArcTQSTargetItem Item;
			Item.TargetType = EArcTQSTargetType::MassEntity;
			Item.EntityHandle = Perceived.Entity;

			if (bUseLastKnownLocation)
			{
				Item.Location = Perceived.LastKnownLocation;
			}
			else
			{
				// Resolve current location from entity transform
				const FMassEntityView EntityView(*EntityManager, Perceived.Entity);
				if (const FTransformFragment* Transform = EntityView.GetFragmentDataPtr<FTransformFragment>())
				{
					Item.Location = Transform->GetTransform().GetLocation();
				}
				else
				{
					Item.Location = Perceived.LastKnownLocation;
				}
			}

			OutItems.Add(MoveTemp(Item));
		}
	};

	const FMassEntityView QuerierView(*EntityManager, QueryContext.QuerierEntity);

	// Sight
	if (Sense == EArcTQSPerceptionSense::Sight || Sense == EArcTQSPerceptionSense::Both)
	{
		if (const FArcMassSightPerceptionResult* SightResult = QuerierView.GetFragmentDataPtr<FArcMassSightPerceptionResult>())
		{
			AddPerceivedEntities(*SightResult);
		}
	}

	// Hearing
	if (Sense == EArcTQSPerceptionSense::Hearing || Sense == EArcTQSPerceptionSense::Both)
	{
		if (const FArcMassHearingPerceptionResult* HearingResult = QuerierView.GetFragmentDataPtr<FArcMassHearingPerceptionResult>())
		{
			AddPerceivedEntities(*HearingResult);
		}
	}
}
