// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityTypes.h"
#include "MassEntityConcepts.h"

#include "ArcConditionTypes.generated.h"

// ---------------------------------------------------------------------------
// Condition Identifiers
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EArcConditionType : uint8
{
	// Group A: Infinite with Hysteresis
	Burning		= 0,
	Bleeding	= 1,
	Chilled		= 2,
	Shocked		= 3,
	Poisoned	= 4,
	Diseased	= 5,
	Weakened	= 6,

	// Group B: Linear Catalysts
	Oiled		= 7,
	Wet			= 8,
	Corroded	= 9,

	// Group C: Binary / Environmental
	Blinded		= 10,
	Suffocating	= 11,
	Exhausted	= 12,

	MAX			UMETA(Hidden)
};

static constexpr int32 ArcConditionTypeCount = static_cast<int32>(EArcConditionType::MAX);

// ---------------------------------------------------------------------------
// Condition Group — determines behavior rules
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EArcConditionGroup : uint8
{
	/** Hysteresis: activates at threshold, deactivates only at 0. Exponential scaling. */
	GroupA_Hysteresis,
	/** Linear: 1 pt = 1% power. No hysteresis. Catalyst/modifier role. */
	GroupB_Linear,
	/** Binary/Environmental: tiered or threshold-based. */
	GroupC_Environmental,
};

// ---------------------------------------------------------------------------
// Overload Phase
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EArcConditionOverloadPhase : uint8
{
	/** Normal operation — saturation accumulates and decays. */
	None,
	/** Reached 100 — bar locked, enhanced effects active. */
	Overloaded,
	/** Overload expired — bar rapidly drops to safe level. */
	Burnout,
};

// ---------------------------------------------------------------------------
// Per-Condition Runtime State (embedded in each condition fragment)
// ---------------------------------------------------------------------------

USTRUCT()
struct FArcConditionState
{
	GENERATED_BODY()

	/** Current saturation value [0, 100]. */
	UPROPERTY()
	float Saturation = 0.f;

	/** Whether the condition is "active" (past activation threshold). For Group A: hysteresis flag. */
	UPROPERTY()
	bool bActive = false;

	/** Current overload phase. */
	UPROPERTY()
	EArcConditionOverloadPhase OverloadPhase = EArcConditionOverloadPhase::None;

	/** Time remaining in the current overload phase (seconds). */
	UPROPERTY()
	float OverloadTimeRemaining = 0.f;

	/** Resistance to this condition [0, 1]. Reduces applied saturation. 0 = no resistance. */
	UPROPERTY()
	float Resistance = 0.f;
};

// ---------------------------------------------------------------------------
// Per-Condition Config (embedded in each condition's const shared fragment)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FArcConditionConfig
{
	GENERATED_BODY()

	/** Which mechanical group this condition belongs to. */
	UPROPERTY(EditAnywhere)
	EArcConditionGroup Group = EArcConditionGroup::GroupA_Hysteresis;

	/** Saturation threshold at which the condition activates (Group A). 0 = always active (Group B). */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "100"))
	float ActivationThreshold = 0.f;

	/** Natural decay rate (saturation points per second). */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float DecayRate = 2.f;

	/** Duration of the overload phase when hitting 100 (seconds). */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float OverloadDuration = 5.f;

	/** Duration of the burnout phase after overload (seconds). */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float BurnoutDuration = 2.f;

	/** Multiplier applied to decay rate during burnout phase. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	float BurnoutDecayMultiplier = 5.f;

	/** Target saturation after burnout completes. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "100"))
	float BurnoutTargetSaturation = 0.f;
};

// ---------------------------------------------------------------------------
// Signals — per-condition
// ---------------------------------------------------------------------------

namespace UE::ArcConditionEffects::Signals
{
	// Apply signals (one per condition)
	const FName BurningApply		= FName(TEXT("ArcConditionBurningApply"));
	const FName BleedingApply		= FName(TEXT("ArcConditionBleedingApply"));
	const FName ChilledApply		= FName(TEXT("ArcConditionChilledApply"));
	const FName ShockedApply		= FName(TEXT("ArcConditionShockedApply"));
	const FName PoisonedApply		= FName(TEXT("ArcConditionPoisonedApply"));
	const FName DiseasedApply		= FName(TEXT("ArcConditionDiseasedApply"));
	const FName WeakenedApply		= FName(TEXT("ArcConditionWeakenedApply"));
	const FName OiledApply			= FName(TEXT("ArcConditionOiledApply"));
	const FName WetApply			= FName(TEXT("ArcConditionWetApply"));
	const FName CorrodedApply		= FName(TEXT("ArcConditionCorrodedApply"));
	const FName BlindedApply		= FName(TEXT("ArcConditionBlindedApply"));
	const FName SuffocatingApply	= FName(TEXT("ArcConditionSuffocatingApply"));
	const FName ExhaustedApply		= FName(TEXT("ArcConditionExhaustedApply"));

	// State change signals
	const FName ConditionStateChanged	= FName(TEXT("ArcConditionStateChanged"));
	const FName ConditionOverloadChanged = FName(TEXT("ArcConditionOverloadChanged"));

	/** Look up the apply signal for a condition type. */
	inline FName GetApplySignal(EArcConditionType Type)
	{
		switch (Type)
		{
		case EArcConditionType::Burning:		return BurningApply;
		case EArcConditionType::Bleeding:		return BleedingApply;
		case EArcConditionType::Chilled:		return ChilledApply;
		case EArcConditionType::Shocked:		return ShockedApply;
		case EArcConditionType::Poisoned:		return PoisonedApply;
		case EArcConditionType::Diseased:		return DiseasedApply;
		case EArcConditionType::Weakened:		return WeakenedApply;
		case EArcConditionType::Oiled:			return OiledApply;
		case EArcConditionType::Wet:			return WetApply;
		case EArcConditionType::Corroded:		return CorrodedApply;
		case EArcConditionType::Blinded:		return BlindedApply;
		case EArcConditionType::Suffocating:	return SuffocatingApply;
		case EArcConditionType::Exhausted:		return ExhaustedApply;
		default:								return NAME_None;
		}
	}
}

// ---------------------------------------------------------------------------
// Application Request — queued in subsystem, processed by application processors
// ---------------------------------------------------------------------------

struct FArcConditionApplicationRequest
{
	FMassEntityHandle Entity;
	EArcConditionType ConditionType = EArcConditionType::Burning;
	/** Amount of saturation to apply. Positive = add, negative = remove. */
	float Amount = 0.f;
};

// ---------------------------------------------------------------------------
// Shared helpers for condition state manipulation
// ---------------------------------------------------------------------------

namespace ArcConditionHelpers
{
	/** Clamp saturation to [0, 100] and trigger overload if hitting 100. */
	inline void SetSaturationClamped(FArcConditionState& State, const FArcConditionConfig& Config, float NewValue)
	{
		State.Saturation = FMath::Clamp(NewValue, 0.f, 100.f);

		if (State.Saturation >= 100.f
			&& State.OverloadPhase == EArcConditionOverloadPhase::None
			&& Config.OverloadDuration > 0.f)
		{
			State.OverloadPhase = EArcConditionOverloadPhase::Overloaded;
			State.OverloadTimeRemaining = Config.OverloadDuration;
		}
	}

	/** Update active flag based on group rules. */
	inline void UpdateActiveFlag(FArcConditionState& State, const FArcConditionConfig& Config)
	{
		switch (Config.Group)
		{
		case EArcConditionGroup::GroupA_Hysteresis:
			if (!State.bActive && State.Saturation >= Config.ActivationThreshold)
			{
				State.bActive = true;
			}
			else if (State.bActive && State.Saturation <= 0.f)
			{
				State.bActive = false;
			}
			break;
		case EArcConditionGroup::GroupB_Linear:
		case EArcConditionGroup::GroupC_Environmental:
			State.bActive = State.Saturation > 0.f;
			break;
		}
	}

	/** Clear a condition entirely (saturation, active, overload). Preserves Resistance. */
	inline void ClearCondition(FArcConditionState& State)
	{
		State.Saturation = 0.f;
		State.bActive = false;
		State.OverloadPhase = EArcConditionOverloadPhase::None;
		State.OverloadTimeRemaining = 0.f;
	}

	/** Apply resistance reduction to an amount. */
	inline float ApplyResistance(float Amount, float Resistance)
	{
		if (Amount > 0.f && Resistance > 0.f)
		{
			return Amount * (1.f - FMath::Clamp(Resistance, 0.f, 1.f));
		}
		return Amount;
	}

	/** Tick decay/overload/burnout for a single condition. Sets out-params if state/overload changed. */
	inline void TickCondition(FArcConditionState& State, const FArcConditionConfig& Config, float DeltaTime,
		bool& bOutStateChanged, bool& bOutOverloadChanged)
	{
		if (State.Saturation <= 0.f && State.OverloadPhase == EArcConditionOverloadPhase::None)
		{
			return;
		}

		const bool bWasActive = State.bActive;
		const EArcConditionOverloadPhase PrevOverload = State.OverloadPhase;

		switch (State.OverloadPhase)
		{
		case EArcConditionOverloadPhase::Overloaded:
		{
			State.OverloadTimeRemaining -= DeltaTime;
			if (State.OverloadTimeRemaining <= 0.f)
			{
				State.OverloadPhase = EArcConditionOverloadPhase::Burnout;
				State.OverloadTimeRemaining = Config.BurnoutDuration;
			}
			break;
		}
		case EArcConditionOverloadPhase::Burnout:
		{
			const float BurnoutDecay = Config.DecayRate * Config.BurnoutDecayMultiplier * DeltaTime;
			State.Saturation = FMath::Max(Config.BurnoutTargetSaturation, State.Saturation - BurnoutDecay);

			State.OverloadTimeRemaining -= DeltaTime;
			if (State.OverloadTimeRemaining <= 0.f || State.Saturation <= Config.BurnoutTargetSaturation)
			{
				State.Saturation = Config.BurnoutTargetSaturation;
				State.OverloadPhase = EArcConditionOverloadPhase::None;
				State.OverloadTimeRemaining = 0.f;
			}
			break;
		}
		default:
		{
			if (Config.DecayRate > 0.f && State.Saturation > 0.f)
			{
				State.Saturation = FMath::Max(0.f, State.Saturation - Config.DecayRate * DeltaTime);
			}
			break;
		}
		}

		UpdateActiveFlag(State, Config);

		bOutStateChanged = (State.bActive != bWasActive);
		bOutOverloadChanged = (State.OverloadPhase != PrevOverload);
	}
}
