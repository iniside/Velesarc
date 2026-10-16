// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_EntityHealth.h"

#include "Mass/EntityManager.h"

namespace ArcUtilityConsiderationEntityHealth_Internal
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

FArcUtilityConsiderationResult FArcUtilityConsideration_EntityHealth::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	const FMassEntityHandle SubjectEntity = Subject == EArcUtilitySubject::Querier ? Context.QuerierEntity : Target.EntityHandle;

	if (!Context.EntityManager || !SubjectEntity.IsSet())
	{
		return FArcUtilityConsiderationResult::Accept(0.0f);
	}

	if (!Context.EntityManager->IsEntityValid(SubjectEntity))
	{
		return FArcUtilityConsiderationResult::Accept(0.0f);
	}

	float HealthValue = 0.f;
	float MaxHealthValue = 0.f;
	if (!ArcUtilityConsiderationEntityHealth_Internal::ResolveAttributeValue(*Context.EntityManager, SubjectEntity, HealthAttribute, HealthValue) ||
		!ArcUtilityConsiderationEntityHealth_Internal::ResolveAttributeValue(*Context.EntityManager, SubjectEntity, MaxHealthAttribute, MaxHealthValue))
	{
		return FArcUtilityConsiderationResult::Reject(TEXT("HealthUnresolved"));
	}

	if (MaxHealthValue <= 0.0f)
	{
		return FArcUtilityConsiderationResult::Reject(TEXT("NoMaxHealth"));
	}

	return FArcUtilityConsiderationResult::Accept(FMath::Clamp(HealthValue / MaxHealthValue, 0.0f, 1.0f));
}
