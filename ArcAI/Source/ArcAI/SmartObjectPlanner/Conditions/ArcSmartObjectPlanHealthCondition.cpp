// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanHealthCondition.h"

#include "Mass/EntityManager.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

namespace ArcSmartObjectPlanHealthCondition_Internal
{
	static bool ResolveAttributeValue(
		const FMassEntityManager& EntityManager,
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

bool FArcSmartObjectPlanHealthCondition::CanUseEntity(
	const FArcPotentialEntity& Entity,
	const FArcSmartObjectPlanEvaluationContext& Context) const
{
	if (!Context.EntityManager)
	{
		return false;
	}

	const FMassEntityHandle TargetEntity = bTestRequestingEntity
		? Context.RequestingEntity
		: Entity.EntityHandle;

	float HealthValue = 0.f;
	float MaxHealthValue = 0.f;
	if (!ArcSmartObjectPlanHealthCondition_Internal::ResolveAttributeValue(*Context.EntityManager, TargetEntity, HealthAttribute, HealthValue))
	{
		return false;
	}
	if (!ArcSmartObjectPlanHealthCondition_Internal::ResolveAttributeValue(*Context.EntityManager, TargetEntity, MaxHealthAttribute, MaxHealthValue))
	{
		return false;
	}

	const float HealthPercent = (MaxHealthValue > 0.f)
		? (HealthValue / MaxHealthValue)
		: 0.f;

	switch (Comparison)
	{
	case EArcHealthComparison::GreaterThan:
		return HealthPercent > HealthThreshold;
	case EArcHealthComparison::LessThan:
		return HealthPercent < HealthThreshold;
	case EArcHealthComparison::GreaterOrEqual:
		return HealthPercent >= HealthThreshold;
	case EArcHealthComparison::LessOrEqual:
		return HealthPercent <= HealthThreshold;
	default:
		return false;
	}
}
