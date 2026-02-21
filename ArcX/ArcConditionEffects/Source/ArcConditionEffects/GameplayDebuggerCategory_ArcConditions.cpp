// Copyright Lukasz Baran. All Rights Reserved.

#include "GameplayDebuggerCategory_ArcConditions.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "MassActorSubsystem.h"
#include "MassAgentComponent.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"

namespace ArcConditionsDebug
{
	static FMassEntityHandle GetEntityFromActor(const AActor& Actor)
	{
		FMassEntityHandle EntityHandle;
		if (const UMassAgentComponent* AgentComp = Actor.FindComponentByClass<UMassAgentComponent>())
		{
			EntityHandle = AgentComp->GetEntityHandle();
		}
		else if (UMassActorSubsystem* ActorSubsystem = UWorld::GetSubsystem<UMassActorSubsystem>(Actor.GetWorld()))
		{
			EntityHandle = ActorSubsystem->GetEntityHandleFromActor(&Actor);
		}
		return EntityHandle;
	}

	static const TCHAR* GetGroupColor(EArcConditionGroup Group)
	{
		switch (Group)
		{
		case EArcConditionGroup::GroupA_Hysteresis:		return TEXT("{red}");
		case EArcConditionGroup::GroupB_Linear:			return TEXT("{yellow}");
		case EArcConditionGroup::GroupC_Environmental:	return TEXT("{cyan}");
		default:										return TEXT("{white}");
		}
	}

	static const TCHAR* GetGroupName(EArcConditionGroup Group)
	{
		switch (Group)
		{
		case EArcConditionGroup::GroupA_Hysteresis:		return TEXT("Hysteresis");
		case EArcConditionGroup::GroupB_Linear:			return TEXT("Linear");
		case EArcConditionGroup::GroupC_Environmental:	return TEXT("Environmental");
		default:										return TEXT("Unknown");
		}
	}

	static const TCHAR* GetOverloadPhaseText(EArcConditionOverloadPhase Phase)
	{
		switch (Phase)
		{
		case EArcConditionOverloadPhase::Overloaded:	return TEXT("{red}OVERLOADED");
		case EArcConditionOverloadPhase::Burnout:		return TEXT("{orange}BURNOUT");
		default:										return nullptr;
		}
	}
} // namespace ArcConditionsDebug

// ---------------------------------------------------------------------------

FGameplayDebuggerCategory_ArcConditions::FGameplayDebuggerCategory_ArcConditions()
	: Super()
{
	OnEntitySelectedHandle = FMassDebugger::OnEntitySelectedDelegate.AddRaw(
		this, &FGameplayDebuggerCategory_ArcConditions::OnEntitySelected);
}

FGameplayDebuggerCategory_ArcConditions::~FGameplayDebuggerCategory_ArcConditions()
{
	FMassDebugger::OnEntitySelectedDelegate.Remove(OnEntitySelectedHandle);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ArcConditions::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_ArcConditions());
}

void FGameplayDebuggerCategory_ArcConditions::OnEntitySelected(
	const FMassEntityManager& EntityManager, FMassEntityHandle EntityHandle)
{
	UWorld* World = EntityManager.GetWorld();
	if (World != GetWorldFromReplicator())
	{
		return;
	}

	AActor* BestActor = nullptr;
	if (EntityHandle.IsSet() && World)
	{
		if (const UMassActorSubsystem* ActorSubsystem = World->GetSubsystem<UMassActorSubsystem>())
		{
			BestActor = ActorSubsystem->GetActorFromHandle(EntityHandle);
		}
	}

	CachedEntity = EntityHandle;
	CachedDebugActor = BestActor;
	check(GetReplicator());
	GetReplicator()->SetDebugActor(BestActor);
}

// ---------------------------------------------------------------------------
// CollectData
// ---------------------------------------------------------------------------

void FGameplayDebuggerCategory_ArcConditions::CollectConditionLine(
	FMassEntityManager& EntityManager, const TCHAR* Name,
	const FArcConditionState& State, const FArcConditionConfig* Config)
{
	using namespace ArcConditionsDebug;

	const TCHAR* GroupColor = Config ? GetGroupColor(Config->Group) : TEXT("{white}");
	const TCHAR* GroupName = Config ? GetGroupName(Config->Group) : TEXT("?");
	const bool bDimmed = State.Saturation <= 0.f && !State.bActive;

	// Name + group
	FString Line;
	if (bDimmed)
	{
		Line = FString::Printf(TEXT("{grey}%-12s"), Name);
	}
	else
	{
		Line = FString::Printf(TEXT("%s%-12s"), GroupColor, Name);
	}

	// Saturation
	Line += FString::Printf(TEXT("  {white}Sat:{yellow}%5.1f"), State.Saturation);

	// Decay rate
	if (Config)
	{
		Line += FString::Printf(TEXT("  {white}Decay:{yellow}%.1f/s"), Config->DecayRate);
	}

	// Resistance
	if (State.Resistance > 0.f)
	{
		Line += FString::Printf(TEXT("  {white}Res:{yellow}%.0f%%"), State.Resistance * 100.f);
	}

	// Activation threshold (Group A)
	if (Config && Config->Group == EArcConditionGroup::GroupA_Hysteresis && Config->ActivationThreshold > 0.f)
	{
		Line += FString::Printf(TEXT("  {white}Thresh:{yellow}%.0f"), Config->ActivationThreshold);
	}

	// Active flag
	if (State.bActive)
	{
		Line += TEXT("  {green}[ACTIVE]");
	}

	// Overload phase
	if (State.OverloadPhase != EArcConditionOverloadPhase::None)
	{
		const TCHAR* PhaseText = GetOverloadPhaseText(State.OverloadPhase);
		if (PhaseText)
		{
			Line += FString::Printf(TEXT("  %s %.1fs{white}"), PhaseText, State.OverloadTimeRemaining);
		}
	}

	AddTextLine(Line);
}

void FGameplayDebuggerCategory_ArcConditions::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	const UWorld* World = GetDataWorld(OwnerPC, DebugActor);
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySubsystem)
	{
		AddTextLine(TEXT("{red}MassEntitySubsystem not available"));
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	if (DebugActor)
	{
		const FMassEntityHandle EntityHandle = ArcConditionsDebug::GetEntityFromActor(*DebugActor);
		CachedEntity = EntityHandle;
		CachedDebugActor = DebugActor;
	}

	// Header
	AddTextLine(TEXT("{white}Arc Conditions"));

	if (!CachedEntity.IsSet())
	{
		AddTextLine(TEXT("{grey}No entity selected."));
		return;
	}

	if (!EntityManager.IsEntityValid(CachedEntity))
	{
		AddTextLine(TEXT("{red}Entity is no longer valid."));
		CachedEntity = FMassEntityHandle();
		return;
	}

	AddTextLine(FString::Printf(TEXT("{white}Entity: {yellow}%s"), *CachedEntity.DebugGetDescription()));

	// Collect all present conditions via macro
	// Each macro call checks if the fragment exists on the entity and collects it.

#define ARC_COLLECT_CONDITION(Name) \
	if (const FArc##Name##ConditionFragment* Frag = EntityManager.GetFragmentDataPtr<FArc##Name##ConditionFragment>(CachedEntity)) \
	{ \
		const FArc##Name##ConditionConfig* Cfg = EntityManager.GetConstSharedFragmentDataPtr<FArc##Name##ConditionConfig>(CachedEntity); \
		CollectConditionLine(EntityManager, TEXT(#Name), Frag->State, Cfg ? &Cfg->Config : nullptr); \
	}

	// Group A: Hysteresis
	AddTextLine(TEXT("{red}--- Hysteresis ---"));
	ARC_COLLECT_CONDITION(Burning)
	ARC_COLLECT_CONDITION(Bleeding)
	ARC_COLLECT_CONDITION(Chilled)
	ARC_COLLECT_CONDITION(Shocked)
	ARC_COLLECT_CONDITION(Poisoned)
	ARC_COLLECT_CONDITION(Diseased)
	ARC_COLLECT_CONDITION(Weakened)

	// Group B: Linear
	AddTextLine(TEXT("{yellow}--- Linear ---"));
	ARC_COLLECT_CONDITION(Oiled)
	ARC_COLLECT_CONDITION(Wet)
	ARC_COLLECT_CONDITION(Corroded)

	// Group C: Environmental
	AddTextLine(TEXT("{cyan}--- Environmental ---"));
	ARC_COLLECT_CONDITION(Blinded)
	ARC_COLLECT_CONDITION(Suffocating)
	ARC_COLLECT_CONDITION(Exhausted)

#undef ARC_COLLECT_CONDITION
}

// ---------------------------------------------------------------------------
// DrawData
// ---------------------------------------------------------------------------

void FGameplayDebuggerCategory_ArcConditions::DrawData(
	APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}

#endif // WITH_GAMEPLAY_DEBUGGER
