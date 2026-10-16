// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcResourceAttributeHandlers.h"

#include "ArcAbilityKitTags.h"
#include "Attributes/ArcAttributeNotification.h"
#include "Events/ArcGameEvent.h"
#include "Events/ArcGameEventFunctions.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"

namespace ArcResourceAttributeHandlersPrivate
{
	FArcAttribute* ResolveEntityAttribute(const FArcAttributeHandlerContext& Context, const FArcAttributeRef& AttributeRef)
	{
		if (!AttributeRef.IsValid())
		{
			return nullptr;
		}

		FProperty* Property = AttributeRef.GetCachedProperty();
		if (!Property)
		{
			return nullptr;
		}

		FStructView FragmentView = Context.EntityManager.GetMutableSparseElementDataForEntity(AttributeRef.FragmentType, Context.Entity);
		uint8* FragmentMemory = FragmentView.GetMemory();
		if (!FragmentMemory)
		{
			return nullptr;
		}

		return Property->ContainerPtrToValuePtr<FArcAttribute>(FragmentMemory);
	}

	float ResolveUpperBound(const FArcAttributeHandlerContext& Context, const FArcAttributeRef& MaxAttribute, const TOptional<float>& MaxConst)
	{
		float UpperBound = TNumericLimits<float>::Max();
		if (const FArcAttribute* Max = ResolveEntityAttribute(Context, MaxAttribute))
		{
			UpperBound = Max->GetCurrentValue();
		}
		if (MaxConst.IsSet())
		{
			UpperBound = FMath::Min(UpperBound, MaxConst.GetValue());
		}
		return UpperBound;
	}
}

void FArcAttributeHandler_MetaToResource::PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const
{
	using namespace ArcResourceAttributeHandlersPrivate;

	const float Requested = NewValue;
	Attribute.InitValue(0.f);

	if (Requested == 0.f || (Op != EArcResourceOp::AddSigned && Requested < 0.f))
	{
		return;
	}

	FArcAttribute* ResourceAttribute = ResolveEntityAttribute(Context, Resource);
	if (!ResourceAttribute)
	{
		return;
	}

	const float Delta = Op == EArcResourceOp::Subtract ? -Requested : Requested;
	const float OldResource = ResourceAttribute->GetCurrentValue();
	const float NewResource = FMath::Clamp(OldResource + Delta, MinConst, ResolveUpperBound(Context, MaxAttribute, MaxConst));
	ArcAttributes::SetAttributeDirectAndNotify(Context, Resource, *ResourceAttribute, NewResource);

	if (!EventTag.IsValid())
	{
		return;
	}

	UArcGameEventSubsystem* EventBus = UWorld::GetSubsystem<UArcGameEventSubsystem>(Context.EntityManager.GetWorld());
	if (!EventBus)
	{
		return;
	}

	FArcResourceChangedPayload Payload;
	Payload.Resource = Resource;
	Payload.OldValue = OldResource;
	Payload.NewValue = NewResource;
	Payload.Requested = Requested;

	FArcGameEvent Event;
	Event.Tag = EventTag;
	Event.Instigator = Context.SourceEntity;
	Event.Target = Context.Entity;
	Event.Magnitude = NewResource - OldResource;
	ArcGameEvents::GatherEnvelopeTags(Context.EntityManager, Event);
	Event.Payload = FInstancedStruct::Make(Payload);
	EventBus->Queue(MoveTemp(Event));
}

void FArcAttributeHandler_HealthDamage::PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) const
{
	using namespace ArcResourceAttributeHandlersPrivate;

	const float Requested = NewValue;
	Attribute.InitValue(0.f);

	if (Requested == 0.f || (Op != EArcResourceOp::AddSigned && Requested < 0.f))
	{
		return;
	}

	FArcAttribute* ResourceAttribute = ResolveEntityAttribute(Context, Resource);
	if (!ResourceAttribute)
	{
		return;
	}

	const float Delta = Op == EArcResourceOp::Subtract ? -Requested : Requested;
	const float OldResource = ResourceAttribute->GetCurrentValue();
	const float NewResource = FMath::Clamp(OldResource + Delta, MinConst, ResolveUpperBound(Context, MaxAttribute, MaxConst));
	ArcAttributes::SetAttributeDirectAndNotify(Context, Resource, *ResourceAttribute, NewResource);

	const float AppliedChange = NewResource - OldResource;
	if (AppliedChange == 0.f)
	{
		return;
	}

	UArcGameEventSubsystem* EventBus = UWorld::GetSubsystem<UArcGameEventSubsystem>(Context.EntityManager.GetWorld());
	if (!EventBus)
	{
		return;
	}

	FArcHealthChangedPayload Payload;
	Payload.OldValue = OldResource;
	Payload.NewValue = NewResource;
	Payload.Requested = Requested;
	if (const FTransformFragment* TransformFragment = Context.EntityManager.GetFragmentDataPtr<FTransformFragment>(Context.Entity))
	{
		Payload.TargetLocation = TransformFragment->GetTransform().GetLocation();
	}

	FArcGameEvent Event;
	Event.Tag = ArcAbilityKitTags::Event_HealthChanged;
	Event.Instigator = Context.SourceEntity;
	Event.Target = Context.Entity;
	Event.Magnitude = AppliedChange;
	Event.SourceTags = Context.CapturedSourceTags;
	Event.TargetTags = Context.CapturedTargetTags;
	Event.Payload = FInstancedStruct::Make(Payload);
	EventBus->Queue(MoveTemp(Event));
}

bool FArcAttributeHandler_ClampRange::PreChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float& NewValue) const
{
	NewValue = FMath::Clamp(NewValue, MinConst, ArcResourceAttributeHandlersPrivate::ResolveUpperBound(Context, MaxAttribute, MaxConst));
	return true;
}
