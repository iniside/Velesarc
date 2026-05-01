// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcPendingAttributeOpsProcessor.h"

#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAggregator.h"
#include "Attributes/ArcAttributeNotification.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Events/ArcPendingEvents.h"
#include "Events/ArcEffectEventSubsystem.h"
#include "Fragments/ArcAttributeChangedFragment.h"
#include "Signals/ArcAttributeChangedSignal.h"
#include "MassSignalSubsystem.h"
#include "MassExecutionContext.h"
#include "MassEntityView.h"
#include "MassCommands.h"

UArcPendingAttributeOpsProcessor::UArcPendingAttributeOpsProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcPendingAttributeOpsProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMassAbilities::Signals::PendingAttributeOps);
}

void UArcPendingAttributeOpsProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcPendingAttributeOpsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcOwnedTagsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcAttributeHandlerSharedFragment>(EMassFragmentPresence::Optional);
}

void UArcPendingAttributeOpsProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcPendingAttributeOps);

	bool bNeedsRecalculate = false;
	TArray<FArcPendingAttributeEvent> PendingAttributeEvents;

	EntityQuery.ForEachEntityChunk(Context, [&EntityManager, &bNeedsRecalculate, &PendingAttributeEvents](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcPendingAttributeOpsFragment> PendingFragments = Ctx.GetMutableFragmentView<FArcPendingAttributeOpsFragment>();

		const FArcAttributeHandlerSharedFragment* SharedHandler = Ctx.GetConstSharedFragmentPtr<FArcAttributeHandlerSharedFragment>();
		const UArcAttributeHandlerConfig* HandlerConfig = SharedHandler ? SharedHandler->Config : nullptr;

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcPendingAttributeOpsFragment& PendingFrag = PendingFragments[EntityIt];
			if (PendingFrag.PendingOps.IsEmpty())
			{
				continue;
			}

			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);
			const FMassEntityView EntityView(EntityManager, EntityHandle);

			TArray<FArcPendingAttributeOp> Ops = MoveTemp(PendingFrag.PendingOps);
			PendingFrag.PendingOps.Reset();

			for (const FArcPendingAttributeOp& PendingOp : Ops)
			{
				if (!PendingOp.Attribute.IsValid())
				{
					continue;
				}

				FProperty* Prop = PendingOp.Attribute.GetCachedProperty();
				if (!Prop)
				{
					continue;
				}

				FStructView FragmentView = EntityView.GetFragmentDataStruct(PendingOp.Attribute.FragmentType);
				uint8* FragmentMemory = FragmentView.GetMemory();
				if (!FragmentMemory)
				{
					continue;
				}

				FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(FragmentMemory);
				float OldBase = Attr->BaseValue;

				switch (PendingOp.Operation)
				{
				case EArcModifierOp::Add:
					Attr->BaseValue += PendingOp.Magnitude;
					break;
				case EArcModifierOp::MultiplyAdditive:
					Attr->BaseValue *= (1.f + PendingOp.Magnitude);
					break;
				case EArcModifierOp::DivideAdditive:
					if ((1.f + PendingOp.Magnitude) != 0.f)
					{
						Attr->BaseValue /= (1.f + PendingOp.Magnitude);
					}
					break;
				case EArcModifierOp::MultiplyCompound:
					Attr->BaseValue *= PendingOp.Magnitude;
					break;
				case EArcModifierOp::AddFinal:
					Attr->BaseValue += PendingOp.Magnitude;
					break;
				case EArcModifierOp::Override:
					Attr->BaseValue = PendingOp.Magnitude;
					break;
				}

				float NewBase = Attr->BaseValue;
				if (OldBase == NewBase)
				{
					continue;
				}

				FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
				FArcAggregator* Aggregator = AggFrag ? AggFrag->FindAggregator(PendingOp.Attribute) : nullptr;

				float FinalValue = NewBase;
				if (Aggregator && !Aggregator->IsEmpty())
				{
					TConstArrayView<FArcOwnedTagsFragment> OwnedTags = Ctx.GetFragmentView<FArcOwnedTagsFragment>();
					const FArcOwnedTagsFragment& Tags = OwnedTags[EntityIt];
					FArcAggregationContext AggContext{EntityHandle, Tags.Tags.GetTagContainer(), EntityManager, NewBase};
					FinalValue = Aggregator->Evaluate(AggContext);
					bNeedsRecalculate = true;
				}

				FArcAttributeHandlerContext HandlerContext(EntityManager, &Ctx, EntityHandle, FragmentMemory, HandlerConfig);

				const float PreNotifyValue = Attr->CurrentValue;
				const bool bChanged = ArcAttributes::NotifyAttributeChanged(
					HandlerContext, PendingOp.Attribute, *Attr, Attr->CurrentValue, FinalValue);
				if (bChanged && PendingOp.OpType == EArcAttributeOpType::Execute)
				{
					ArcAttributes::NotifyAttributeExecuted(
						HandlerContext, PendingOp.Attribute, *Attr, OldBase, Attr->CurrentValue);
				}
				if (PreNotifyValue != Attr->CurrentValue)
				{
					PendingAttributeEvents.Add(FArcPendingAttributeEvent{EntityHandle, PendingOp.Attribute, PreNotifyValue, Attr->CurrentValue});
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

	if (bNeedsRecalculate)
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
