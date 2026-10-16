// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcMagnitudeCalculation.h"
#include "Effects/ArcEffectContext.h"
#include "Attributes/ArcAttribute.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"

namespace ArcMagnitudeCalculationPrivate
{
	float ReadCurrentValue(const FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcAttributeRef& AttributeRef)
	{
		if (!AttributeRef.IsValid() || !EntityManager.IsEntityValid(Entity))
		{
			return 0.f;
		}

		FConstStructView FragView = EntityManager.GetSparseElementDataForEntity(AttributeRef.FragmentType, Entity);
		const uint8* Memory = FragView.GetMemory();
		FProperty* Prop = AttributeRef.GetCachedProperty();
		if (!Memory || !Prop)
		{
			return 0.f;
		}

		return Prop->ContainerPtrToValuePtr<FArcAttribute>(Memory)->CurrentValue;
	}
}

float FArcMagnitude_AttributeBased::Calculate(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager || !SourceAttribute.IsValid())
	{
		return 0.f;
	}

	FMassEntityHandle Entity = (CaptureFrom == EArcCaptureSource::Source)
		? Context.SourceEntity
		: Context.TargetEntity;

	if (!Context.EntityManager->IsEntityValid(Entity))
	{
		return 0.f;
	}

	FConstStructView FragView = Context.EntityManager->GetSparseElementDataForEntity(SourceAttribute.FragmentType, Entity);
	const uint8* Memory = FragView.GetMemory();
	if (!Memory)
	{
		return 0.f;
	}

	FProperty* Prop = SourceAttribute.GetCachedProperty();
	if (!Prop)
	{
		return 0.f;
	}

	const FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(Memory);
	return Attr->CurrentValue;
}

float FArcLiveMagnitude::Calculate(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager || !Context.EntityManager->IsEntityValid(Context.TargetEntity))
	{
		return 0.f;
	}

	FMassEntityView TargetView(*Context.EntityManager, Context.TargetEntity);
	const FArcOwnedTagsFragment* OwnedTags = TargetView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
	const FArcAggregationContext AggContext{Context.TargetEntity,
		OwnedTags ? OwnedTags->GetTags().GetTagContainer() : FGameplayTagContainer::EmptyContainer,
		*Context.EntityManager, 0.f};
	return EvaluateLive(AggContext);
}

void FArcLiveMagnitude_AttributeLinear::GetSourceAttributes(TArray<FArcAttributeRef>& OutAttributes) const
{
	if (Attribute.IsValid())
	{
		OutAttributes.Add(Attribute);
	}
}

float FArcLiveMagnitude_AttributeLinear::EvaluateLive(const FArcAggregationContext& Context) const
{
	const float Value = ArcMagnitudeCalculationPrivate::ReadCurrentValue(Context.EntityManager, Context.Owner, Attribute);
	return (Value + PreAdd) * Coefficient + PostAdd;
}
