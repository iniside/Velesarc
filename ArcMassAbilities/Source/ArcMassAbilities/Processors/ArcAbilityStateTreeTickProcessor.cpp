// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcAbilityStateTreeTickProcessor.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/Schema/ArcAbilityContext.h"
#include "Events/ArcEffectEventSubsystem.h"
#include "Events/ArcPendingEvents.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Signals/ArcAbilitySignals.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "StateTree.h"

UArcAbilityStateTreeTickProcessor::UArcAbilityStateTreeTickProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = FName(TEXT("Tasks"));
	ExecutionOrder.ExecuteAfter.Add(UArcAbilityInputProcessor::StaticClass()->GetFName());
}

void UArcAbilityStateTreeTickProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired);

	AbilitySubsystem = UWorld::GetSubsystem<UArcAbilityStateTreeSubsystem>(Owner.GetWorld());
}

void UArcAbilityStateTreeTickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcAbilityCollectionFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcAbilityInputFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcOwnedTagsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
}

void UArcAbilityStateTreeTickProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	if (!AbilitySubsystem)
	{
		return;
	}

	UArcEffectEventSubsystem* EventSubsystem = Context.GetWorld()->GetSubsystem<UArcEffectEventSubsystem>();
	if (!EventSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcAbilityStateTreeTickProcessor);

	const double CurrentTime = Context.GetWorld()->GetTimeSeconds();

	TArray<FArcPendingTagEvent> PendingTagEvents;

	EntityQuery.ForEachEntityChunk(Context,
		[this, &EntityManager, CurrentTime, &PendingTagEvents](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		TArrayView<FArcAbilityCollectionFragment> Collections = ChunkContext.GetMutableFragmentView<FArcAbilityCollectionFragment>();
		TArrayView<FArcAbilityInputFragment> Inputs = ChunkContext.GetMutableFragmentView<FArcAbilityInputFragment>();
		TArrayView<FArcOwnedTagsFragment> OwnedTagsView = ChunkContext.GetMutableFragmentView<FArcOwnedTagsFragment>();
		TArrayView<FArcEffectStackFragment> EffectStacks = ChunkContext.GetMutableFragmentView<FArcEffectStackFragment>();
		const bool bHasEffectStacks = EffectStacks.Num() > 0;

		for (int32 EntityIdx = 0; EntityIdx < NumEntities; ++EntityIdx)
		{
			FArcAbilityCollectionFragment& Collection = Collections[EntityIdx];
			FArcAbilityInputFragment& Input = Inputs[EntityIdx];
			FArcOwnedTagsFragment& OwnedTags = OwnedTagsView[EntityIdx];
			FArcEffectStackFragment* EffectStack = bHasEffectStacks ? &EffectStacks[EntityIdx] : nullptr;
			const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIdx);

			for (FArcActiveAbility& Ability : Collection.Abilities)
			{
				if (!Ability.bIsActive || !Ability.Definition || !Ability.Definition->StateTree
					|| Ability.RunStatus != EStateTreeRunStatus::Running)
				{
					continue;
				}

				FStateTreeInstanceData* InstanceData = AbilitySubsystem->GetInstanceData(Ability.InstanceHandle);
				if (!InstanceData)
				{
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
				ExecContext.SetContextRequirements(OwnedTags, EffectStack, Input, AbilityContext, Ability.SourceData);
				Ability.RunStatus = ExecContext.Tick(DeltaTime);
			}

			for (FArcActiveAbility& Ability : Collection.Abilities)
			{
				if (Ability.bIsActive
					&& (Ability.RunStatus == EStateTreeRunStatus::Succeeded
						|| Ability.RunStatus == EStateTreeRunStatus::Failed))
				{
					if (Ability.Definition)
					{
						OwnedTags.RemoveTags(Ability.Definition->GrantTagsWhileActive, PendingTagEvents, Entity);
					}

					if (Ability.InstanceHandle.IsValid())
					{
						FStateTreeInstanceData* InstanceData = AbilitySubsystem->GetInstanceData(Ability.InstanceHandle);
						if (InstanceData && Ability.Definition && Ability.Definition->StateTree)
						{
							FArcAbilityExecutionContext StopContext(*AbilitySubsystem, *Ability.Definition->StateTree, *InstanceData, ChunkContext);
							StopContext.SetEntity(Entity);
							StopContext.Stop();
						}
						AbilitySubsystem->FreeInstanceData(Ability.InstanceHandle);
						Ability.InstanceHandle = FMassStateTreeInstanceHandle();
					}

					Ability.bIsActive = false;
				}
			}
		}
	});

	for (const FArcPendingTagEvent& Evt : PendingTagEvents)
	{
		EventSubsystem->BroadcastTagCountChanged(Evt.Entity, Evt.Tag, Evt.OldCount, Evt.NewCount);
	}
}
