// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_NeedResource.h"
#include "NeedsSystem/ArcNeedsFragment.h"
#include "MassEntityManager.h"

float FArcTQSStep_NeedResource::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	if (!QueryContext.EntityManager)
	{
		return bFilterItemsWithoutResource ? 0.0f : DefaultScore;
	}

	// Read querier's need value
	float NeedFactor = 1.0f;
	if (NeedType && QueryContext.QuerierEntity.IsValid()
		&& QueryContext.EntityManager->IsEntityValid(QueryContext.QuerierEntity))
	{
		const FStructView NeedView = QueryContext.EntityManager->GetFragmentDataStruct(
			QueryContext.QuerierEntity, NeedType);

		if (NeedView.IsValid())
		{
			const FArcNeedFragment* NeedFragment = NeedView.GetPtr<FArcNeedFragment>();
			NeedFactor = FMath::Clamp(NeedFragment->CurrentValue / 100.0f, 0.0f, 1.0f);
		}
	}

	// Read target's resource amount
	float ResourceFactor = 0.0f;
	if (ResourceType && Item.EntityHandle.IsValid()
		&& QueryContext.EntityManager->IsEntityValid(Item.EntityHandle))
	{
		const FStructView ResourceView = QueryContext.EntityManager->GetFragmentDataStruct(
			Item.EntityHandle, ResourceType);

		if (ResourceView.IsValid())
		{
			const FArcResourceFragment* ResourceFragment = ResourceView.GetPtr<FArcResourceFragment>();
			ResourceFactor = FMath::Clamp(
				ResourceFragment->CurrentResourceAmount / MaxResourceForNormalization, 0.0f, 1.0f);
		}
		else
		{
			return bFilterItemsWithoutResource ? 0.0f : DefaultScore;
		}
	}
	else
	{
		return bFilterItemsWithoutResource ? 0.0f : DefaultScore;
	}

	const float RawScore = NeedFactor * ResourceFactor;
	return ResponseCurve.Evaluate(RawScore);
}
