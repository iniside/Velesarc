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

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UArcConditionReactionProcessor::UArcConditionReactionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
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

	// All 13 condition fragments — optional, read-only
	EntityQuery.AddRequirement<FArcBurningConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcBleedingConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcChilledConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcShockedConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcPoisonedConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcDiseasedConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcWeakenedConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcOiledConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcWetConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcCorrodedConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcBlindedConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcSuffocatingConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcExhaustedConditionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
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

#define ARC_PROCESS_CONDITION(Name, EnumVal) \
{ \
	const FArcConditionState* State = Arc::Condition::GetOptionalStateConst<FArc##Name##ConditionFragment>(Ctx, Idx); \
	ProcessConditionTransition(EArcConditionType::EnumVal, State, EntityPrevStates[static_cast<int32>(EArcConditionType::EnumVal)], Entity, ASC, Subsystem); \
	if (ConditionSet && ASC) \
	{ \
		ASC->SetNumericAttributeBase(UArcConditionAttributeSet::Get##Name##SaturationAttribute(), State ? State->Saturation : 0.f); \
	} \
}

void UArcConditionReactionProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	EnsureTagContainersBuilt();

	UArcConditionEffectsSubsystem* Subsystem = UWorld::GetSubsystem<UArcConditionEffectsSubsystem>(EntityManager.GetWorld());

	EntityQuery.ForEachEntityChunk(Context,
		[this, &EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FArcCoreAbilitySystemFragment> ASCFragments = Ctx.GetFragmentView<FArcCoreAbilitySystemFragment>();
			const bool bHasASC = !ASCFragments.IsEmpty();

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

				// Process all 13 conditions
				ARC_PROCESS_CONDITION(Burning, Burning)
				ARC_PROCESS_CONDITION(Bleeding, Bleeding)
				ARC_PROCESS_CONDITION(Chilled, Chilled)
				ARC_PROCESS_CONDITION(Shocked, Shocked)
				ARC_PROCESS_CONDITION(Poisoned, Poisoned)
				ARC_PROCESS_CONDITION(Diseased, Diseased)
				ARC_PROCESS_CONDITION(Weakened, Weakened)
				ARC_PROCESS_CONDITION(Oiled, Oiled)
				ARC_PROCESS_CONDITION(Wet, Wet)
				ARC_PROCESS_CONDITION(Corroded, Corroded)
				ARC_PROCESS_CONDITION(Blinded, Blinded)
				ARC_PROCESS_CONDITION(Suffocating, Suffocating)
				ARC_PROCESS_CONDITION(Exhausted, Exhausted)
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

#undef ARC_PROCESS_CONDITION

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
