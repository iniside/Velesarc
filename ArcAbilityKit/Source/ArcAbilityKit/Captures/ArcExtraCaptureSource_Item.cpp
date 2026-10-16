// Copyright Lukasz Baran. All Rights Reserved.

#include "Captures/ArcExtraCaptureSource_Item.h"

#include "Sources/ArcSourceItemResolver.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/ArcItemData.h"
#include "Effects/ArcEffectContext.h"

bool FArcExtraCaptureSource_Item::ResolveValue(
	const FArcEffectContext& Context,
	FMassEntityManager& /*EntityManager*/,
	const FArcEvaluateParameters& EvalParams,
	float& OutValue) const
{
	if (!Attribute.IsValid())
	{
		return false;
	}

	const FArcItemData* ItemData = ArcAbilityKit::FindSourceItem(Context);
	if (!ItemData)
	{
		return false;
	}

	const FArcItemInstance_ItemStats* StatsInstance =
		ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(ItemData);
	if (!StatsInstance)
	{
		return false;
	}

	OutValue = StatsInstance->GetStatValue(Attribute, EvalParams.SourceTags);
	return true;
}
