// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionEffectsSubsystem.h"

#include "ArcConditionFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "Engine/Engine.h"

void UArcConditionEffectsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcConditionEffectsSubsystem::Deinitialize()
{
	PendingRequests.Empty();
	Super::Deinitialize();
}

void UArcConditionEffectsSubsystem::ApplyCondition(const FMassEntityHandle& Entity, EArcConditionType ConditionType, float Amount)
{
	if (!Entity.IsValid() || FMath::IsNearlyZero(Amount))
	{
		return;
	}

	FArcConditionApplicationRequest& Request = PendingRequests.AddDefaulted_GetRef();
	Request.Entity = Entity;
	Request.ConditionType = ConditionType;
	Request.Amount = Amount;

	// Raise the per-condition apply signal to wake the correct application processor
	const FName Signal = UE::ArcConditionEffects::Signals::GetApplySignal(ConditionType);
	if (Signal != NAME_None)
	{
		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(GetWorld());
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntity(Signal, Entity);
		}
	}
}

// ---------------------------------------------------------------------------
// Per-condition state lookup via typed fragment query
// ---------------------------------------------------------------------------

const FArcConditionState* UArcConditionEffectsSubsystem::GetConditionState(const UWorld* World, const FMassEntityHandle& Entity, EArcConditionType ConditionType)
{
	if (!World || !Entity.IsValid())
	{
		return nullptr;
	}

	const UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySubsystem)
	{
		return nullptr;
	}

	const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();
	if (!EntityManager.IsEntityValid(Entity))
	{
		return nullptr;
	}

	// Switch on condition type to query the correct typed fragment
	switch (ConditionType)
	{
#define ARC_CASE_CONDITION(Name, EnumVal) \
	case EArcConditionType::EnumVal: \
	{ \
		const FArc##Name##ConditionFragment* Frag = EntityManager.GetFragmentDataPtr<FArc##Name##ConditionFragment>(Entity); \
		return Frag ? &Frag->State : nullptr; \
	}

	ARC_CASE_CONDITION(Burning,     Burning)
	ARC_CASE_CONDITION(Bleeding,    Bleeding)
	ARC_CASE_CONDITION(Chilled,     Chilled)
	ARC_CASE_CONDITION(Shocked,     Shocked)
	ARC_CASE_CONDITION(Poisoned,    Poisoned)
	ARC_CASE_CONDITION(Diseased,    Diseased)
	ARC_CASE_CONDITION(Weakened,    Weakened)
	ARC_CASE_CONDITION(Oiled,       Oiled)
	ARC_CASE_CONDITION(Wet,         Wet)
	ARC_CASE_CONDITION(Corroded,    Corroded)
	ARC_CASE_CONDITION(Blinded,     Blinded)
	ARC_CASE_CONDITION(Suffocating, Suffocating)
	ARC_CASE_CONDITION(Exhausted,   Exhausted)

#undef ARC_CASE_CONDITION

	default:
		return nullptr;
	}
}

float UArcConditionEffectsSubsystem::GetConditionSaturation(const UObject* WorldContextObject, const FMassEntityHandle& Entity, EArcConditionType ConditionType)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const FArcConditionState* State = GetConditionState(World, Entity, ConditionType);
	return State ? State->Saturation : 0.f;
}

bool UArcConditionEffectsSubsystem::IsConditionActive(const UObject* WorldContextObject, const FMassEntityHandle& Entity, EArcConditionType ConditionType)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const FArcConditionState* State = GetConditionState(World, Entity, ConditionType);
	return State ? State->bActive : false;
}

EArcConditionOverloadPhase UArcConditionEffectsSubsystem::GetConditionOverloadPhase(const UObject* WorldContextObject, const FMassEntityHandle& Entity, EArcConditionType ConditionType)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	const FArcConditionState* State = GetConditionState(World, Entity, ConditionType);
	return State ? State->OverloadPhase : EArcConditionOverloadPhase::None;
}
