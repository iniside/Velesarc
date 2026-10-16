// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSStep.h"

#if !UE_BUILD_SHIPPING
enum class EArcTQSStepState : uint8
{
	NotRun,
	Executed,
	Skipped,
	Null
};

struct FArcTQSStepResult
{
	FString StepName;
	EArcTQSStepType StepType = EArcTQSStepType::Score;
	EArcTQSStepState State = EArcTQSStepState::NotRun;
	TArray<float> RawScores;
	TArray<float> CumulativeScores;
	int32 FilteredCount = 0;
};
#endif

struct FArcTQSQueryResult
{
	TArray<FArcTQSTargetItem> Items;
	TArray<int32> ResultIndices;
	FVector QuerierLocation = FVector::ZeroVector;
	EArcTQSQueryStatus Status = EArcTQSQueryStatus::Pending;
	EArcTQSSelectionMode SelectionMode = EArcTQSSelectionMode::HighestScore;
	int32 TotalGenerated = 0;
	int32 TotalValid = 0;
	double ExecutionTimeMs = 0.0;

#if !UE_BUILD_SHIPPING
	TArray<int32> FilteredByStep;
	TArray<FArcTQSStepResult> Steps;
	bool bGeneratorLevelOnly = false;
#endif
};
