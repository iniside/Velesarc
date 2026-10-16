// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcEffectInhibitionProcessor.h"
#include "Mass/ArcMassAbilitiesProcessorGroups.h"
#include "Attributes/ArcAttributeAggregatorProcessor.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComp_Inhibition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectTask.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "ArcMassAbilitiesLogs.h"

UArcEffectInhibitionProcessor::UArcEffectInhibitionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::ArcMassAbilities::ProcessorGroups::Tasks;
	ExecutionOrder.ExecuteBefore.Add(UArcAttributeAggregatorProcessor::StaticClass()->GetFName());
}

void UArcEffectInhibitionProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMassAbilities::Signals::AttributeRecalculate);
}

void UArcEffectInhibitionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadWrite);
	// FArcOwnedTagsFragment is sparse (Phase 2 conversion) — required for inhibition checks.
	EntityQuery.AddSparseRequirement<FArcOwnedTagsFragment>();
}

void UArcEffectInhibitionProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Context.GetWorld());

	EntityQuery.ForEachEntityChunk(Context,
		[SignalSubsystem](FMassExecutionContext& ChunkContext)
	{
		TArrayView<FArcEffectStackFragment> Stacks = ChunkContext.GetMutableFragmentView<FArcEffectStackFragment>();
		TArrayView<FArcAggregatorFragment> Aggregators = ChunkContext.GetMutableFragmentView<FArcAggregatorFragment>();

		for (FMassSparseEntityIterator EntityIt = ChunkContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			const int32 Idx = *EntityIt;
			FArcEffectStackFragment& Stack = Stacks[Idx];
			FArcAggregatorFragment& AggFrag = Aggregators[Idx];
			const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);

			const FArcOwnedTagsFragment* OwnedTagsPtr = ChunkContext.GetSparseElement<FArcOwnedTagsFragment>(EntityIt);
			const FArcOwnedTagsFragment& OwnedTags = *OwnedTagsPtr;

			bool bNeedRecalc = false;

			for (FArcActiveEffect& Effect : Stack.ActiveEffects)
			{
				const FArcEffectSpec* Spec = Effect.Spec.GetPtr<FArcEffectSpec>();
				const FArcEffectComp_Inhibition* InhibComp = ArcEffects::Private::FindInhibitionComponent(Effect);
				if (!InhibComp)
				{
					continue;
				}

				const bool bShouldInhibit = ArcEffects::Private::ShouldInhibitEffect(Effect, OwnedTags.Tags);

				if (bShouldInhibit && !Effect.bInhibited)
				{
					Effect.bInhibited = true;
					ArcEffects::Private::RebuildEffectMods(AggFrag, Effect);
					ArcEffects::Private::NotifyInhibitionChanged(Effect, true);
					bNeedRecalc = true;
					ARCMA_VLOG(ChunkContext.GetWorld(), Log,
						TEXT("[Effect] Inhibited definition=%s entity=%s inhibiting_tags=%s"),
						*Spec->Definition->GetName(),
						*Entity.DebugGetDescription(),
						*InhibComp->InhibitedByTags.ToString());
				}
				else if (!bShouldInhibit && Effect.bInhibited)
				{
					Effect.bInhibited = false;
					ArcEffects::Private::RebuildEffectMods(AggFrag, Effect);
					ArcEffects::Private::NotifyInhibitionChanged(Effect, false);
					bNeedRecalc = true;
					ARCMA_VLOG(ChunkContext.GetWorld(), Log,
						TEXT("[Effect] De-inhibited definition=%s entity=%s mods_restored=%d stacks=%d"),
						*Spec->Definition->GetName(),
						*Entity.DebugGetDescription(),
						Effect.ModifierHandles.Num(),
						Spec->Context.StackCount);
				}
			}

			if (bNeedRecalc && SignalSubsystem)
			{
				SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
			}
		}
	});
}
