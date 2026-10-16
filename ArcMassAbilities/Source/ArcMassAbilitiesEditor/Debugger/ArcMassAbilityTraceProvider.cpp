// Copyright Lukasz Baran. All Rights Reserved.

#include "Debugger/ArcMassAbilityTraceProvider.h"

DEFINE_LOG_CATEGORY(LogArcMABProvider);

const FName FArcMassAbilityTraceProvider::ProviderName(TEXT("ArcMassAbilityTraceProvider"));

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace ArcMassAbilityTraceProviderInternal
{
	/** Returns the timestamp of the last event in the record across all timelines, or -1. */
	static double GetLatestTimestamp(const FArcMABEntityRecord& Record)
	{
		double Latest = -1.0;

		for (const FArcMABAbilityActivatedRecord& R : Record.AbilityActivated)
		{
			if (R.Time > Latest) { Latest = R.Time; }
		}
		for (const FArcMABAbilityEndedRecord& R : Record.AbilityEnded)
		{
			if (R.Time > Latest) { Latest = R.Time; }
		}
		for (const FArcMABEffectAppliedRecord& R : Record.EffectApplied)
		{
			if (R.Time > Latest) { Latest = R.Time; }
		}
		for (const FArcMABEffectExecutedRecord& R : Record.EffectExecuted)
		{
			if (R.Time > Latest) { Latest = R.Time; }
		}
		for (const FArcMABAttributeChangedRecord& R : Record.AttributeChanged)
		{
			if (R.Time > Latest) { Latest = R.Time; }
		}
		for (const FArcMABTagCountChangedRecord& R : Record.TagCountChanged)
		{
			if (R.Time > Latest) { Latest = R.Time; }
		}
		for (const FArcMABCostFailedRecord& R : Record.CostFailed)
		{
			if (R.Time > Latest) { Latest = R.Time; }
		}
		for (const FArcMABCooldownFailedRecord& R : Record.CooldownFailed)
		{
			if (R.Time > Latest) { Latest = R.Time; }
		}
		for (const FArcMABSignalRaisedRecord& R : Record.SignalRaised)
		{
			if (R.Time > Latest) { Latest = R.Time; }
		}

		return Latest;
	}
} // namespace ArcMassAbilityTraceProviderInternal

// ---------------------------------------------------------------------------
// Private helper — must be called with write lock held
// ---------------------------------------------------------------------------

FArcMABEntityRecord& FArcMassAbilityTraceProvider::FindOrAddRecordLocked(
	uint64 InstanceId, uint32 EntityIndex, uint32 EntitySerial)
{
	FArcMABEntityRecord& Record = InstanceRecords.FindOrAdd(InstanceId);
	if (Record.InstanceId == 0)
	{
		Record.InstanceId   = InstanceId;
		Record.EntityIndex  = EntityIndex;
		Record.EntitySerial = EntitySerial;

		UE_LOGF(LogArcMABProvider, Log,"[ArcMAB Provider] New entity record: InstanceId=%llu Entity[%u:%u]",
			InstanceId, EntityIndex, EntitySerial);
	}
	return Record;
}

// ---------------------------------------------------------------------------
// Append — write lock per call
// ---------------------------------------------------------------------------

void FArcMassAbilityTraceProvider::AppendAbilityActivated(uint64 InstanceId, FArcMABAbilityActivatedRecord&& Record)
{
	FWriteScopeLock ScopeLock(RecordsLock);
	FArcMABEntityRecord& Entity = FindOrAddRecordLocked(InstanceId, Record.EntityIndex, Record.EntitySerial);
	Entity.AbilityActivated.Add(MoveTemp(Record));
}

void FArcMassAbilityTraceProvider::AppendAbilityEnded(uint64 InstanceId, FArcMABAbilityEndedRecord&& Record)
{
	FWriteScopeLock ScopeLock(RecordsLock);
	FArcMABEntityRecord& Entity = FindOrAddRecordLocked(InstanceId, Record.EntityIndex, Record.EntitySerial);
	Entity.AbilityEnded.Add(MoveTemp(Record));
}

void FArcMassAbilityTraceProvider::AppendEffectApplied(uint64 InstanceId, FArcMABEffectAppliedRecord&& Record)
{
	FWriteScopeLock ScopeLock(RecordsLock);
	FArcMABEntityRecord& Entity = FindOrAddRecordLocked(InstanceId, Record.EntityIndex, Record.EntitySerial);
	Entity.EffectApplied.Add(MoveTemp(Record));
}

void FArcMassAbilityTraceProvider::AppendEffectExecuted(uint64 InstanceId, FArcMABEffectExecutedRecord&& Record)
{
	FWriteScopeLock ScopeLock(RecordsLock);
	FArcMABEntityRecord& Entity = FindOrAddRecordLocked(InstanceId, Record.EntityIndex, Record.EntitySerial);
	Entity.EffectExecuted.Add(MoveTemp(Record));
}

void FArcMassAbilityTraceProvider::AppendAttributeChanged(uint64 InstanceId, FArcMABAttributeChangedRecord&& Record)
{
	FWriteScopeLock ScopeLock(RecordsLock);
	FArcMABEntityRecord& Entity = FindOrAddRecordLocked(InstanceId, Record.EntityIndex, Record.EntitySerial);
	Entity.AttributeChanged.Add(MoveTemp(Record));
}

void FArcMassAbilityTraceProvider::AppendTagCountChanged(uint64 InstanceId, FArcMABTagCountChangedRecord&& Record)
{
	FWriteScopeLock ScopeLock(RecordsLock);
	FArcMABEntityRecord& Entity = FindOrAddRecordLocked(InstanceId, Record.EntityIndex, Record.EntitySerial);
	Entity.TagCountChanged.Add(MoveTemp(Record));
}

void FArcMassAbilityTraceProvider::AppendCostFailed(uint64 InstanceId, FArcMABCostFailedRecord&& Record)
{
	FWriteScopeLock ScopeLock(RecordsLock);
	FArcMABEntityRecord& Entity = FindOrAddRecordLocked(InstanceId, Record.EntityIndex, Record.EntitySerial);
	Entity.CostFailed.Add(MoveTemp(Record));
}

void FArcMassAbilityTraceProvider::AppendCooldownFailed(uint64 InstanceId, FArcMABCooldownFailedRecord&& Record)
{
	FWriteScopeLock ScopeLock(RecordsLock);
	FArcMABEntityRecord& Entity = FindOrAddRecordLocked(InstanceId, Record.EntityIndex, Record.EntitySerial);
	Entity.CooldownFailed.Add(MoveTemp(Record));
}

void FArcMassAbilityTraceProvider::AppendSignalRaised(uint64 InstanceId, FArcMABSignalRaisedRecord&& Record)
{
	FWriteScopeLock ScopeLock(RecordsLock);
	FArcMABEntityRecord& Entity = FindOrAddRecordLocked(InstanceId, Record.EntityIndex, Record.EntitySerial);
	Entity.SignalRaised.Add(MoveTemp(Record));
}

// ---------------------------------------------------------------------------
// Read accessors — read lock
// ---------------------------------------------------------------------------

const FArcMABEntityRecord* FArcMassAbilityTraceProvider::GetRecordsForInstance(uint64 InstanceId) const
{
	FReadScopeLock ScopeLock(RecordsLock);
	return InstanceRecords.Find(InstanceId);
}

const FArcMABEntityRecord* FArcMassAbilityTraceProvider::GetRecordsAtTime(double Time, double WindowSeconds) const
{
	FReadScopeLock ScopeLock(RecordsLock);
	for (const TPair<uint64, FArcMABEntityRecord>& Pair : InstanceRecords)
	{
		const double Latest = ArcMassAbilityTraceProviderInternal::GetLatestTimestamp(Pair.Value);
		if (Latest >= 0.0 && FMath::Abs(Latest - Time) <= WindowSeconds)
		{
			return &Pair.Value;
		}
	}
	return nullptr;
}

void FArcMassAbilityTraceProvider::EnumerateInstances(
	TFunctionRef<void(uint64 InstanceId, const FArcMABEntityRecord&)> Callback) const
{
	FReadScopeLock ScopeLock(RecordsLock);
	for (const TPair<uint64, FArcMABEntityRecord>& Pair : InstanceRecords)
	{
		Callback(Pair.Key, Pair.Value);
	}
}

bool FArcMassAbilityTraceProvider::HasData() const
{
	FReadScopeLock ScopeLock(RecordsLock);
	return !InstanceRecords.IsEmpty();
}
