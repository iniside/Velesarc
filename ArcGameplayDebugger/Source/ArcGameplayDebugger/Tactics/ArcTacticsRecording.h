// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSTypes.h"
#include "Debugger/ArcTacticsTrace.h"
#include "Decision/ArcTacticsCompiledData.h"
#include "Decision/ArcTacticsInstance.h"
#include "Mass/ArcTacticsOption.h"

struct FArcTacticsRecordedTarget
{
	EArcTacticsValueIdentity Type = EArcTacticsValueIdentity::None;
	uint32 EntityIndex = 0;
	uint32 EntitySerial = 0;
	FVector3f Location = FVector3f::ZeroVector;
	bool bHasLocation = false;
};

struct FArcTacticsRecordedPosition
{
	FVector3f Location = FVector3f::ZeroVector;
	bool bValid = false;
};

struct FArcTacticsRecordedCandidate
{
	double Time = 0.0;
	uint32 DecisionGeneration = 0;
	int32 Order = INDEX_NONE;
	FString Package;
	FString Name;
	int32 SourceIndex = INDEX_NONE;
	bool bFromSelector = false;
	FArcTacticsRecordedTarget Target;
	float Score = 0.0f;
	int32 Priority = 0;
	bool bHasPriority = false;
	EArcTacticsCandidateOutcome Outcome = EArcTacticsCandidateOutcome::Accepted;
	FString Reason;
	TArray<FString> ConsiderationNames;
	TArray<float> Raw;
	TArray<float> Curved;
	int32 EarlyOutIndex = INDEX_NONE;
	int32 VetoIndex = INDEX_NONE;
};

struct FArcTacticsRecordedDecisionPackage
{
	uint32 DecisionGeneration = 0;
	FString ConfigName;
	float MinScore = 0.0f;
	bool bActive = false;
	bool bExcluded = false;
	FString Diagnostic;
};

struct FArcTacticsRecordedDecision
{
	double Time = 0.0;
	uint32 DecisionGeneration = 0;
	EArcTacticsSelectionMode SelectionMode = EArcTacticsSelectionMode::HighestScore;
	TArray<FArcTacticsRecordedDecisionPackage> Packages;
	int32 SelectedOrder = INDEX_NONE;
	EArcTacticsDecisionStatus Status = EArcTacticsDecisionStatus::Idle;
	FString Diagnostic;
	FArcTacticsRecordedPosition Position;
	TArray<FArcTacticsRecordedCandidate> Candidates;
	TArray<int32> QueryIndices;

	const FArcTacticsRecordedCandidate* FindSelectedCandidate() const
	{
		if (SelectedOrder == INDEX_NONE)
		{
			return nullptr;
		}
		return Candidates.FindByPredicate([this](const FArcTacticsRecordedCandidate& Candidate)
		{
			return Candidate.Order == SelectedOrder;
		});
	}
};

struct FArcTacticsRecordedActionEnd
{
	EArcTacticsActionEndStatus Status = EArcTacticsActionEndStatus::Cancelled;
	EArcTacticsActionEndReason Reason = EArcTacticsActionEndReason::Completed;
	bool bCooldownStamped = false;
	float Cooldown = 0.0f;
	FString HistoryTag;
	bool bHistoryAppended = false;
};

struct FArcTacticsRecordedActionSpan
{
	double StartTime = 0.0;
	double EndTime = 0.0;
	bool bEnteredInRecording = false;
	bool bEnded = false;
	bool bOpenAtRecordingEnd = false;
	uint32 ActionSerial = 0;
	FString Package;
	FString Name;
	int32 SourceIndex = INDEX_NONE;
	bool bFromSelector = false;
	FArcTacticsRecordedTarget Target;
	FArcTacticsRecordedPosition StartPosition;
	FArcTacticsRecordedPosition EndPosition;
	FArcTacticsRecordedActionEnd End;
	TArray<int32> PathIndices;
	TArray<int32> QueryIndices;
};

struct FArcTacticsRecordedQueryItem
{
	FVector3f Location = FVector3f::ZeroVector;
	float Score = 0.0f;
	uint32 EntityIndex = 0;
	uint32 EntitySerial = 0;
	bool bValid = false;
	bool bSelected = false;
	bool bApplied = false;
};

struct FArcTacticsRecordedQueryStep
{
	FString Name;
	EArcTQSStepType Type = EArcTQSStepType::Filter;
	int32 FilteredCount = 0;
	TArray<float> RawScores;
	TArray<float> CumulativeScores;
};

struct FArcTacticsRecordedQuery
{
	double Time = 0.0;
	uint32 DecisionGeneration = 0;
	EArcTacticsQuerySource SourceKind = EArcTacticsQuerySource::GlobalTask;
	FString SourceName;
	uint32 ActionSerial = 0;
	FString QueryName;
	int32 QueryId = INDEX_NONE;
	EArcTQSQueryStatus Status = EArcTQSQueryStatus::Pending;
	EArcTQSSelectionMode SelectionMode = EArcTQSSelectionMode::HighestScore;
	FArcTacticsRecordedPosition QuerierLocation;
	int32 TotalItemCount = 0;
	bool bComplete = false;
	TArray<FArcTacticsRecordedQueryItem> Items;
	TArray<FArcTacticsRecordedQueryStep> Steps;
	int32 SpanIndex = INDEX_NONE;
	int32 DecisionIndex = INDEX_NONE;
};

struct FArcTacticsRecordedPath
{
	double Time = 0.0;
	int32 SpanIndex = INDEX_NONE;
	bool bRepath = false;
	bool bSucceeded = false;
	bool bPartial = false;
	FVector3f Start = FVector3f::ZeroVector;
	FVector3f End = FVector3f::ZeroVector;
	TArray<FVector3f> Points;
};

enum class EArcTacticsRecordedActionEventType : uint8
{
	Entered,
	StepChanged,
	Path,
	Query,
	Ended
};

struct FArcTacticsRecordedActionEvent
{
	EArcTacticsRecordedActionEventType Type = EArcTacticsRecordedActionEventType::Entered;
	double Time = 0.0;
	int32 SpanIndex = INDEX_NONE;
	FArcTacticsRecordedPosition Position;
	int32 StepIndex = INDEX_NONE;
	int32 StepCount = 0;
	FString StepName;
	FString StepTypeName;
	int32 PathIndex = INDEX_NONE;
	int32 QueryIndex = INDEX_NONE;
};

struct FArcTacticsRecordedEntity
{
	uint64 WorldId = 0;
	uint32 EntityIndex = 0;
	uint32 EntitySerial = 0;
	TArray<FString> ConfigNames;
	TArray<FArcTacticsRecordedDecision> Decisions;
	TArray<FArcTacticsRecordedActionSpan> Actions;
	TArray<FArcTacticsRecordedActionEvent> Events;
	TArray<FArcTacticsRecordedPath> Paths;
	TArray<FArcTacticsRecordedQuery> Queries;
};

struct FArcTacticsRecordedWorld
{
	uint64 WorldId = 0;
	double OriginWorldTime = 0.0;
	bool bHasRecordingStarted = false;
};

struct FArcTacticsRecording
{
	FString FilePath;
	TArray<FArcTacticsRecordedWorld> Worlds;
	TArray<FArcTacticsRecordedEntity> Entities;
	double Duration = 0.0;
	int32 UnclaimedCandidateCount = 0;
	int32 UnclaimedQueryCount = 0;
};
