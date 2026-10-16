// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debugger/ArcMassAbilityTraceTypes.h"
#include "TraceServices/Model/AnalysisSession.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcMABProvider, Log, All);

// ---------------------------------------------------------------------------
// FArcMassAbilityTraceProvider
//
// Stores deserialized ArcMassAbility trace events keyed by per-entity
// InstanceId.  One FArcMABEntityRecord per entity, each holding separate
// per-event-type timelines.  Thread safety via FRWLock — Append* takes the
// write lock; all accessors take the read lock.
// ---------------------------------------------------------------------------

class FArcMassAbilityTraceProvider : public TraceServices::IProvider
{
public:
	static const FName ProviderName;

	// -----------------------------------------------------------------------
	// Write API — called by the analyzer on the analysis thread
	// -----------------------------------------------------------------------

	void AppendAbilityActivated(uint64 InstanceId, FArcMABAbilityActivatedRecord&& Record);
	void AppendAbilityEnded    (uint64 InstanceId, FArcMABAbilityEndedRecord&&     Record);
	void AppendEffectApplied   (uint64 InstanceId, FArcMABEffectAppliedRecord&&    Record);
	void AppendEffectExecuted  (uint64 InstanceId, FArcMABEffectExecutedRecord&&   Record);
	void AppendAttributeChanged(uint64 InstanceId, FArcMABAttributeChangedRecord&& Record);
	void AppendTagCountChanged (uint64 InstanceId, FArcMABTagCountChangedRecord&&  Record);
	void AppendCostFailed      (uint64 InstanceId, FArcMABCostFailedRecord&&       Record);
	void AppendCooldownFailed  (uint64 InstanceId, FArcMABCooldownFailedRecord&&   Record);
	void AppendSignalRaised    (uint64 InstanceId, FArcMABSignalRaisedRecord&&     Record);

	// -----------------------------------------------------------------------
	// Read API — called by the track / playback extension on the game thread
	// -----------------------------------------------------------------------

	/** Returns the bundle for a specific entity instance, or nullptr. */
	const FArcMABEntityRecord* GetRecordsForInstance(uint64 InstanceId) const;

	/**
	 * Returns the first entity record whose most-recent event timestamp falls
	 * within [Time - WindowSeconds, Time + WindowSeconds].  Used by the
	 * playback extension for viewport drawing.
	 */
	const FArcMABEntityRecord* GetRecordsAtTime(double Time, double WindowSeconds = 0.3) const;

	/** Walk every stored entity bundle. */
	void EnumerateInstances(TFunctionRef<void(uint64 InstanceId, const FArcMABEntityRecord&)> Callback) const;

	bool HasData() const;

private:
	// Finds-or-adds the entity record and stamps its identity fields on first
	// creation.  Must be called with the write lock held.
	FArcMABEntityRecord& FindOrAddRecordLocked(uint64 InstanceId, uint32 EntityIndex, uint32 EntitySerial);

	TMap<uint64 /*InstanceId*/, FArcMABEntityRecord> InstanceRecords;
	mutable FRWLock RecordsLock;
};
