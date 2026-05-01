// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcAbilityInputProcessor.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Abilities/ArcAbilityCooldown.h"
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

UArcAbilityInputProcessor::UArcAbilityInputProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = FName(TEXT("Tasks"));
}

void UArcAbilityInputProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMassAbilities::Signals::AbilityInputReceived);

	AbilitySubsystem = UWorld::GetSubsystem<UArcAbilityStateTreeSubsystem>(Owner.GetWorld());
}

void UArcAbilityInputProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcAbilityCollectionFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcAbilityInputFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcOwnedTagsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
}

void UArcAbilityInputProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcAbilityInputProcessor);

	const double CurrentTime = Context.GetWorld()->GetTimeSeconds();

	TArray<FArcPendingTagEvent> PendingTagEvents;
	TArray<FArcPendingAbilityEvent> PendingAbilityEvents;

	EntityQuery.ForEachEntityChunk(Context,
		[this, &EntityManager, CurrentTime, &PendingTagEvents, &PendingAbilityEvents](FMassExecutionContext& ChunkContext)
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

			TArray<int32, TInlineAllocator<4>> AbilitiesToActivate;
			for (const FGameplayTag& PressedTag : Input.PressedThisFrame)
			{
				for (int32 AbilityIdx = 0; AbilityIdx < Collection.Abilities.Num(); ++AbilityIdx)
				{
					FArcActiveAbility& Ability = Collection.Abilities[AbilityIdx];
					if (!Ability.bIsActive && Ability.InputTag == PressedTag && Ability.Definition)
					{
						AbilitiesToActivate.AddUnique(AbilityIdx);
					}
				}
			}

			for (const int32 AbilityIdx : AbilitiesToActivate)
			{
				FArcActiveAbility& Ability = Collection.Abilities[AbilityIdx];
				const UArcAbilityDefinition* Def = Ability.Definition;

				if (Def->ActivationRequiredTags.Num() > 0
					&& !OwnedTags.Tags.HasAll(Def->ActivationRequiredTags))
				{
					continue;
				}

				if (Def->ActivationBlockedTags.Num() > 0
					&& OwnedTags.Tags.HasAny(Def->ActivationBlockedTags))
				{
					continue;
				}

				bool bBlocked = false;
				for (const FArcActiveAbility& Active : Collection.Abilities)
				{
					if (!Active.bIsActive || !Active.Definition)
					{
						continue;
					}
					if (Active.Definition->BlockAbilitiesWithTags.Num() > 0
						&& Def->AbilityTags.HasAny(Active.Definition->BlockAbilitiesWithTags))
					{
						bBlocked = true;
						break;
					}
				}
				if (bBlocked)
				{
					continue;
				}

				if (Def->Cost.IsValid())
				{
					const FArcMassAbilityCost* CostPtr = Def->Cost.GetPtr<FArcMassAbilityCost>();
					if (CostPtr)
					{
						FArcMassAbilityCostContext CostCtx{EntityManager, Entity, Ability.Handle, Def};
						if (!CostPtr->CheckCost(CostCtx))
						{
							continue;
						}
					}
				}

				if (Def->Cooldown.IsValid())
				{
					const FArcAbilityCooldown* CooldownPtr = Def->Cooldown.GetPtr<FArcAbilityCooldown>();
					if (CooldownPtr && !CooldownPtr->CheckCooldown(EntityManager, Entity, Ability.Handle))
					{
						continue;
					}
				}

				if (Def->CancelAbilitiesWithTags.Num() > 0)
				{
					for (FArcActiveAbility& Other : Collection.Abilities)
					{
						if (&Other != &Ability && Other.bIsActive && Other.Definition
							&& Other.Definition->AbilityTags.HasAny(Def->CancelAbilitiesWithTags))
						{
							OwnedTags.RemoveTags(Other.Definition->GrantTagsWhileActive, PendingTagEvents, Entity);

							if (Other.InstanceHandle.IsValid())
							{
								FStateTreeInstanceData* OtherInstanceData = AbilitySubsystem->GetInstanceData(Other.InstanceHandle);
								if (OtherInstanceData)
								{
									FArcAbilityExecutionContext StopContext(*AbilitySubsystem, *Other.Definition->StateTree, *OtherInstanceData, ChunkContext);
									StopContext.SetEntity(Entity);
									StopContext.Stop();
								}
								AbilitySubsystem->FreeInstanceData(Other.InstanceHandle);
								Other.InstanceHandle = FMassStateTreeInstanceHandle();
							}

							Other.bIsActive = false;
							Other.RunStatus = EStateTreeRunStatus::Failed;
						}
					}
				}

				OwnedTags.AddTags(Def->GrantTagsWhileActive, PendingTagEvents, Entity);

				if (Def->StateTree)
				{
					if (!Ability.InstanceHandle.IsValid())
					{
						Ability.InstanceHandle = AbilitySubsystem->AllocateInstanceData(Def->StateTree);
					}

					FStateTreeInstanceData* InstanceData = AbilitySubsystem->GetInstanceData(Ability.InstanceHandle);
					if (InstanceData)
					{
						FArcAbilityContext AbilityContext;
						AbilityContext.Handle = Ability.Handle;
						AbilityContext.Definition = Ability.Definition;
						AbilityContext.SourceEntity = Ability.SourceEntity;

						FArcAbilityExecutionContext ExecContext(*AbilitySubsystem, *Def->StateTree, *InstanceData, ChunkContext);
						ExecContext.SetEntity(Entity);
						ExecContext.SetAbilityHandle(Ability.Handle);
						ExecContext.SetContextRequirements(OwnedTags, EffectStack, Input, AbilityContext, Ability.SourceData);
						Ability.RunStatus = ExecContext.Start();
						Ability.LastUpdateTimeInSeconds = CurrentTime;

						if (Ability.RunStatus == EStateTreeRunStatus::Running)
						{
							Ability.RunStatus = ExecContext.Tick(0.f);
						}
					}
				}

				Ability.bIsActive = true;
				PendingAbilityEvents.Add(FArcPendingAbilityEvent{Entity, const_cast<UArcAbilityDefinition*>(Ability.Definition.Get())});
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

			Input.PressedThisFrame.Reset();
			Input.ReleasedThisFrame.Reset();
		}
	});

	for (const FArcPendingTagEvent& Evt : PendingTagEvents)
	{
		EventSubsystem->BroadcastTagCountChanged(Evt.Entity, Evt.Tag, Evt.OldCount, Evt.NewCount);
	}

	for (const FArcPendingAbilityEvent& Evt : PendingAbilityEvents)
	{
		EventSubsystem->BroadcastAbilityActivated(Evt.Entity, Evt.Ability);
	}
}
