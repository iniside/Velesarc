// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcAbilityStateTreeTickProcessor.h"
#include "Mass/ArcMassAbilitiesProcessorGroups.h"
#include "ArcMassAbilitiesLogs.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/Schema/ArcAbilityContext.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAggregator.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Signals/ArcAbilitySignals.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "StateTree.h"

UArcAbilityStateTreeTickProcessor::UArcAbilityStateTreeTickProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::ArcMassAbilities::ProcessorGroups::Tasks;
	ExecutionOrder.ExecuteAfter.Add(UArcAbilityInputProcessor::StaticClass()->GetFName());
}

void UArcAbilityStateTreeTickProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired);

	AbilitySubsystem = UWorld::GetSubsystem<UArcAbilityStateTreeSubsystem>(Owner.GetWorld());
}

void UArcAbilityStateTreeTickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// Anchor on FArcAggregatorFragment — a regular fragment present on all UArcAbilitiesTrait entities.
	// FArcAbilityCollectionFragment and FArcAbilityInputFragment are now sparse and may be seeded
	// post-Commit; AddSparseRequirement + CreateSparseEntityIterator would skip those entities.
	// Resolve per entity via GetSparseFragmentDataPtr with a null-check as the filter instead.
	EntityQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
}

void UArcAbilityStateTreeTickProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	if (!AbilitySubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcAbilityStateTreeTickProcessor);

	const double CurrentTime = Context.GetWorld()->GetTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context,
		[this, &EntityManager, CurrentTime](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		TArrayView<FArcEffectStackFragment> EffectStacks = ChunkContext.GetMutableFragmentView<FArcEffectStackFragment>();
		const bool bHasEffectStacks = EffectStacks.Num() > 0;

		for (int32 EntityIdx = 0; EntityIdx < NumEntities; ++EntityIdx)
		{
			FArcEffectStackFragment* EffectStack = bHasEffectStacks ? &EffectStacks[EntityIdx] : nullptr;
			const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIdx);
			FMassEntityView EntityView(EntityManager, Entity);

			FArcAbilityCollectionFragment* CollectionPtr = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
			FArcAbilityInputFragment* InputPtr = EntityView.GetSparseFragmentDataPtr<FArcAbilityInputFragment>();
			if (!CollectionPtr || !InputPtr)
			{
				continue;
			}
			FArcAbilityCollectionFragment& Collection = *CollectionPtr;
			FArcAbilityInputFragment& Input = *InputPtr;

			if (!EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>())
			{
				continue;
			}

			for (FArcActiveAbility& Ability : Collection.Abilities)
			{
				if (!Ability.bIsActive || !Ability.Definition || !Ability.Definition->StateTree
					|| Ability.RunStatus != EStateTreeRunStatus::Running)
				{
					if (Ability.Definition)
					{
						ARCMA_VLOG(ChunkContext.GetWorld(), Verbose,
							TEXT("[Ability] Skip tick ability=%s reason=not_running entity=%s"),
							*Ability.Definition->GetName(),
							*Entity.DebugGetDescription());
					}
					continue;
				}

				FStateTreeInstanceData* InstanceData = AbilitySubsystem->GetInstanceData(Ability.InstanceHandle);
				if (!InstanceData)
				{
					ARCMA_VLOG(ChunkContext.GetWorld(), Warning,
						TEXT("[Ability] Active ability has null instance data ability=%s entity=%s"),
						*Ability.Definition->GetName(),
						*Entity.DebugGetDescription());
					continue;
				}

				const float DeltaTime = static_cast<float>(CurrentTime - Ability.LastUpdateTimeInSeconds);
				Ability.LastUpdateTimeInSeconds = CurrentTime;

				FArcAbilityContext AbilityContext;
				AbilityContext.Handle = Ability.Handle;
				AbilityContext.Definition = Ability.Definition;
				AbilityContext.SourceEntity = Ability.SourceEntity;

				FArcAbilityExecutionContext ExecContext(*AbilitySubsystem, *Ability.Definition->StateTree, *InstanceData, ChunkContext);
				ExecContext.SetEntity(Entity);
				ExecContext.SetAbilityHandle(Ability.Handle);
				ExecContext.SetContextRequirements(Entity, EntityManager, EffectStack, AbilityContext, Ability.CustomData.GetPrimaryMutable());
				Ability.RunStatus = ExecContext.Tick(DeltaTime);
				ARCMA_VLOG(ChunkContext.GetWorld(), Verbose,
					TEXT("[Ability] Tick result ability=%s status=%d entity=%s"),
					*Ability.Definition->GetName(),
					static_cast<int32>(Ability.RunStatus),
					*Entity.DebugGetDescription());
			}

			// Pass 1.5 — read scheduled tick for abilities still running; enqueue Mass signal accordingly.
			// Runs after Pass 1 (Tick) and before Pass 2 (teardown). Only walks Running abilities.
			for (FArcActiveAbility& Ability : Collection.Abilities)
			{
				if (!Ability.bIsActive || Ability.RunStatus != EStateTreeRunStatus::Running
					|| !Ability.Definition || !Ability.Definition->StateTree)
				{
					continue;
				}

				FStateTreeInstanceData* InstanceData = AbilitySubsystem->GetInstanceData(Ability.InstanceHandle);
				if (!InstanceData) { continue; }

				// Reset before recompute — a stale sentinel from the prior wake would suppress this one.
				Ability.NextWakeTimeInSeconds = -1.0;

				FStateTreeReadOnlyExecutionContext ScheduleContext(AbilitySubsystem, Ability.Definition->StateTree, *InstanceData);
				const FStateTreeScheduledTick Sched = ScheduleContext.GetNextScheduledTick();

				if (Sched.ShouldSleep())
				{
					continue;
				}

				if (Sched.ShouldTickEveryFrames() || Sched.ShouldTickOnceNextFrame())
				{
					SignalSubsystem->SignalEntityDeferred(ChunkContext, UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
					Ability.NextWakeTimeInSeconds = CurrentTime;
					continue;
				}

				if (Sched.HasCustomTickRate())
				{
					const float Delay = Sched.GetTickRate();
					SignalSubsystem->DelaySignalEntityDeferred(ChunkContext, UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity, Delay);
					Ability.NextWakeTimeInSeconds = CurrentTime + Delay;
				}
			}

			for (FArcActiveAbility& Ability : Collection.Abilities)
			{
				if (Ability.bIsActive
					&& (Ability.RunStatus == EStateTreeRunStatus::Succeeded
						|| Ability.RunStatus == EStateTreeRunStatus::Failed))
				{
					if (Ability.Definition)
					{
						const EArcAbilityEndReason EndReason = (Ability.RunStatus == EStateTreeRunStatus::Succeeded)
							? EArcAbilityEndReason::Completed
							: EArcAbilityEndReason::Failed;
						ARCMA_VLOG(ChunkContext.GetWorld(), Log,
							TEXT("[Ability] Ended ability=%s reason=%s entity=%s"),
							*Ability.Definition->GetName(),
							(Ability.RunStatus == EStateTreeRunStatus::Succeeded) ? TEXT("Succeeded") : TEXT("Failed"),
							*Entity.DebugGetDescription());
						ArcAbilities::Private::QueueAbilityEnded(EntityManager, Entity, Ability, EndReason, FMassEntityHandle());
						ArcAbilities::RemoveOwnedTags(EntityManager, Entity, Ability.Definition->GrantTagsWhileActive);
					}

					ArcAbilities::Private::StopAndFreeAbilityTree(Ability, *AbilitySubsystem, Entity, ChunkContext);

					Ability.bIsActive = false;
					Ability.NextWakeTimeInSeconds = -1.0;
				}
			}
		}
	});
}
