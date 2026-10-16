// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcAttributeNotification.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "ArcMassAbilitiesLogs.h"
#include "Events/ArcPendingEvents.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Events/ArcGameEventFunctions.h"
#include "Events/ArcAttributeEventPayloads.h"
#include "Debugger/ArcMassAbilityTrace.h"
#include "Fragments/ArcAttributeChangedFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Signals/ArcAttributeChangedSignal.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/Commands.h"
#include "MassSignalSubsystem.h"

namespace ArcAttributeNotificationPrivate
{
	constexpr int32 MaxLiveRecalcDepth = 8;
	constexpr int32 MaxLiveRecalcsPerPass = 256;
	thread_local int32 LiveRecalcDepth = 0;
	thread_local int32 LiveRecalcCount = 0;

	void PublishAttributeChanges(FMassEntityManager& EntityManager, TConstArrayView<FArcPendingAttributeEvent> Events)
	{
		if (Events.IsEmpty())
		{
			return;
		}

		UArcGameEventSubsystem* EventBus = UWorld::GetSubsystem<UArcGameEventSubsystem>(EntityManager.GetWorld());
		if (!EventBus)
		{
			return;
		}

		TMap<FMassEntityHandle, FArcGameEvent> EnvelopeByEntity;
		for (const FArcPendingAttributeEvent& Evt : Events)
		{
			TRACE_ARCMA_ATTRIBUTE_CHANGED(EventBus, Evt.Entity, Evt.Attribute, Evt.OldValue, Evt.NewValue);

			FArcGameEvent* Envelope = EnvelopeByEntity.Find(Evt.Entity);
			if (!Envelope)
			{
				Envelope = &EnvelopeByEntity.Add(Evt.Entity);
				Envelope->Tag = ArcGameEventTags::AttributeChanged;
				Envelope->Instigator = Evt.Entity;
				Envelope->Target = Evt.Entity;
				ArcGameEvents::GatherEnvelopeTags(EntityManager, *Envelope);
			}

			FArcAttributeChangedPayload Payload;
			Payload.Attribute = Evt.Attribute;
			Payload.OldValue = Evt.OldValue;
			Payload.NewValue = Evt.NewValue;

			FArcGameEvent Event = *Envelope;
			Event.Magnitude = Evt.NewValue - Evt.OldValue;
			Event.Payload = FInstancedStruct::Make(Payload);
			EventBus->Queue(MoveTemp(Event));
		}
	}
}

namespace ArcAttributes
{

bool NotifyAttributeChanged(
	FArcAttributeHandlerContext& Context,
	const FArcAttributeRef& AttrRef,
	FArcAttribute& Attr,
	float OldValue,
	float NewValue)
{
	if (OldValue == NewValue)
	{
		return true;
	}

	if (Context.HandlerConfig)
	{
		const FArcAttributeHandler* Handler = Context.HandlerConfig->FindHandler(AttrRef);
		if (Handler && !Handler->PreChange(Context, Attr, NewValue))
		{
			ARCMA_VLOG(Context.World, Warning,
				TEXT("[Attr] PreChange rejected %s proposed=%.4f entity=%s"),
				*AttrRef.PropertyName.ToString(), NewValue,
				*Context.Entity.DebugGetDescription());
			return false;
		}
	}

	Attr.CurrentValue = NewValue;
	ARCMA_VLOG(Context.World, Log,
		TEXT("[Attr] %s: %.4f -> %.4f entity=%s"),
		*AttrRef.PropertyName.ToString(), OldValue, NewValue,
		*Context.Entity.DebugGetDescription());

	if (Context.HandlerConfig)
	{
		const FArcAttributeHandler* Handler = Context.HandlerConfig->FindHandler(AttrRef);
		if (Handler)
		{
			Handler->PostChange(Context, Attr, OldValue, NewValue);
		}
	}

	return true;
}

void NotifyAttributeExecuted(
	FArcAttributeHandlerContext& Context,
	const FArcAttributeRef& AttrRef,
	FArcAttribute& Attr,
	float OldValue,
	float NewValue)
{
	if (OldValue == NewValue)
	{
		return;
	}

	if (Context.HandlerConfig)
	{
		const FArcAttributeHandler* Handler = Context.HandlerConfig->FindHandler(AttrRef);
		if (Handler)
		{
			Handler->PostExecute(Context, Attr, OldValue, NewValue);
		}
	}
}

void SetAttributeDirectAndNotify(
	const FArcAttributeHandlerContext& Context,
	const FArcAttributeRef& AttributeRef,
	FArcAttribute& Attribute,
	float NewValue)
{
	const float OldValue = Attribute.GetCurrentValue();
	Attribute.SetValueDirect(NewValue);
	if (OldValue == NewValue)
	{
		return;
	}

	const FArcPendingAttributeEvent Change{Context.Entity, AttributeRef, OldValue, NewValue};
	ArcAttributeNotificationPrivate::PublishAttributeChanges(Context.EntityManager, MakeArrayView(&Change, 1));

	RecalculateLiveDependents(Context, AttributeRef);
}

void EnsureAggregationPolicy(
	FArcAggregator& Aggregator,
	const FArcAttributeRef& AttributeRef,
	const UArcAttributeHandlerConfig* HandlerConfig)
{
	if (Aggregator.AggregationPolicy.IsValid() || !HandlerConfig)
	{
		return;
	}

	const FInstancedStruct* PolicyStruct = HandlerConfig->FindAggregationPolicy(AttributeRef);
	if (PolicyStruct)
	{
		Aggregator.AggregationPolicy = FConstSharedStruct::Make(PolicyStruct->GetScriptStruct(), PolicyStruct->GetMemory());
	}
}

void RecalculateLiveDependents(
	FMassEntityManager& EntityManager,
	FMassExecutionContext* MassContext,
	FMassEntityHandle Entity,
	const UArcAttributeHandlerConfig* HandlerConfig,
	const FArcAttributeRef& ChangedAttribute,
	TArray<FArcPendingAttributeEvent>& OutEvents)
{
	using namespace ArcAttributeNotificationPrivate;

	if (!EntityManager.IsEntityValid(Entity))
	{
		return;
	}

	FMassEntityView EntityView(EntityManager, Entity);
	FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
	if (!AggFrag || !AggFrag->HasLiveDependents(ChangedAttribute))
	{
		return;
	}

	if (LiveRecalcDepth >= MaxLiveRecalcDepth)
	{
		UE_LOGF(LogArcMassAbilities, Warning,
			"Live dependent recalculation depth cap reached at %ls entity=%ls",
			*ChangedAttribute.PropertyName.ToString(), *Entity.DebugGetDescription());
		return;
	}

	TArray<FArcAttributeRef> Dependents;
	AggFrag->GatherLiveDependents(ChangedAttribute, Dependents);

	const FArcOwnedTagsFragment* OwnedTags = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
	const FGameplayTagContainer& OwnerTags = OwnedTags ? OwnedTags->GetTags().GetTagContainer() : FGameplayTagContainer::EmptyContainer;

	if (LiveRecalcDepth == 0)
	{
		LiveRecalcCount = 0;
	}
	++LiveRecalcDepth;

	for (const FArcAttributeRef& Dependent : Dependents)
	{
		if (++LiveRecalcCount > MaxLiveRecalcsPerPass)
		{
			UE_LOGF(LogArcMassAbilities, Warning,
				"Live dependent recalculation cap reached at %ls entity=%ls",
				*Dependent.PropertyName.ToString(), *Entity.DebugGetDescription());
			break;
		}

		FArcAggregator* Aggregator = AggFrag->FindAggregator(Dependent);
		FProperty* Prop = Dependent.GetCachedProperty();
		if (!Aggregator || !Prop)
		{
			continue;
		}

		FStructView FragmentView = EntityManager.GetMutableSparseElementDataForEntity(Dependent.FragmentType, Entity);
		uint8* FragmentMemory = FragmentView.GetMemory();
		if (!FragmentMemory)
		{
			continue;
		}

		EnsureAggregationPolicy(*Aggregator, Dependent, HandlerConfig);

		FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(FragmentMemory);
		const float OldValue = Attr->CurrentValue;
		const FArcAggregationContext AggContext{Entity, OwnerTags, EntityManager, Attr->BaseValue};
		const float NewValue = Aggregator->Evaluate(AggContext);

		FArcAttributeHandlerContext HandlerContext(EntityManager, MassContext, Entity, FragmentMemory, HandlerConfig, Dependent);
		NotifyAttributeChanged(HandlerContext, Dependent, *Attr, OldValue, NewValue);
		if (OldValue != Attr->CurrentValue)
		{
			OutEvents.Add(FArcPendingAttributeEvent{Entity, Dependent, OldValue, Attr->CurrentValue});
		}
	}

	--LiveRecalcDepth;
}

void RecalculateLiveDependents(
	const FArcAttributeHandlerContext& Context,
	const FArcAttributeRef& ChangedAttribute)
{
	TArray<FArcPendingAttributeEvent> Events;
	RecalculateLiveDependents(Context.EntityManager, Context.MassContext, Context.Entity,
	                          Context.HandlerConfig, ChangedAttribute, Events);
	ArcAttributeNotificationPrivate::PublishAttributeChanges(Context.EntityManager, Events);
}

void FlushAttributeChangeEvents(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	TConstArrayView<FArcPendingAttributeEvent> Events)
{
	if (Events.IsEmpty())
	{
		return;
	}

	ArcAttributeNotificationPrivate::PublishAttributeChanges(EntityManager, Events);

	TMap<FMassEntityHandle, TArray<FArcAttributeChangeEntry>> ChangesByEntity;
	TArray<FMassEntityHandle> ChangedEntities;

	for (const FArcPendingAttributeEvent& Evt : Events)
	{
		FArcAttributeChangeEntry Entry;
		Entry.Attribute = Evt.Attribute;
		Entry.OldValue = Evt.OldValue;
		Entry.NewValue = Evt.NewValue;
		ChangesByEntity.FindOrAdd(Evt.Entity).Add(Entry);
	}

	ChangesByEntity.GetKeys(ChangedEntities);

	Context.Defer().PushCommand<FMassDeferredSetCommand>(
		[ChangesByEntity = MoveTemp(ChangesByEntity)](const FMassEntityManager& InEntityManager)
		{
			FMassEntityManager& MutableEntityManager = const_cast<FMassEntityManager&>(InEntityManager);
			for (const TPair<FMassEntityHandle, TArray<FArcAttributeChangeEntry>>& Pair : ChangesByEntity)
			{
				FStructView SparseView = MutableEntityManager.AddSparseElementToEntity(Pair.Key, FArcAttributeChangedFragment::StaticStruct());
				FArcAttributeChangedFragment& Fragment = SparseView.Get<FArcAttributeChangedFragment>();
				Fragment.Changes = Pair.Value;
			}
		});

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
	if (SignalSubsystem)
	{
		SignalSubsystem->SignalEntitiesDeferred(Context, UE::ArcMassAbilities::Signals::AttributeChanged, ChangedEntities);
	}
}

} // namespace ArcAttributes
