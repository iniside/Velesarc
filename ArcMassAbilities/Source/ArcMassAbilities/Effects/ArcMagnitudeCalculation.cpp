// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcMagnitudeCalculation.h"
#include "Effects/ArcEffectContext.h"
#include "Attributes/ArcAttribute.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"

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

	FMassEntityView EntityView(*Context.EntityManager, Entity);
	FStructView FragView = EntityView.GetFragmentDataStruct(SourceAttribute.FragmentType);
	uint8* Memory = FragView.GetMemory();
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
