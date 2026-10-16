// Copyright Lukasz Baran. All Rights Reserved.

#include "Debugger/ArcMassAbilityTrace.h"
#if WITH_ARCMA_TRACE

#include "Debugger/ArcMassAbilityEntityContext.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Effects/ArcEffectDefinition.h"
#include "ObjectTrace.h"
#include "Trace/Trace.inl"

DEFINE_LOG_CATEGORY_STATIC(LogArcMassAbilityTrace, Log, All);

UE_TRACE_CHANNEL_DEFINE(ArcMassAbilityChannel)

// ---------------------------------------------------------------------------
// File-scoped identity map: packed FMassEntityHandle -> TRACE_INSTANCE id
// ---------------------------------------------------------------------------

namespace ArcMassAbilityTraceInternal
{
	TMap<uint64, uint64> GEntityToInstanceId;
}

// ---------------------------------------------------------------------------
// Event definitions — one per queued event type
// ---------------------------------------------------------------------------

UE_TRACE_EVENT_BEGIN(ArcMassAbilityDebugger, AttributeChanged)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, AttributeName)
	UE_TRACE_EVENT_FIELD(float, OldValue)
	UE_TRACE_EVENT_FIELD(float, NewValue)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcMassAbilityDebugger, AbilityActivated)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(uint64, AbilityObjId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, AbilityName)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcMassAbilityDebugger, EffectApplied)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(uint64, EffectObjId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, EffectName)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcMassAbilityDebugger, EffectExecuted)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(uint64, EffectObjId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, EffectName)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcMassAbilityDebugger, TagCountChanged)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, TagName)
	UE_TRACE_EVENT_FIELD(int32, OldCount)
	UE_TRACE_EVENT_FIELD(int32, NewCount)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcMassAbilityDebugger, AbilityEnded)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(uint64, AbilityObjId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, AbilityName)
	UE_TRACE_EVENT_FIELD(uint8, Reason)
	UE_TRACE_EVENT_FIELD(uint32, InstigatorIndex)
	UE_TRACE_EVENT_FIELD(uint32, InstigatorSerial)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcMassAbilityDebugger, CostFailed)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(uint64, AbilityObjId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, AbilityName)
	UE_TRACE_EVENT_FIELD(uint8, Reason)
	UE_TRACE_EVENT_FIELD(float, MissingAmount)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcMassAbilityDebugger, CooldownFailed)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(uint64, AbilityObjId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, AbilityName)
	UE_TRACE_EVENT_FIELD(float, RemainingMs)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(ArcMassAbilityDebugger, SignalRaised)
	UE_TRACE_EVENT_FIELD(uint64, Cycle)
	UE_TRACE_EVENT_FIELD(uint64, InstanceId)
	UE_TRACE_EVENT_FIELD(uint32, EntityIndex)
	UE_TRACE_EVENT_FIELD(uint32, EntitySerial)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, SignalName)
UE_TRACE_EVENT_END()

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

namespace UE::ArcMassAbilityTrace
{

// Ensure a per-entity TRACE_INSTANCE exists and return its id.
// Returns 0 if OBJECT_TRACE is disabled or Subsystem is null.
static uint64 EnsureEntityInstance(const UArcGameEventSubsystem* Subsystem, FMassEntityHandle Entity)
{
#if OBJECT_TRACE_ENABLED
	if (!Subsystem)
	{
		return 0;
	}

	const uint64 EntityKey = static_cast<uint64>(Entity.Index) | (static_cast<uint64>(Entity.SerialNumber) << 32);

	const uint64* ExistingId = ArcMassAbilityTraceInternal::GEntityToInstanceId.Find(EntityKey);
	if (ExistingId)
	{
		return *ExistingId;
	}

	TRACE_OBJECT(Subsystem);
	const uint64 OuterId    = FObjectTrace::GetObjectId(Subsystem);
	const uint64 NewId      = FObjectTrace::AllocateInstanceId();
	ArcMassAbilityTraceInternal::GEntityToInstanceId.Add(EntityKey, NewId);

	const FString EntityName = FString::Printf(TEXT("Entity [%d:%d]"), Entity.Index, Entity.SerialNumber);
	TRACE_INSTANCE(Subsystem, NewId, OuterId, FArcMassAbilityEntityContext::StaticStruct(), EntityName);

	UE_LOGF(LogArcMassAbilityTrace, Log,"[ArcMA Trace] EnsureEntityInstance: Entity [%d:%d] -> InstanceId=%llu",
		Entity.Index, Entity.SerialNumber, NewId);

	return NewId;
#else
	return 0;
#endif
}

// ---------------------------------------------------------------------------

void OutputAttributeChanged(const UArcGameEventSubsystem* Subsystem, FMassEntityHandle Entity, const FArcAttributeRef& Attribute, float OldValue, float NewValue)
{
	const uint64 InstanceId = EnsureEntityInstance(Subsystem, Entity);

	const FString AttrName = Attribute.PropertyName.ToString();

	UE_TRACE_LOG(ArcMassAbilityDebugger, AttributeChanged, ArcMassAbilityChannel)
		<< AttributeChanged.Cycle(FPlatformTime::Cycles64())
		<< AttributeChanged.InstanceId(InstanceId)
		<< AttributeChanged.EntityIndex(static_cast<uint32>(Entity.Index))
		<< AttributeChanged.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< AttributeChanged.AttributeName(*AttrName, AttrName.Len())
		<< AttributeChanged.OldValue(OldValue)
		<< AttributeChanged.NewValue(NewValue);
}

// ---------------------------------------------------------------------------

void OutputAbilityActivated(const UArcGameEventSubsystem* Subsystem, FMassEntityHandle Entity, const UArcAbilityDefinition* Ability)
{
	const uint64 InstanceId = EnsureEntityInstance(Subsystem, Entity);

	uint64 AbilityObjId = 0;
	FString AbilityName = TEXT("None");
	if (Ability)
	{
#if OBJECT_TRACE_ENABLED
		TRACE_OBJECT(Ability);
		AbilityObjId = FObjectTrace::GetObjectId(Ability);
#endif
		AbilityName = Ability->GetName();
	}

	UE_TRACE_LOG(ArcMassAbilityDebugger, AbilityActivated, ArcMassAbilityChannel)
		<< AbilityActivated.Cycle(FPlatformTime::Cycles64())
		<< AbilityActivated.InstanceId(InstanceId)
		<< AbilityActivated.EntityIndex(static_cast<uint32>(Entity.Index))
		<< AbilityActivated.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< AbilityActivated.AbilityObjId(AbilityObjId)
		<< AbilityActivated.AbilityName(*AbilityName, AbilityName.Len());
}

// ---------------------------------------------------------------------------

void OutputEffectApplied(const UArcGameEventSubsystem* Subsystem, FMassEntityHandle Entity, const UArcEffectDefinition* Effect)
{
	const uint64 InstanceId = EnsureEntityInstance(Subsystem, Entity);

	uint64 EffectObjId = 0;
	FString EffectName = TEXT("None");
	if (Effect)
	{
#if OBJECT_TRACE_ENABLED
		TRACE_OBJECT(Effect);
		EffectObjId = FObjectTrace::GetObjectId(Effect);
#endif
		EffectName = Effect->GetName();
	}

	UE_TRACE_LOG(ArcMassAbilityDebugger, EffectApplied, ArcMassAbilityChannel)
		<< EffectApplied.Cycle(FPlatformTime::Cycles64())
		<< EffectApplied.InstanceId(InstanceId)
		<< EffectApplied.EntityIndex(static_cast<uint32>(Entity.Index))
		<< EffectApplied.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< EffectApplied.EffectObjId(EffectObjId)
		<< EffectApplied.EffectName(*EffectName, EffectName.Len());
}

// ---------------------------------------------------------------------------

void OutputEffectExecuted(const UArcGameEventSubsystem* Subsystem, FMassEntityHandle Entity, const UArcEffectDefinition* Effect)
{
	const uint64 InstanceId = EnsureEntityInstance(Subsystem, Entity);

	uint64 EffectObjId = 0;
	FString EffectName = TEXT("None");
	if (Effect)
	{
#if OBJECT_TRACE_ENABLED
		TRACE_OBJECT(Effect);
		EffectObjId = FObjectTrace::GetObjectId(Effect);
#endif
		EffectName = Effect->GetName();
	}

	UE_TRACE_LOG(ArcMassAbilityDebugger, EffectExecuted, ArcMassAbilityChannel)
		<< EffectExecuted.Cycle(FPlatformTime::Cycles64())
		<< EffectExecuted.InstanceId(InstanceId)
		<< EffectExecuted.EntityIndex(static_cast<uint32>(Entity.Index))
		<< EffectExecuted.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< EffectExecuted.EffectObjId(EffectObjId)
		<< EffectExecuted.EffectName(*EffectName, EffectName.Len());
}

// ---------------------------------------------------------------------------

void OutputTagCountChanged(const UArcGameEventSubsystem* Subsystem, FMassEntityHandle Entity, FGameplayTag Tag, int32 OldCount, int32 NewCount)
{
	const uint64 InstanceId = EnsureEntityInstance(Subsystem, Entity);

	const FString TagName = Tag.ToString();

	UE_TRACE_LOG(ArcMassAbilityDebugger, TagCountChanged, ArcMassAbilityChannel)
		<< TagCountChanged.Cycle(FPlatformTime::Cycles64())
		<< TagCountChanged.InstanceId(InstanceId)
		<< TagCountChanged.EntityIndex(static_cast<uint32>(Entity.Index))
		<< TagCountChanged.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< TagCountChanged.TagName(*TagName, TagName.Len())
		<< TagCountChanged.OldCount(OldCount)
		<< TagCountChanged.NewCount(NewCount);
}

// ---------------------------------------------------------------------------

void OutputAbilityEnded(const UArcGameEventSubsystem* Subsystem, FMassEntityHandle Entity, const UArcAbilityDefinition* Ability, uint8 Reason, FMassEntityHandle Instigator)
{
	const uint64 InstanceId = EnsureEntityInstance(Subsystem, Entity);

	uint64 AbilityObjId = 0;
	FString AbilityName = TEXT("None");
	if (Ability)
	{
#if OBJECT_TRACE_ENABLED
		TRACE_OBJECT(Ability);
		AbilityObjId = FObjectTrace::GetObjectId(Ability);
#endif
		AbilityName = Ability->GetName();
	}

	UE_TRACE_LOG(ArcMassAbilityDebugger, AbilityEnded, ArcMassAbilityChannel)
		<< AbilityEnded.Cycle(FPlatformTime::Cycles64())
		<< AbilityEnded.InstanceId(InstanceId)
		<< AbilityEnded.EntityIndex(static_cast<uint32>(Entity.Index))
		<< AbilityEnded.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< AbilityEnded.AbilityObjId(AbilityObjId)
		<< AbilityEnded.AbilityName(*AbilityName, AbilityName.Len())
		<< AbilityEnded.Reason(Reason)
		<< AbilityEnded.InstigatorIndex(static_cast<uint32>(Instigator.Index))
		<< AbilityEnded.InstigatorSerial(static_cast<uint32>(Instigator.SerialNumber));
}

// ---------------------------------------------------------------------------

void OutputCostFailed(const UArcGameEventSubsystem* Subsystem, FMassEntityHandle Entity, const UArcAbilityDefinition* Ability, uint8 Reason, float MissingAmount)
{
	const uint64 InstanceId = EnsureEntityInstance(Subsystem, Entity);

	uint64 AbilityObjId = 0;
	FString AbilityName = TEXT("None");
	if (Ability)
	{
#if OBJECT_TRACE_ENABLED
		TRACE_OBJECT(Ability);
		AbilityObjId = FObjectTrace::GetObjectId(Ability);
#endif
		AbilityName = Ability->GetName();
	}

	UE_TRACE_LOG(ArcMassAbilityDebugger, CostFailed, ArcMassAbilityChannel)
		<< CostFailed.Cycle(FPlatformTime::Cycles64())
		<< CostFailed.InstanceId(InstanceId)
		<< CostFailed.EntityIndex(static_cast<uint32>(Entity.Index))
		<< CostFailed.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< CostFailed.AbilityObjId(AbilityObjId)
		<< CostFailed.AbilityName(*AbilityName, AbilityName.Len())
		<< CostFailed.Reason(Reason)
		<< CostFailed.MissingAmount(MissingAmount);
}

// ---------------------------------------------------------------------------

void OutputCooldownFailed(const UArcGameEventSubsystem* Subsystem, FMassEntityHandle Entity, const UArcAbilityDefinition* Ability, float RemainingMs)
{
	const uint64 InstanceId = EnsureEntityInstance(Subsystem, Entity);

	uint64 AbilityObjId = 0;
	FString AbilityName = TEXT("None");
	if (Ability)
	{
#if OBJECT_TRACE_ENABLED
		TRACE_OBJECT(Ability);
		AbilityObjId = FObjectTrace::GetObjectId(Ability);
#endif
		AbilityName = Ability->GetName();
	}

	UE_TRACE_LOG(ArcMassAbilityDebugger, CooldownFailed, ArcMassAbilityChannel)
		<< CooldownFailed.Cycle(FPlatformTime::Cycles64())
		<< CooldownFailed.InstanceId(InstanceId)
		<< CooldownFailed.EntityIndex(static_cast<uint32>(Entity.Index))
		<< CooldownFailed.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< CooldownFailed.AbilityObjId(AbilityObjId)
		<< CooldownFailed.AbilityName(*AbilityName, AbilityName.Len())
		<< CooldownFailed.RemainingMs(RemainingMs);
}

// ---------------------------------------------------------------------------

void OutputSignalRaised(const UArcGameEventSubsystem* Subsystem, FMassEntityHandle Entity, FName SignalName)
{
	const uint64 InstanceId = EnsureEntityInstance(Subsystem, Entity);

	const FString SignalNameString = SignalName.ToString();

	UE_TRACE_LOG(ArcMassAbilityDebugger, SignalRaised, ArcMassAbilityChannel)
		<< SignalRaised.Cycle(FPlatformTime::Cycles64())
		<< SignalRaised.InstanceId(InstanceId)
		<< SignalRaised.EntityIndex(static_cast<uint32>(Entity.Index))
		<< SignalRaised.EntitySerial(static_cast<uint32>(Entity.SerialNumber))
		<< SignalRaised.SignalName(*SignalNameString, SignalNameString.Len());
}

} // namespace UE::ArcMassAbilityTrace

#endif // WITH_ARCMA_TRACE
