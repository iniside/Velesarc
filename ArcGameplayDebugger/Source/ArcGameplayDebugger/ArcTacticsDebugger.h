// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "Tactics/ArcTacticsTraceRecorder.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class UArcTacticsDebuggerDrawComponent;
class UWorld;
struct FArcTacticsDebugDrawData;
struct FArcTacticsDebugSnapshot;
struct FArcTacticsRecordedActionSpan;
struct FArcTacticsRecordedCandidate;
struct FArcTacticsRecordedDecision;
struct FArcTacticsRecordedEntity;
struct FArcTacticsRecordedQuery;
struct FArcTacticsRecording;

class FArcTacticsDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Tick();
	void Draw();

	bool bShow = false;

private:
	struct FEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
	};

	enum class EHistoryDecisionChange : uint8
	{
		NoWinner,
		Changed,
		Unknown
	};

	void RefreshEntityList();
	void DrawLive();
	void DrawLiveSnapshot(const FArcTacticsDebugSnapshot& Snapshot) const;
	void DrawLiveActiveAction(const FArcTacticsDebugSnapshot& Snapshot) const;
	void TrackLiveActiveAction(UWorld& World, const FArcTacticsDebugSnapshot& Snapshot);
	void DrawHistory();
	void DrawHistoryToolbar();
	void DrawHistoryEntityList(const FArcTacticsRecording& Recording);
	void DrawHistoryTimeline(const FArcTacticsRecording& Recording, const FArcTacticsRecordedEntity& Entity);
	void DrawHistoryDecision(const FArcTacticsRecordedEntity& Entity);
	void DrawHistoryConsiderations(const FArcTacticsRecordedCandidate& Candidate);
	void DrawHistoryRuns(const FArcTacticsRecordedEntity& Entity);
	void DrawHistoryRun(const FArcTacticsRecordedEntity& Entity, int32 SpanIndex);
	void DrawHistoryRunEvent(const FArcTacticsRecordedEntity& Entity, int32 SpanIndex, int32 EventIndex);
	void DrawHistoryDecisionQueries(const FArcTacticsRecordedEntity& Entity, const FArcTacticsRecordedDecision& Decision);
	bool DrawHistoryQueryRow(const FArcTacticsRecordedQuery& Query, int32 QueryIndex, const FString& TimeText);
	void SelectHistoryDecision(int32 DecisionIndex);
	void SelectHistorySpan(int32 SpanIndex, bool bReveal);
	void SyncHistoryRecording(const FArcTacticsRecording* Recording);
	void SelectHistoryEntity(int32 EntityIndex, const FArcTacticsRecording& Recording);
	void RebuildHistoryEntityCache(const FArcTacticsRecordedEntity& Entity);
	void FitHistoryView(const FArcTacticsRecording& Recording);
	static const FArcTacticsRecordedActionSpan* FindHistorySpanAt(const FArcTacticsRecordedEntity& Entity, double Time);
	void ResetHistoryState();
	void BuildLiveDrawData(UWorld& World, const FEntityEntry& Entry, const FArcTacticsDebugSnapshot& Snapshot, FArcTacticsDebugDrawData& OutData) const;
	void BuildHistoryDrawData(UWorld* World, const FArcTacticsRecordedEntity& Entity, FArcTacticsDebugDrawData& OutData) const;
	void ApplyDrawData(UWorld* World, const FArcTacticsDebugDrawData& Data);
	void EnsureDrawActor(UWorld& World);
	void DestroyDrawActor();

	TArray<FEntityEntry> CachedEntities;
	int32 SelectedEntityIndex = INDEX_NONE;
	char Filter[256] = {};
	float LastRefreshTime = 0.0f;
	FArcTacticsTraceRecorder TraceRecorder;

	TWeakObjectPtr<UWorld> LiveTrackedWorld;
	FMassEntityHandle LiveTrackedEntity;
	uint32 LiveTrackedSerial = 0;
	double LiveTrackedSince = 0.0;
	bool bLiveTrackedFromEnter = false;
	uint64 LiveTrackedFrame = 0;

	const FArcTacticsRecording* HistoryRecording = nullptr;
	FString HistoryRecordingPath;
	FString HistorySelectedFile;
	float HistoryRecordSeconds = 10.0f;
	bool bHistoryRecordUntilStop = false;
	char HistoryFilter[256] = {};
	int32 HistoryEntityIndex = INDEX_NONE;
	bool bHistoryHasEntityKey = false;
	uint64 HistoryEntityWorldId = 0;
	uint32 HistoryEntityKeyIndex = 0;
	uint32 HistoryEntityKeySerial = 0;
	int32 HistoryDecisionIndex = INDEX_NONE;
	int32 HistorySpanIndex = INDEX_NONE;
	int32 HistoryRunEventIndex = INDEX_NONE;
	int32 HistoryQueryIndex = INDEX_NONE;
	bool bHistoryRevealSpan = false;
	TArray<EHistoryDecisionChange> HistoryDecisionChanges;
	TArray<int32> HistorySpanDecisions;
	TArray<TArray<int32>> HistorySpanEvents;
	double HistoryViewStart = 0.0;
	double HistoryViewDuration = 1.0;
	bool bHistoryPanning = false;

	TWeakObjectPtr<AActor> DrawActor;
	TWeakObjectPtr<UArcTacticsDebuggerDrawComponent> DrawComponent;
	bool bDrawnThisFrame = false;
	bool bHasDrawShapes = false;
};
