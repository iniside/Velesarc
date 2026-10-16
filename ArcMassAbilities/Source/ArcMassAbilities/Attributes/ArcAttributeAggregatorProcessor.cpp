// Copyright Lukasz Baran. All Rights Reserved.

#include "Attributes/ArcAttributeAggregatorProcessor.h"
#include "ArcMassAbilitiesLogs.h"

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
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/EntityView.h"
#include "Mass/Commands.h"
#include "StructUtils/SharedStruct.h"
#include "Events/ArcPendingEvents.h"

UArcAttributeAggregatorProcessor::UArcAttributeAggregatorProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
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
}

void UArcAttributeAggregatorProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcAttributeAggregator);

	TArray<FArcPendingAttributeEvent> PendingAttributeEvents;
	TArray<FMassEntityHandle> EntitiesNeedingRecalc;

	EntityQuery.ForEachEntityChunk(Context, [&EntityManager, &EntitiesNeedingRecalc, &PendingAttributeEvents](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcAggregatorFragment> AggregatorFragments = Ctx.GetMutableFragmentView<FArcAggregatorFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcAggregatorFragment& AggFrag = AggregatorFragments[EntityIt];
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);
			FMassEntityView EntityView(EntityManager, EntityHandle);

			const FArcAttributeHandlerSharedFragment* SharedHandler = EntityView.GetConstSharedFragmentDataPtr<FArcAttributeHandlerSharedFragment>();
			const UArcAttributeHandlerConfig* HandlerConfig = SharedHandler ? SharedHandler->Config : nullptr;

			const FArcOwnedTagsFragment* TagsPtr = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
			const FArcOwnedTagsFragment& Tags = TagsPtr ? *TagsPtr : FArcOwnedTagsFragment{};

			TArray<FArcAttributeRef> EvaluationOrder;
			AggFrag.GetEvaluationOrder(EvaluationOrder);

			for (const FArcAttributeRef& AttrRef : EvaluationOrder)
			{
				FArcAggregator* AggregatorPtr = AggFrag.FindAggregator(AttrRef);
				if (!AggregatorPtr)
				{
					continue;
				}
				FArcAggregator& Aggregator = *AggregatorPtr;

				ArcAttributes::EnsureAggregationPolicy(Aggregator, AttrRef, HandlerConfig);

				FProperty* Prop = AttrRef.GetCachedProperty();
				if (!Prop)
				{
					continue;
				}

				FStructView FragmentView = EntityManager.GetMutableSparseElementDataForEntity(AttrRef.FragmentType, EntityHandle);
				uint8* FragmentMemory = FragmentView.GetMemory();
				if (!FragmentMemory)
				{
					continue;
				}

				FArcAttribute* Attr = Prop->ContainerPtrToValuePtr<FArcAttribute>(FragmentMemory);
				const float OldValue = Attr->CurrentValue;

				FArcAggregationContext AggContext{EntityHandle, Tags.Tags.GetTagContainer(), EntityManager, Attr->BaseValue};
				float NewValue = Aggregator.Evaluate(AggContext);
				ARCMA_VLOG(EntityManager.GetWorld(), Verbose,
					TEXT("[Attr] Recalc attr=%s::%s old=%.4f new=%.4f entity=%s"),
					*AttrRef.FragmentType->GetName(),
					*AttrRef.PropertyName.ToString(),
					OldValue,
					NewValue,
					*EntityHandle.DebugGetDescription());

				const float PreNotifyBase = Attr->BaseValue;

				FArcAttributeHandlerContext HandlerContext(EntityManager, &Ctx, EntityHandle, FragmentMemory, HandlerConfig, AttrRef);
				bool bAccepted = ArcAttributes::NotifyAttributeChanged(HandlerContext, AttrRef, *Attr, OldValue, NewValue);
				if (!bAccepted)
				{
					continue;
				}
				if (Attr->BaseValue != PreNotifyBase)
				{
					ARCMA_VLOG(EntityManager.GetWorld(), Verbose,
						TEXT("[Attr] Re-signal AttributeRecalculate entity=%s reason=BaseValue clamp"),
						*EntityHandle.DebugGetDescription());
					EntitiesNeedingRecalc.AddUnique(EntityHandle);
				}
				if (OldValue != Attr->CurrentValue)
				{
					PendingAttributeEvents.Add(FArcPendingAttributeEvent{EntityHandle, AttrRef, OldValue, Attr->CurrentValue});
				}
			}

		}
	});

	ArcAttributes::FlushAttributeChangeEvents(EntityManager, Context, PendingAttributeEvents);

	if (!EntitiesNeedingRecalc.IsEmpty())
	{
		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntities(UE::ArcMassAbilities::Signals::AttributeRecalculate, EntitiesNeedingRecalc);
		}
	}
}
