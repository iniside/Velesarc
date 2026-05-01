// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionReactionProcessor.h"

#include "ArcConditionAttributeSet.h"
#include "ArcConditionEffectsConfig.h"
#include "ArcConditionEffectsSubsystem.h"
#include "ArcConditionFragments.h"
#include "ArcConditionGameplayTags.h"
#include "GameplayEffect.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Player/ArcHeroComponentBase.h"
#include "ArcConditionMassEffectsConfig.h"
#include "ArcConditionSaturationAttributes.h"
#include "ArcConditionSaturationHandler.h"
#include "Effects/ArcEffectFunctions.h"
#include "Fragments/ArcEffectStackFragment.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UArcConditionReactionProcessor::UArcConditionReactionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

// ---------------------------------------------------------------------------
// InitializeInternal
// ---------------------------------------------------------------------------

void UArcConditionReactionProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::ConditionStateChanged);
	SubscribeToSignal(*SignalSubsystem, UE::ArcConditionEffects::Signals::ConditionOverloadChanged);
}

// ---------------------------------------------------------------------------
// ConfigureQueries
// ---------------------------------------------------------------------------

void UArcConditionReactionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// ASC fragment — optional (entities without ASC still get prev-state tracking)
	EntityQuery.AddRequirement<FArcCoreAbilitySystemFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

	// Consolidated condition states fragment — optional, read-only
	EntityQuery.AddRequirement<FArcConditionStatesFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	
	EntityQuery.AddRequirement<FArcConditionSaturationAttributes>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);

	EntityQuery.AddSubsystemRequirement<UArcConditionEffectsSubsystem>(EMassFragmentAccess::ReadWrite);
}

// ---------------------------------------------------------------------------
// EnsureTagContainersBuilt
// ---------------------------------------------------------------------------

void UArcConditionReactionProcessor::EnsureTagContainersBuilt()
{
	if (bTagContainersBuilt)
	{
		return;
	}

	for (int32 i = 0; i < ArcConditionTypeCount; ++i)
	{
		const EArcConditionType Type = static_cast<EArcConditionType>(i);
		ActiveTagContainers[i].AddTag(ArcConditionTags::GetActiveTag(Type));
		OverloadTagContainers[i].AddTag(ArcConditionTags::GetOverloadTag(Type));
	}

	bTagContainersBuilt = true;
}

// ---------------------------------------------------------------------------
// SignalEntities
// ---------------------------------------------------------------------------

void UArcConditionReactionProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	EnsureTagContainersBuilt();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcConditionReaction);

	UArcConditionEffectsSubsystem* Subsystem = UWorld::GetSubsystem<UArcConditionEffectsSubsystem>(EntityManager.GetWorld());
	UArcConditionMassEffectsConfig* MassConfig = Subsystem ? Subsystem->GetMassEffectsConfig() : nullptr;

	EntityQuery.ForEachEntityChunk(Context,
		[this, &EntityManager, Subsystem, MassConfig](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FArcCoreAbilitySystemFragment> ASCFragments = Ctx.GetFragmentView<FArcCoreAbilitySystemFragment>();
			const bool bHasASC = !ASCFragments.IsEmpty();

			TArrayView<FArcConditionSaturationAttributes> SatAttrFragments = Ctx.GetMutableFragmentView<FArcConditionSaturationAttributes>();
			const bool bHasSatAttrs = !SatAttrFragments.IsEmpty();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const int32 Idx = *EntityIt;
				const FMassEntityHandle Entity = Ctx.GetEntity(Idx);

				// Get or create prev-state cache entry
				TStaticArray<FArcConditionPrevState, ArcConditionTypeCount>& EntityPrevStates = PrevStates.FindOrAdd(Entity);

				// Get ASC and ConditionSet if present
				UArcCoreAbilitySystemComponent* ASC = nullptr;
				const UArcConditionAttributeSet* ConditionSet = nullptr;

				if (bHasASC)
				{
					ASC = ASCFragments[Idx].AbilitySystem.Get();
					if (ASC)
					{
						ConditionSet = ASC->GetSet<UArcConditionAttributeSet>();
					}
				}

				FArcConditionSaturationAttributes* SatAttrs = bHasSatAttrs ? &SatAttrFragments[Idx] : nullptr;

				// Process all 13 conditions
				TConstArrayView<FArcConditionStatesFragment> CondFrags = Ctx.GetFragmentView<FArcConditionStatesFragment>();
				const FArcConditionStatesFragment* CondFrag = !CondFrags.IsEmpty() ? &CondFrags[Idx] : nullptr;

				for (int32 i = 0; i < ArcConditionTypeCount; ++i)
				{
					const EArcConditionType Type = static_cast<EArcConditionType>(i);
					const FArcConditionState* State = CondFrag ? &CondFrag->States[i] : nullptr;
					const FArcConditionPrevState SnapshotPrev = EntityPrevStates[i];

					ProcessConditionTransition(Type, State, EntityPrevStates[i], Entity, ASC, Subsystem);

					if (MassConfig)
					{
						const FArcConditionMassEffectEntry* MassEntry = MassConfig->ConditionEffects.Find(Type);
						ProcessMassConditionTransition(Type, State, SnapshotPrev, Entity, EntityManager, MassEntry);
					}

					const float Sat = State ? State->Saturation : 0.f;

					if (ConditionSet && ASC)
					{
						ASC->SetNumericAttributeBase(UArcConditionAttributeSet::GetSaturationAttributeByIndex(i), Sat);
					}

					if (SatAttrs)
					{
						SatAttrs->GetSaturationByIndex(i)->SetBaseValue(Sat);
					}
				}
			}
		}
	);

	// Prune entries for entities that are no longer valid
	for (auto It = PrevStates.CreateIterator(); It; ++It)
	{
		if (!EntityManager.IsEntityValid(It->Key))
		{
			It.RemoveCurrent();
		}
	}
}

// ---------------------------------------------------------------------------
// ProcessConditionTransition
// ---------------------------------------------------------------------------

void UArcConditionReactionProcessor::ProcessConditionTransition(
	EArcConditionType ConditionType,
	const FArcConditionState* CurrentState,
	FArcConditionPrevState& PrevState,
	FMassEntityHandle Entity,
	UArcCoreAbilitySystemComponent* ASC,
	UArcConditionEffectsSubsystem* Subsystem)
{
	// Treat absent fragment as inactive defaults
	const bool bIsActive = CurrentState ? CurrentState->bActive : false;
	const float Saturation = CurrentState ? CurrentState->Saturation : 0.f;
	const EArcConditionOverloadPhase OverloadPhase = CurrentState ? CurrentState->OverloadPhase : EArcConditionOverloadPhase::None;

	const bool bWasActive = PrevState.bActive;
	const float PrevSaturation = PrevState.Saturation;
	const EArcConditionOverloadPhase PrevOverloadPhase = PrevState.OverloadPhase;

	const int32 TypeIdx = static_cast<int32>(ConditionType);

	// Look up effect config
	const FArcConditionEffectEntry* EffectEntry = nullptr;
	if (Subsystem)
	{
		if (UArcConditionEffectsConfig* Config = Subsystem->GetEffectsConfig())
		{
			EffectEntry = Config->ConditionEffects.Find(ConditionType);
		}
	}

	// --- Activation: !wasActive -> isActive ---
	if (!bWasActive && bIsActive)
	{
		if (Subsystem)
		{
			Subsystem->OnConditionActivated.Broadcast(Entity, ConditionType, Saturation);
		}

		if (ASC && EffectEntry && EffectEntry->ActivationEffect)
		{
			ApplyEffectToASC(ASC, EffectEntry->ActivationEffect, Saturation);
		}
	}

	// --- Deactivation: wasActive -> !isActive ---
	if (bWasActive && !bIsActive)
	{
		if (Subsystem)
		{
			Subsystem->OnConditionDeactivated.Broadcast(Entity, ConditionType, Saturation);
		}

		if (ASC)
		{
			RemoveEffectsWithTags(ASC, ActiveTagContainers[TypeIdx]);
		}
	}

	// --- Break: saturation crossed 100 threshold ---
	if (PrevSaturation < 100.f && Saturation >= 100.f)
	{
		if (Subsystem)
		{
			Subsystem->OnConditionBreak.Broadcast(Entity, ConditionType, Saturation);
		}

		if (ASC && EffectEntry && EffectEntry->BurstEffect)
		{
			ApplyEffectToASC(ASC, EffectEntry->BurstEffect, Saturation);
		}
	}

	// --- Overload transitions ---
	if (PrevOverloadPhase != OverloadPhase)
	{
		// None -> Overloaded: apply overload effect
		if (PrevOverloadPhase == EArcConditionOverloadPhase::None && OverloadPhase == EArcConditionOverloadPhase::Overloaded)
		{
			if (Subsystem)
			{
				Subsystem->OnConditionOverloadChanged.Broadcast(Entity, ConditionType, OverloadPhase);
			}

			if (ASC && EffectEntry && EffectEntry->OverloadEffect)
			{
				ApplyEffectToASC(ASC, EffectEntry->OverloadEffect, Saturation);
			}
		}
		// Overloaded -> Burnout/None or Burnout -> None: remove overload effects
		else if (PrevOverloadPhase == EArcConditionOverloadPhase::Overloaded || PrevOverloadPhase == EArcConditionOverloadPhase::Burnout)
		{
			if (Subsystem)
			{
				Subsystem->OnConditionOverloadChanged.Broadcast(Entity, ConditionType, OverloadPhase);
			}

			if (ASC)
			{
				RemoveEffectsWithTags(ASC, OverloadTagContainers[TypeIdx]);
			}
		}
	}

	// Update prev state
	PrevState.bActive = bIsActive;
	PrevState.Saturation = Saturation;
	PrevState.OverloadPhase = OverloadPhase;
}

// ---------------------------------------------------------------------------
// ApplyEffectToASC
// ---------------------------------------------------------------------------

void UArcConditionReactionProcessor::ApplyEffectToASC(UArcCoreAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> EffectClass, float Saturation)
{
	if (!ASC || !EffectClass)
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.f, ASC->MakeEffectContext());
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(ArcConditionTags::SetByCaller_Saturation, Saturation);
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

// ---------------------------------------------------------------------------
// RemoveEffectsWithTags
// ---------------------------------------------------------------------------

void UArcConditionReactionProcessor::RemoveEffectsWithTags(UArcCoreAbilitySystemComponent* ASC, const FGameplayTagContainer& TagContainer)
{
	if (!ASC)
	{
		return;
	}

	ASC->RemoveActiveEffectsWithTags(TagContainer);
}

// ---------------------------------------------------------------------------
// ProcessMassConditionTransition
// ---------------------------------------------------------------------------

void UArcConditionReactionProcessor::ProcessMassConditionTransition(
	EArcConditionType ConditionType,
	const FArcConditionState* CurrentState,
	const FArcConditionPrevState& PrevState,
	FMassEntityHandle Entity,
	FMassEntityManager& EntityManager,
	const FArcConditionMassEffectEntry* MassEntry)
{
	const bool bIsActive = CurrentState ? CurrentState->bActive : false;
	const float Saturation = CurrentState ? CurrentState->Saturation : 0.f;
	const EArcConditionOverloadPhase OverloadPhase = CurrentState ? CurrentState->OverloadPhase : EArcConditionOverloadPhase::None;

	const bool bWasActive = PrevState.bActive;
	const float PrevSaturation = PrevState.Saturation;
	const EArcConditionOverloadPhase PrevOverloadPhase = PrevState.OverloadPhase;

	const int32 TypeIdx = static_cast<int32>(ConditionType);

	// Activation
	if (!bWasActive && bIsActive)
	{
		if (MassEntry && MassEntry->ActivationEffect)
		{
			ArcEffects::TryApplyEffect(EntityManager, Entity, MassEntry->ActivationEffect, Entity);
		}
	}

	// Deactivation
	if (bWasActive && !bIsActive)
	{
		ArcEffects::RemoveEffectsByTag(EntityManager, Entity, ActiveTagContainers[TypeIdx]);
	}

	// Break (saturation crossed 100)
	if (PrevSaturation < 100.f && Saturation >= 100.f)
	{
		if (MassEntry && MassEntry->BurstEffect)
		{
			ArcEffects::TryApplyEffect(EntityManager, Entity, MassEntry->BurstEffect, Entity);
		}
	}

	// Overload transitions
	if (PrevOverloadPhase != OverloadPhase)
	{
		if (PrevOverloadPhase == EArcConditionOverloadPhase::None && OverloadPhase == EArcConditionOverloadPhase::Overloaded)
		{
			if (MassEntry && MassEntry->OverloadEffect)
			{
				ArcEffects::TryApplyEffect(EntityManager, Entity, MassEntry->OverloadEffect, Entity);
			}
		}
		else if (PrevOverloadPhase == EArcConditionOverloadPhase::Overloaded || PrevOverloadPhase == EArcConditionOverloadPhase::Burnout)
		{
			ArcEffects::RemoveEffectsByTag(EntityManager, Entity, OverloadTagContainers[TypeIdx]);
		}
	}
}
