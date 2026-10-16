// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcAttributeObserverProcessor.h"
#include "ArcMassAbilitiesLogs.h"

#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Fragments/ArcAttributeChangedFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Signals/ArcAttributeChangedSignal.h"
#include "MassSignalSubsystem.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/EntityView.h"

UArcAttributeObserverProcessor::UArcAttributeObserverProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	bRequiresGameThreadExecution = true;

	// Run after the aggregator so observers see fully-aggregated attribute values.
	ExecutionOrder.ExecuteAfter.Add(TEXT("ArcAttributeAggregatorProcessor"));
}

void UArcAttributeObserverProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMassAbilities::Signals::AttributeChanged);
}

void UArcAttributeObserverProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// Optional — entities without the shared handler fragment are skipped in SignalEntities.
	EntityQuery.AddConstSharedRequirement<FArcAttributeHandlerSharedFragment>(EMassFragmentPresence::Optional);
}

void UArcAttributeObserverProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcAttributeObserver);

	EntityQuery.ForEachEntityChunk(Context, [&EntityManager](FMassExecutionContext& Ctx)
	{
		const FArcAttributeHandlerSharedFragment* SharedHandler = Ctx.GetConstSharedFragmentPtr<FArcAttributeHandlerSharedFragment>();
		if (!SharedHandler)
		{
			return;
		}

		const UArcAttributeHandlerConfig* HandlerConfig = SharedHandler->Config;
		if (!HandlerConfig)
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);

			// Fetch mutable so we can Reset() after dispatch — a subsequent AttributeChanged
			// signal that arrives without a fragment rewrite must not re-dispatch stale entries.
			FStructView ChangesView = EntityManager.GetMutableSparseElementDataForEntity(
				FArcAttributeChangedFragment::StaticStruct(), EntityHandle);
			FArcAttributeChangedFragment* Changes = ChangesView.GetPtr<FArcAttributeChangedFragment>();
			if (!Changes || Changes->Changes.IsEmpty())
			{
				continue;
			}

			// Copy the entries so Reset() below cannot interact with the iteration.
			// Handlers write attribute changes via the pending-ops pipeline, not this fragment directly.
			TArray<FArcAttributeChangeEntry> LocalEntries = Changes->Changes;
			Changes->Changes.Reset();

			for (const FArcAttributeChangeEntry& Entry : LocalEntries)
			{
				const TArray<FArcAttributeRef>* Observers = HandlerConfig->FindObserversFor(Entry.Attribute);
				if (!Observers)
				{
					continue;
				}

				for (const FArcAttributeRef& ObserverRef : *Observers)
				{
					if (!ObserverRef.IsValid())
					{
						continue;
					}

					// Resolve the observer's fragment memory — skip if entity lacks the fragment
					FStructView ObserverFragView = EntityManager.GetMutableSparseElementDataForEntity(ObserverRef.FragmentType, EntityHandle);
					uint8* ObserverFragMem = ObserverFragView.GetMemory();
					if (!ObserverFragMem)
					{
						ARCMA_VLOG(Ctx.GetWorld(), Warning,
							TEXT("[Attr] Entity %s lacks fragment %s declared by observer config"),
							*EntityHandle.DebugGetDescription(),
							ObserverRef.FragmentType ? *ObserverRef.FragmentType->GetName() : TEXT("<null>"));
						continue;
					}

					const FArcAttributeHandler* Handler = HandlerConfig->FindHandler(ObserverRef);
					if (!Handler)
					{
						continue;
					}

					FArcAttributeHandlerContext ObsCtx(EntityManager, &Ctx, EntityHandle, ObserverFragMem, HandlerConfig, ObserverRef);
					FArcAttribute* Self = ObsCtx.ResolveAttribute(ObserverRef);
					if (!Self)
					{
						continue;
					}

					ARCMA_VLOG(Ctx.GetWorld(), Log,
						TEXT("[Attr] Observer dispatch entity=%s changed=%s::%s obs=%s::%s old=%.4f new=%.4f"),
						*EntityHandle.DebugGetDescription(),
						Entry.Attribute.FragmentType ? *Entry.Attribute.FragmentType->GetName() : TEXT("<null>"),
						*Entry.Attribute.PropertyName.ToString(),
						ObserverRef.FragmentType ? *ObserverRef.FragmentType->GetName() : TEXT("<null>"),
						*ObserverRef.PropertyName.ToString(),
						Entry.OldValue,
						Entry.NewValue);

					Handler->OnObservedAttributeChanged(
						ObsCtx, *Self, Entry.Attribute, Entry.OldValue, Entry.NewValue);
				}
			}
		}
	});
}
