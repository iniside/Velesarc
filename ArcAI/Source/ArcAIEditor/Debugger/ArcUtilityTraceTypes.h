// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FArcUtilityTraceConsiderationSnapshot
{
	int32 EntryIndex = INDEX_NONE;
	int32 TargetIndex = INDEX_NONE;
	int32 ConsiderationIndex = INDEX_NONE;
	FString ConsiderationName;
	float RawScore = 0.0f;
	float CurvedScore = 0.0f;
	float CompensatedScore = 0.0f;
};

struct FArcUtilityTraceTargetRecord
{
	uint8 TargetType = 0;
	FVector Location = FVector::ZeroVector;
};

struct FArcUtilityTraceScoredPair
{
	int32 EntryIndex = INDEX_NONE;
	int32 TargetIndex = INDEX_NONE;
	float Score = 0.0f;
};

struct FArcUtilityTraceRequestRecord
{
	double Timestamp = 0.0;
	double EndTimestamp = 0.0;
	uint64 InstanceId = 0;

	int32 RequestId = INDEX_NONE;
	FVector QuerierLocation = FVector::ZeroVector;
	FVector QuerierForward = FVector::ForwardVector;
	int32 NumEntries = 0;
	int32 NumTargets = 0;

	uint8 Status = 0;
	uint8 SelectionMode = 0;
	float MinScore = 0.0f;
	float ExecutionTimeMs = 0.0f;

	TArray<FArcUtilityTraceTargetRecord> Targets;
	TArray<FArcUtilityTraceConsiderationSnapshot> ConsiderationSnapshots;
	TArray<FArcUtilityTraceScoredPair> ScoredPairs;

	int32 WinnerEntryIndex = INDEX_NONE;
	int32 WinnerTargetIndex = INDEX_NONE;
	float WinnerScore = 0.0f;
};
