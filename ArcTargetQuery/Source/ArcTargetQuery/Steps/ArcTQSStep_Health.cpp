// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Health.h"

#include "Mass/EntityManager.h"

namespace ArcTQSStepHealth_Internal
{
	static bool ResolveAttributeValue(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcAttributeRef& Ref,
		float& OutValue)
	{
		if (!Ref.IsValid())
		{
			return false;
		}

		FConstStructView FragmentView = EntityManager.GetSparseElementDataForEntity(Ref.FragmentType, Entity);
		const uint8* FragmentMemory = FragmentView.GetMemory();
		if (!FragmentMemory)
		{
			return false;
		}

		FProperty* Prop = Ref.GetCachedProperty();
		if (!Prop)
		{
			return false;
		}

		const FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(FragmentMemory);
		if (!Attr)
		{
			return false;
		}

		OutValue = Attr->GetCurrentValue();
		return true;
	}
}

float FArcTQSStep_Health::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	if (!QueryContext.EntityManager || !Item.EntityHandle.IsValid())
	{
		return bFilterItemsWithoutHealth ? UnscorableRaw : DefaultScoreForNoHealth;
	}

	if (!QueryContext.EntityManager->IsEntityValid(Item.EntityHandle))
	{
		return bFilterItemsWithoutHealth ? UnscorableRaw : DefaultScoreForNoHealth;
	}

	float HealthValue = 0.f;
	float MaxHealthValue = 0.f;
	if (!ArcTQSStepHealth_Internal::ResolveAttributeValue(*QueryContext.EntityManager, Item.EntityHandle, HealthAttribute, HealthValue))
	{
		return bFilterItemsWithoutHealth ? UnscorableRaw : DefaultScoreForNoHealth;
	}
	if (!ArcTQSStepHealth_Internal::ResolveAttributeValue(*QueryContext.EntityManager, Item.EntityHandle, MaxHealthAttribute, MaxHealthValue))
	{
		return bFilterItemsWithoutHealth ? UnscorableRaw : DefaultScoreForNoHealth;
	}

	if (MaxHealthValue <= 0.0f)
	{
		return bFilterItemsWithoutHealth ? UnscorableRaw : DefaultScoreForNoHealth;
	}

	return FMath::Clamp(HealthValue / MaxHealthValue, 0.0f, 1.0f);
}
