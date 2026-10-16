// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "ArcMassAbilitiesLogs.h"

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
#include "Fragments/ArcAttributeChangedFragment.h"
#include "Signals/ArcAttributeChangedSignal.h"
#include "MassSignalSubsystem.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/EntityView.h"
#include "Mass/Commands.h"

UArcPendingAttributeOpsProcessor::UArcPendingAttributeOpsProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
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
	// Anchor on a regular fragment so the query matches archetypes. Pending ops live in a
	// sparse fragment that may be added post-Commit (tests, deferred commands) — do not use
	// AddSparseRequirement + CreateSparseEntityIterator, which skips entities whose sparse
	// composition was not registered at archetype-build time. Resolve per entity instead.
	// Handler config is optional — lookup via FMassEntityView, not AddConstSharedRequirement.
	EntityQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcPendingAttributeOpsProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcPendingAttributeOps);

	TArray<FArcPendingAttributeEvent> PendingAttributeEvents;
	TArray<FMassEntityHandle> EntitiesNeedingRecalc;

	EntityQuery.ForEachEntityChunk(Context, [&EntityManager, &EntitiesNeedingRecalc, &PendingAttributeEvents](FMassExecutionContext& Ctx)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);
			FMassEntityView EntityView(EntityManager, EntityHandle);

			FArcPendingAttributeOpsFragment* PendingFrag = EntityView.GetSparseFragmentDataPtr<FArcPendingAttributeOpsFragment>();
			if (!PendingFrag || PendingFrag->PendingOps.IsEmpty())
			{
				continue;
			}

			const FArcAttributeHandlerSharedFragment* SharedHandler = EntityView.GetConstSharedFragmentDataPtr<FArcAttributeHandlerSharedFragment>();
			const UArcAttributeHandlerConfig* HandlerConfig = SharedHandler ? SharedHandler->Config : nullptr;

			TArray<FArcPendingAttributeOp> Ops = MoveTemp(PendingFrag->PendingOps);
			PendingFrag->PendingOps.Reset();

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

				FStructView FragmentView = EntityManager.GetMutableSparseElementDataForEntity(PendingOp.Attribute.FragmentType, EntityHandle);
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
					ARCMA_VLOG(EntityManager.GetWorld(), Verbose,
						TEXT("[Attr] Op no-op attr=%s::%s entity=%s"),
						*PendingOp.Attribute.FragmentType->GetName(),
						*PendingOp.Attribute.PropertyName.ToString(),
						*EntityHandle.DebugGetDescription());
					continue;
				}

				ARCMA_VLOG(EntityManager.GetWorld(), Log,
					TEXT("[Attr] Op applied attr=%s::%s op=%d magnitude=%.4f old_base=%.4f new_base=%.4f entity=%s"),
					*PendingOp.Attribute.FragmentType->GetName(),
					*PendingOp.Attribute.PropertyName.ToString(),
					static_cast<int32>(PendingOp.Operation),
					PendingOp.Magnitude,
					OldBase,
					NewBase,
					*EntityHandle.DebugGetDescription());

				FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
				FArcAggregator* Aggregator = AggFrag ? AggFrag->FindAggregator(PendingOp.Attribute) : nullptr;

				float FinalValue = NewBase;
				if (Aggregator && !Aggregator->IsEmpty())
				{
					const FArcOwnedTagsFragment* TagsPtr = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
					const FGameplayTagContainer EmptyTags;
					FArcAggregationContext AggContext{EntityHandle, TagsPtr ? TagsPtr->Tags.GetTagContainer() : EmptyTags, EntityManager, NewBase};
					FinalValue = Aggregator->Evaluate(AggContext);
					ARCMA_VLOG(EntityManager.GetWorld(), Verbose,
						TEXT("[Attr] Re-signal AttributeRecalculate entity=%s reason=aggregator needed"),
						*EntityHandle.DebugGetDescription());
					EntitiesNeedingRecalc.AddUnique(EntityHandle);
				}

				FArcAttributeHandlerContext HandlerContext(
					EntityManager,
					&Ctx,
					EntityHandle,
					FragmentMemory,
					HandlerConfig,
					PendingOp.Attribute,
					PendingOp.SourceEntity,
					PendingOp.CapturedSourceTags,
					PendingOp.CapturedTargetTags);

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
					ArcAttributes::RecalculateLiveDependents(EntityManager, &Ctx, EntityHandle, HandlerConfig,
					                                         PendingOp.Attribute, PendingAttributeEvents);
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
