// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

// Mirror the engine pattern: trace in non-shipping builds only.
#if !UE_BUILD_SHIPPING
#define WITH_ARCMA_TRACE 1
#else
#define WITH_ARCMA_TRACE 0
#endif

#if WITH_ARCMA_TRACE

#include "Trace/Trace.h"
#include "HAL/PlatformTime.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "Attributes/ArcAttribute.h"

// ---------------------------------------------------------------------------
// Channel
// ---------------------------------------------------------------------------

UE_TRACE_CHANNEL_EXTERN(ArcMassAbilityChannel, ARCMASSABILITIES_API)

class UArcGameEventSubsystem;
class UArcAbilityDefinition;
class UArcEffectDefinition;

// ---------------------------------------------------------------------------
// Trace emission functions
// ---------------------------------------------------------------------------

namespace UE::ArcMassAbilityTrace
{
	void OutputAttributeChanged(const UArcGameEventSubsystem* Bus, FMassEntityHandle Entity, const FArcAttributeRef& Attribute, float OldValue, float NewValue);
	void OutputAbilityActivated(const UArcGameEventSubsystem* Bus, FMassEntityHandle Entity, const UArcAbilityDefinition* Ability);
	void OutputEffectApplied(const UArcGameEventSubsystem* Bus, FMassEntityHandle Entity, const UArcEffectDefinition* Effect);
	void OutputEffectExecuted(const UArcGameEventSubsystem* Bus, FMassEntityHandle Entity, const UArcEffectDefinition* Effect);
	void OutputTagCountChanged(const UArcGameEventSubsystem* Bus, FMassEntityHandle Entity, FGameplayTag Tag, int32 OldCount, int32 NewCount);
	void OutputAbilityEnded(const UArcGameEventSubsystem* Bus, FMassEntityHandle Entity, const UArcAbilityDefinition* Ability, uint8 Reason, FMassEntityHandle Instigator);
	void OutputCostFailed(const UArcGameEventSubsystem* Bus, FMassEntityHandle Entity, const UArcAbilityDefinition* Ability, uint8 Reason, float MissingAmount);
	void OutputCooldownFailed(const UArcGameEventSubsystem* Bus, FMassEntityHandle Entity, const UArcAbilityDefinition* Ability, float RemainingMs);
	void OutputSignalRaised(const UArcGameEventSubsystem* Bus, FMassEntityHandle Entity, FName SignalName);

} // namespace UE::ArcMassAbilityTrace

// ---------------------------------------------------------------------------
// Macros — channel-gated so the branch is free when channel is disabled
// ---------------------------------------------------------------------------

#define TRACE_ARCMA_ATTRIBUTE_CHANGED(...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel)) \
	{ UE::ArcMassAbilityTrace::OutputAttributeChanged(__VA_ARGS__); }

#define TRACE_ARCMA_ABILITY_ACTIVATED(...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel)) \
	{ UE::ArcMassAbilityTrace::OutputAbilityActivated(__VA_ARGS__); }

#define TRACE_ARCMA_EFFECT_APPLIED(...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel)) \
	{ UE::ArcMassAbilityTrace::OutputEffectApplied(__VA_ARGS__); }

#define TRACE_ARCMA_EFFECT_EXECUTED(...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel)) \
	{ UE::ArcMassAbilityTrace::OutputEffectExecuted(__VA_ARGS__); }

#define TRACE_ARCMA_TAG_COUNT_CHANGED(...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel)) \
	{ UE::ArcMassAbilityTrace::OutputTagCountChanged(__VA_ARGS__); }

#define TRACE_ARCMA_ABILITY_ENDED(...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel)) \
	{ UE::ArcMassAbilityTrace::OutputAbilityEnded(__VA_ARGS__); }

#define TRACE_ARCMA_COST_FAILED(...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel)) \
	{ UE::ArcMassAbilityTrace::OutputCostFailed(__VA_ARGS__); }

#define TRACE_ARCMA_COOLDOWN_FAILED(...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel)) \
	{ UE::ArcMassAbilityTrace::OutputCooldownFailed(__VA_ARGS__); }

#define TRACE_ARCMA_SIGNAL_RAISED(...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel)) \
	{ UE::ArcMassAbilityTrace::OutputSignalRaised(__VA_ARGS__); }

#else // WITH_ARCMA_TRACE

// Shipping stubs — compile to nothing
#define TRACE_ARCMA_ATTRIBUTE_CHANGED(...)  (void)0
#define TRACE_ARCMA_ABILITY_ACTIVATED(...)  (void)0
#define TRACE_ARCMA_EFFECT_APPLIED(...)     (void)0
#define TRACE_ARCMA_EFFECT_EXECUTED(...)    (void)0
#define TRACE_ARCMA_TAG_COUNT_CHANGED(...)  (void)0
#define TRACE_ARCMA_ABILITY_ENDED(...)      (void)0
#define TRACE_ARCMA_COST_FAILED(...)        (void)0
#define TRACE_ARCMA_COOLDOWN_FAILED(...)    (void)0
#define TRACE_ARCMA_SIGNAL_RAISED(...)      (void)0

#endif // WITH_ARCMA_TRACE
