// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "IRewindDebuggerTrackCreator.h"
#include "RewindDebuggerTrack.h"
#include "SEventTimelineView.h"

class FArcUtilityTraceProvider;
struct FArcUtilityTraceRequestRecord;

/** Track creator — factory registered as modular feature. */
class FArcUtilityRewindDebuggerTrackCreator : public RewindDebugger::IRewindDebuggerTrackCreator
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

/** Track — one per querier entity/actor, shows all utility evaluations from that querier. */
class FArcUtilityRewindDebuggerTrack : public RewindDebugger::FRewindDebuggerTrack
{
public:
	explicit FArcUtilityRewindDebuggerTrack(const RewindDebugger::FObjectId& InObjectId);

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
	const TArray<FArcUtilityTraceRequestRecord>* CachedRequests = nullptr;

	TSharedPtr<SEventTimelineView::FTimelineEventData> EventData;
};
