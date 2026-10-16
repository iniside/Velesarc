// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcAbilityInputProcessor.h"
#include "Mass/ArcMassAbilitiesProcessorGroups.h"
#include "ArcMassAbilitiesLogs.h"
#include "Abilities/ArcAbilityActivationRequirement.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/Schema/ArcAbilityContext.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Events/ArcPendingEvents.h"
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

UArcAbilityInputProcessor::UArcAbilityInputProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteInGroup = UE::ArcMassAbilities::ProcessorGroups::Tasks;
}

void UArcAbilityInputProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMassAbilities::Signals::AbilityInputReceived);

	AbilitySubsystem = UWorld::GetSubsystem<UArcAbilityStateTreeSubsystem>(Owner.GetWorld());
}

void UArcAbilityInputProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// Anchor on FArcAggregatorFragment — a regular fragment present on all UArcAbilitiesTrait entities.
	// FArcAbilityCollectionFragment and FArcAbilityInputFragment are now sparse and may be seeded
	// post-Commit (e.g. deferred command, test AddSparseElementToEntity). Using AddSparseRequirement
	// + CreateSparseEntityIterator would skip those entities. Resolve per entity via GetSparseFragmentDataPtr
	// with a null-check as the filter instead.
	// Note: entities using only UArcAbilityGrantTrait (no UArcAbilitiesTrait) have no regular ability-
	// system fragments and will not be iterated — this is accepted scope.
	EntityQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
}

void UArcAbilityInputProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	if (!AbilitySubsystem)
	{
		return;
	}

	UArcGameEventSubsystem* EventBus = Context.GetWorld()->GetSubsystem<UArcGameEventSubsystem>();
	if (!EventBus)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcAbilityInputProcessor);

	const double CurrentTime = Context.GetWorld()->GetTimeSeconds();

	TArray<FArcPendingAbilityEvent> PendingAbilityEvents;

	EntityQuery.ForEachEntityChunk(Context,
		[this, &EntityManager, CurrentTime, &PendingAbilityEvents, EventBus](FMassExecutionContext& ChunkContext)
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

			FArcOwnedTagsFragment* OwnedTagsPtr = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
			if (!OwnedTagsPtr)
			{
				continue;
			}
			FArcOwnedTagsFragment& OwnedTags = *OwnedTagsPtr;

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
				Ability.OriginTags = Input.PressedOriginTags.FindRef(Ability.InputTag);

				if (Def->ActivationRequiredTags.Num() > 0
					&& !OwnedTags.Tags.HasAll(Def->ActivationRequiredTags))
				{
					ARCMA_VLOG(ChunkContext.GetWorld(), Warning,
						TEXT("[Input] Activation blocked: required tags missing ability=%s missing=%s entity=%s"),
						*Def->GetName(),
						*Def->ActivationRequiredTags.ToString(),
						*Entity.DebugGetDescription());
					continue;
				}

				if (Def->ActivationBlockedTags.Num() > 0
					&& OwnedTags.Tags.HasAny(Def->ActivationBlockedTags))
				{
					ARCMA_VLOG(ChunkContext.GetWorld(), Warning,
						TEXT("[Input] Activation blocked: blocked tags present ability=%s blocking=%s entity=%s"),
						*Def->GetName(),
						*Def->ActivationBlockedTags.ToString(),
						*Entity.DebugGetDescription());
					continue;
				}

				bool bBlocked = false;
				const FArcActiveAbility* BlockingAbility = nullptr;
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
						BlockingAbility = &Active;
						break;
					}
				}
				if (bBlocked)
				{
					ARCMA_VLOG(ChunkContext.GetWorld(), Warning,
						TEXT("[Input] Activation blocked: ability=%s blocked by active ability=%s entity=%s"),
						*Def->GetName(),
						BlockingAbility && BlockingAbility->Definition ? *BlockingAbility->Definition->GetName() : TEXT("unknown"),
						*Entity.DebugGetDescription());
					continue;
				}

				if (!Def->bGateInStateTree)
				{
					if (Def->Cost.IsValid())
					{
						const FArcMassAbilityCost* CostPtr = Def->Cost.GetPtr<FArcMassAbilityCost>();
						if (CostPtr)
						{
							FArcMassAbilityCostContext CostCtx{EntityManager, Entity, Ability.Handle, Def};
							FArcCostCheckResult CostResult;
							if (!CostPtr->CheckCost(CostCtx, &CostResult))
							{
								ARCMA_VLOG(ChunkContext.GetWorld(), Warning,
									TEXT("[Cost] Cost insufficient ability=%s entity=%s"),
									*Def->GetName(),
									*Entity.DebugGetDescription());
								ArcAbilities::Private::QueueCostFailed(*EventBus, EntityManager, Entity, Ability.Handle, CostResult);
								continue;
							}
						}
					}
				}

				if (!Def->bGateInStateTree)
				{
					if (Def->Cooldown.IsValid())
					{
						const FArcAbilityCooldown* CooldownPtr = Def->Cooldown.GetPtr<FArcAbilityCooldown>();
						if (CooldownPtr)
						{
							FArcCooldownCheckResult CdResult;
							if (!CooldownPtr->CheckCooldown(EntityManager, Entity, Ability.Handle, &CdResult))
							{
								ARCMA_VLOG(ChunkContext.GetWorld(), Warning,
									TEXT("[Cooldown] Cooldown active ability=%s entity=%s"),
									*Def->GetName(),
									*Entity.DebugGetDescription());
								ArcAbilities::Private::QueueCooldownFailed(*EventBus, EntityManager, Entity, Ability.Handle, CdResult);
								continue;
							}
						}
					}
				}

				bool bRequirementBlocked = false;
				for (const FInstancedStruct& RequirementStruct : Def->ActivationRequirements)
				{
					const FArcAbilityActivationRequirement* Requirement = RequirementStruct.GetPtr<FArcAbilityActivationRequirement>();
					if (Requirement != nullptr)
					{
						FArcAbilityActivationContext ReqCtx{EntityManager, Entity, Ability.Handle, Def, EntityView};
						if (!Requirement->Check(ReqCtx))
						{
							ARCMA_VLOG(ChunkContext.GetWorld(), Warning,
								TEXT("[Activation] Requirement blocked ability=%s entity=%s"),
								*Def->GetName(),
								*Entity.DebugGetDescription());
							bRequirementBlocked = true;
							break;
						}
					}
				}
				if (bRequirementBlocked)
				{
					continue;
				}

				if (Def->CancelAbilitiesWithTags.Num() > 0)
				{
					for (FArcActiveAbility& Other : Collection.Abilities)
					{
						if (&Other != &Ability && Other.bIsActive && Other.Definition
							&& Other.Definition->AbilityTags.HasAny(Def->CancelAbilitiesWithTags))
						{
							ARCMA_VLOG(ChunkContext.GetWorld(), Log,
								TEXT("[Ability] Cancelled ability=%s by ability=%s entity=%s"),
								*Other.Definition->GetName(),
								*Def->GetName(),
								*Entity.DebugGetDescription());
							ArcAbilities::Private::CancelActiveAbility(EntityManager, Entity, Other, *AbilitySubsystem, ChunkContext,
								EArcAbilityEndReason::Cancelled, Entity);
						}
					}
				}

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
						ExecContext.SetContextRequirements(Entity, EntityManager, EffectStack, AbilityContext, Ability.CustomData.GetPrimaryMutable());

						Ability.bIsActive = true;                   // Set before Start() so EnterState observes the ability as active.
						Ability.RunStatus = ExecContext.Start();

						if (Ability.RunStatus == EStateTreeRunStatus::Failed)
						{
							// Rollback: do not grant tags, do not emit event, free instance.
							Ability.bIsActive = false;
							AbilitySubsystem->FreeInstanceData(Ability.InstanceHandle);
							Ability.InstanceHandle = FMassStateTreeInstanceHandle();
							ARCMA_VLOG(ChunkContext.GetWorld(), Warning,
								TEXT("[Ability] Activation failed in Start() ability=%s entity=%s"),
								*Def->GetName(), *Entity.DebugGetDescription());
							continue;
						}

						if (Ability.RunStatus == EStateTreeRunStatus::Succeeded)
						{
							// Tree completed synchronously inside Start() — ExitState already ran.
							// Do NOT grant GrantTagsWhileActive (ability was never persistently active).
							// Emit Activated + Ended so listeners see the full lifecycle, then tear down.
							ArcAbilities::Private::QueueAbilityActivated(*EventBus, EntityManager, Entity, Ability.Handle);
							ArcAbilities::Private::QueueAbilityEnded(EntityManager, Entity, Ability, EArcAbilityEndReason::Completed, FMassEntityHandle());
							AbilitySubsystem->FreeInstanceData(Ability.InstanceHandle);
							Ability.InstanceHandle = FMassStateTreeInstanceHandle();
							Ability.bIsActive = false;
							ARCMA_VLOG(ChunkContext.GetWorld(), Log,
								TEXT("[Ability] Completed synchronously in Start() ability=%s entity=%s"),
								*Def->GetName(), *Entity.DebugGetDescription());
							continue;
						}

						Ability.LastUpdateTimeInSeconds = CurrentTime;

						// Read the schedule the first EnterState may have requested via AddScheduledTickRequest.
						// The delayed signal we enqueue here is for the *future scheduled wake*, NOT a re-dispatch
						// of the current input event — Start() already handled the input event inline. The plugin
						// rule ("don't fire AbilityStateTreeTickRequired from input dispatch") targets duplicate
						// dispatch of the same input, not future-wake scheduling, so this is rule-compliant.
						if (Ability.RunStatus == EStateTreeRunStatus::Running)
						{
							const FStateTreeScheduledTick Sched = ExecContext.GetNextScheduledTick();
							if (Sched.HasCustomTickRate())
							{
								const float Delay = Sched.GetTickRate();
								SignalSubsystem->DelaySignalEntityDeferred(ChunkContext, UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity, Delay);
								Ability.NextWakeTimeInSeconds = CurrentTime + Delay;
							}
							else if (Sched.ShouldTickEveryFrames() || Sched.ShouldTickOnceNextFrame())
							{
								SignalSubsystem->SignalEntityDeferred(ChunkContext, UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
								Ability.NextWakeTimeInSeconds = CurrentTime;
							}
							// Sleep → no signal, no wake outstanding
						}
					}
					else
					{
						// No instance data — treat as failure, do not grant tags or fire event.
						ARCMA_VLOG(ChunkContext.GetWorld(), Warning,
							TEXT("[Ability] Activation failed: no instance data ability=%s entity=%s"),
							*Def->GetName(), *Entity.DebugGetDescription());
						continue;
					}
				}

				// Success: grant tags, log activated, emit event.
				// For abilities without a StateTree, bIsActive is set here (first time).
				// For abilities with a StateTree, bIsActive was already set to true before Start().
				if (!Def->StateTree)
				{
					Ability.bIsActive = true;
				}
				ArcAbilities::AddOwnedTags(EntityManager, Entity, Def->GrantTagsWhileActive);
				ARCMA_VLOG(ChunkContext.GetWorld(), Log,
					TEXT("[Ability] Activated ability=%s entity=%s"),
					*Def->GetName(),
					*Entity.DebugGetDescription());
				PendingAbilityEvents.Add(FArcPendingAbilityEvent{Entity, Ability.Handle});
			}

			Input.PressedThisFrame.Reset();
			Input.ReleasedThisFrame.Reset();
			Input.PressedOriginTags.Reset();
		}
	});

	for (const FArcPendingAbilityEvent& Evt : PendingAbilityEvents)
	{
		ArcAbilities::Private::QueueAbilityActivated(*EventBus, EntityManager, Evt.Entity, Evt.Handle);
	}
}
