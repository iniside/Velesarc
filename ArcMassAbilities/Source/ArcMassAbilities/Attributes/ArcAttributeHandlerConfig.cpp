// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcAttributeHandlerConfig.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "Fragments/ArcAggregatorFragment.h"

FArcAttributeHandlerContext::FArcAttributeHandlerContext(
	FMassEntityManager& InEntityManager,
	FMassExecutionContext* InMassContext,
	FMassEntityHandle InEntity,
	uint8* InOwningFragmentMemory,
	const UArcAttributeHandlerConfig* InHandlerConfig)
	: EntityManager(InEntityManager)
	, MassContext(InMassContext)
	, World(InEntityManager.GetWorld())
	, Entity(InEntity)
	, OwningFragmentMemory(InOwningFragmentMemory)
	, HandlerConfig(InHandlerConfig)
{}

FArcAttribute* FArcAttributeHandlerContext::ResolveAttribute(const FArcAttributeRef& AttrRef) const
{
	check(OwningFragmentMemory);
	check(AttrRef.FragmentType);

	FProperty* Prop = AttrRef.GetCachedProperty();
	if (!Prop)
	{
		return nullptr;
	}

	return Prop->ContainerPtrToValuePtr<FArcAttribute>(OwningFragmentMemory);
}

bool FArcAttributeHandlerContext::ModifyInline(const FArcAttributeRef& AttrRef, float NewValue)
{
	FArcAttribute* Attr = ResolveAttribute(AttrRef);
	if (!Attr)
	{
		return false;
	}

	float OldValue = Attr->CurrentValue;
	if (OldValue == NewValue)
	{
		return true;
	}

	if (HandlerConfig)
	{
		const FArcAttributeHandler* Handler = HandlerConfig->FindHandler(AttrRef);
		if (Handler && !const_cast<FArcAttributeHandler*>(Handler)->PreChange(*this, *Attr, NewValue))
		{
			return false;
		}
	}

	Attr->CurrentValue = NewValue;

	if (HandlerConfig)
	{
		const FArcAttributeHandler* Handler = HandlerConfig->FindHandler(AttrRef);
		if (Handler)
		{
			const_cast<FArcAttributeHandler*>(Handler)->PostChange(*this, *Attr, OldValue, NewValue);
		}
	}

	return true;
}

float FArcAttributeHandlerContext::EvaluateWithTags(
	const FArcAttributeRef& AttrRef, float BaseValue,
	const FGameplayTagContainer& SourceTags,
	const FGameplayTagContainer& TargetTags) const
{
	FMassEntityView EntityView(EntityManager, Entity);
	const FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
	if (!AggFrag)
	{
		return BaseValue;
	}

	const FArcAggregator* Agg = AggFrag->Aggregators.Find(AttrRef);
	if (!Agg)
	{
		return BaseValue;
	}

	return Agg->EvaluateWithTags(BaseValue, SourceTags, TargetTags);
}

const FArcAttributeHandler* UArcAttributeHandlerConfig::FindHandler(const FArcAttributeRef& AttributeRef) const
{
	const FInstancedStruct* Found = Handlers.Find(AttributeRef);
	if (Found && Found->IsValid())
	{
		return Found->GetPtr<FArcAttributeHandler>();
	}
	return nullptr;
}

const FInstancedStruct* UArcAttributeHandlerConfig::FindAggregationPolicy(const FArcAttributeRef& AttributeRef) const
{
	const FInstancedStruct* Found = AggregationPolicies.Find(AttributeRef);
	if (Found && Found->IsValid())
	{
		return Found;
	}
	return nullptr;
}
