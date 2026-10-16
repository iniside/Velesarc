// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "IRewindDebuggerTrackCreator.h"
#include "RewindDebuggerTrack.h"
#include "SEventTimelineView.h"

class FArcMassAbilityTraceProvider;
struct FArcMABEntityRecord;

DECLARE_LOG_CATEGORY_EXTERN(LogArcMABTrack, Log, All);

// ---------------------------------------------------------------------------
// FArcMassAbilityRewindDebuggerTrackCreator
//
// Modular-feature factory.  Registered by ArcMassAbilitiesEditorModule.
// Matches entities traced via TRACE_INSTANCE(..., FArcMassAbilityEntityContext::StaticStruct(), ...).
// ---------------------------------------------------------------------------

class FArcMassAbilityRewindDebuggerTrackCreator : public RewindDebugger::IRewindDebuggerTrackCreator
{
protected:
	virtual FName GetTargetTypeNameInternal() const override;
	virtual FName GetNameInternal() const override;
	virtual void GetTrackTypesInternal(TArray<RewindDebugger::FRewindDebuggerTrackType>& Types) const override;
	virtual TSharedPtr<RewindDebugger::FRewindDebuggerTrack> CreateTrackInternal(
		const RewindDebugger::FObjectId& InObjectId) const override;
	virtual bool HasDebugInfoInternal(const RewindDebugger::FObjectId& InObjectId) const override;
	virtual bool IsCreatingPrimaryChildTrackInternal() const override { return true; }
};

// ---------------------------------------------------------------------------
// FArcMassAbilityRewindDebuggerTrack
//
// One track per Mass entity.  Displays event counts in the details panel and
// point markers on the timeline (one row, colour-coded by event type).
// ---------------------------------------------------------------------------

class FArcMassAbilityRewindDebuggerTrack : public RewindDebugger::FRewindDebuggerTrack
{
public:
	explicit FArcMassAbilityRewindDebuggerTrack(const RewindDebugger::FObjectId& InObjectId);

private:
	virtual bool UpdateInternal() override;
	virtual TSharedPtr<SWidget> GetTimelineViewInternal() override;
	virtual TSharedPtr<SWidget> GetDetailsViewInternal() override;
	virtual FSlateIcon GetIconInternal() override;
	virtual FName GetNameInternal() const override;
	virtual FText GetDisplayNameInternal() const override;
	virtual uint64 GetObjectIdInternal() const override;
	virtual bool HasDebugDataInternal() const override;

	RewindDebugger::FObjectId ObjectId;

	/** Cached pointer into the provider — valid as long as the session is open. */
	const FArcMABEntityRecord* CachedRecord = nullptr;

	/** Persistent timeline data — rebuilt in UpdateInternal, consumed by SEventTimelineView. */
	TSharedPtr<SEventTimelineView::FTimelineEventData> EventData;
};
