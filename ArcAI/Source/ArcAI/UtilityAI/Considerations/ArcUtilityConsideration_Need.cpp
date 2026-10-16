// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_Need.h"
#include "Mass/EntityManager.h"
#include "Fragments/ArcNeedFragment.h"

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

FArcUtilityConsiderationResult FArcUtilityConsideration_TargetNeed::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	return FArcUtilityConsiderationResult::Accept(ReadNeedValue(Context.EntityManager, Target.EntityHandle, NeedType, MaxNeedValue));
}

FArcUtilityConsiderationResult FArcUtilityConsideration_OwnerNeed::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	return FArcUtilityConsiderationResult::Accept(ReadNeedValue(Context.EntityManager, Context.QuerierEntity, NeedType, MaxNeedValue));
}
