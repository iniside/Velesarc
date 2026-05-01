// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcAttributeAggregatorProcessor.h"

#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAggregator.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcAttributeNotification.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Fragments/ArcAttributeChangedFragment.h"
#include "Signals/ArcAttributeChangedSignal.h"
#include "MassSignalSubsystem.h"
#include "MassExecutionContext.h"
#include "MassEntityView.h"
#include "MassCommands.h"
#include "StructUtils/SharedStruct.h"
#include "Events/ArcPendingEvents.h"
#include "Events/ArcEffectEventSubsystem.h"

UArcAttributeAggregatorProcessor::UArcAttributeAggregatorProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcAttributeAggregatorProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMassAbilities::Signals::AttributeRecalculate);
}

void UArcAttributeAggregatorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcOwnedTagsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcAttributeHandlerSharedFragment>(EMassFragmentPresence::Optional);
}

void UArcAttributeAggregatorProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcAttributeAggregator);

	bool bNeedsResignal = false;
	TArray<FArcPendingAttributeEvent> PendingAttributeEvents;

	EntityQuery.ForEachEntityChunk(Context, [&EntityManager, &bNeedsResignal, &PendingAttributeEvents](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcAggregatorFragment> AggregatorFragments = Ctx.GetMutableFragmentView<FArcAggregatorFragment>();
		TConstArrayView<FArcOwnedTagsFragment> OwnedTags = Ctx.GetFragmentView<FArcOwnedTagsFragment>();

		const FArcAttributeHandlerSharedFragment* SharedHandler = Ctx.GetConstSharedFragmentPtr<FArcAttributeHandlerSharedFragment>();
		const UArcAttributeHandlerConfig* HandlerConfig = SharedHandler ? SharedHandler->Config : nullptr;

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcAggregatorFragment& AggFrag = AggregatorFragments[EntityIt];
			const FArcOwnedTagsFragment& Tags = OwnedTags[EntityIt];
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);

			const FMassEntityView EntityView(EntityManager, EntityHandle);

			for (TPair<FArcAttributeRef, FArcAggregator>& Pair : AggFrag.Aggregators)
			{
				const FArcAttributeRef& AttrRef = Pair.Key;
				FArcAggregator& Aggregator = Pair.Value;

				if (!Aggregator.AggregationPolicy.IsValid() && HandlerConfig)
				{
					const FInstancedStruct* PolicyStruct = HandlerConfig->FindAggregationPolicy(AttrRef);
					if (PolicyStruct)
					{
						Aggregator.AggregationPolicy = FConstSharedStruct::Make(PolicyStruct->GetScriptStruct(), PolicyStruct->GetMemory());
					}
				}

				FProperty* Prop = AttrRef.GetCachedProperty();
				if (!Prop)
				{
					continue;
				}

				FStructView FragmentView = EntityView.GetFragmentDataStruct(AttrRef.FragmentType);
				uint8* FragmentMemory = FragmentView.GetMemory();
				if (!FragmentMemory)
				{
					continue;
				}

				FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(FragmentMemory);
				const float OldValue = Attr->CurrentValue;

				FArcAggregationContext AggContext{EntityHandle, Tags.Tags.GetTagContainer(), EntityManager, Attr->BaseValue};
				float NewValue = Aggregator.Evaluate(AggContext);

				const float PreNotifyBase = Attr->BaseValue;

				FArcAttributeHandlerContext HandlerContext(EntityManager, &Ctx, EntityHandle, FragmentMemory, HandlerConfig);
				bool bAccepted = ArcAttributes::NotifyAttributeChanged(HandlerContext, AttrRef, *Attr, OldValue, NewValue);
				if (!bAccepted)
				{
					continue;
				}
				if (Attr->BaseValue != PreNotifyBase)
				{
					bNeedsResignal = true;
				}
				if (OldValue != Attr->CurrentValue)
				{
					PendingAttributeEvents.Add(FArcPendingAttributeEvent{EntityHandle, AttrRef, OldValue, Attr->CurrentValue});
				}
			}

		}
	});

	UArcEffectEventSubsystem* EventSys = UWorld::GetSubsystem<UArcEffectEventSubsystem>(EntityManager.GetWorld());
	if (EventSys)
	{
		for (const FArcPendingAttributeEvent& Evt : PendingAttributeEvents)
		{
			EventSys->BroadcastAttributeChanged(Evt.Entity, Evt.Attribute, Evt.OldValue, Evt.NewValue);
		}
	}

	if (!PendingAttributeEvents.IsEmpty())
	{
		TMap<FMassEntityHandle, TArray<FArcAttributeChangeEntry>> ChangesByEntity;
		TArray<FMassEntityHandle> ChangedEntities;

		for (const FArcPendingAttributeEvent& Evt : PendingAttributeEvents)
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

	if (bNeedsResignal)
	{
		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
		if (SignalSubsystem)
		{
			EntityQuery.ForEachEntityChunk(Context, [SignalSubsystem](FMassExecutionContext& Ctx)
			{
				for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
				{
					SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Ctx.GetEntity(EntityIt));
				}
			});
		}
	}
}
