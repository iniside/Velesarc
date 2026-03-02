// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_Need.h"
#include "MassEntityManager.h"
#include "NeedsSystem/ArcNeedsFragment.h"

static float ReadNeedValue(const FMassEntityManager* EntityManager, FMassEntityHandle EntityHandle, const UScriptStruct* NeedType, float MaxNeedValue)
{
	if (!EntityManager || !EntityHandle.IsSet() || !NeedType)
	{
		return 0.0f;
	}

	if (!EntityManager->IsEntityValid(EntityHandle))
	{
		return 0.0f;
	}

	const FStructView FragmentView = EntityManager->GetFragmentDataStruct(EntityHandle, NeedType);
	if (!FragmentView.IsValid())
	{
		return 0.0f;
	}

	const FArcNeedFragment* NeedFragment = FragmentView.GetPtr<FArcNeedFragment>();
	if (!NeedFragment)
	{
		return 0.0f;
	}

	return FMath::Clamp(NeedFragment->CurrentValue / MaxNeedValue, 0.0f, 1.0f);
}

float FArcUtilityConsideration_TargetNeed::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	return ReadNeedValue(Context.EntityManager, Target.EntityHandle, NeedType, MaxNeedValue);
}

float FArcUtilityConsideration_OwnerNeed::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	return ReadNeedValue(Context.EntityManager, Context.QuerierEntity, NeedType, MaxNeedValue);
}
