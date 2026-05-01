// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionEffectsSubsystem.h"

#include "ArcConditionMassEffectsConfig.h"
#include "ArcConditionFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcConditionEffects, Log, All);

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
	if (!World || !Entity.IsValid() || ConditionType >= EArcConditionType::MAX)
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

	const FArcConditionStatesFragment* Frag = EntityManager.GetFragmentDataPtr<FArcConditionStatesFragment>(Entity);
	return Frag ? &Frag->States[static_cast<int32>(ConditionType)] : nullptr;
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

void UArcConditionEffectsSubsystem::SetEffectsConfig(UArcConditionEffectsConfig* Config)
{
	if (EffectsConfig && EffectsConfig != Config)
	{
		UE_LOG(LogArcConditionEffects, Warning,
			TEXT("SetEffectsConfig called with a different config. "
				 "Existing GEs applied under the old config will NOT be automatically cleaned up."));
	}
	EffectsConfig = Config;
}

void UArcConditionEffectsSubsystem::SetMassEffectsConfig(UArcConditionMassEffectsConfig* Config)
{
	MassEffectsConfig = Config;
}
