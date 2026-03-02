// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Record of a step definition (name, type, weight). */
struct FArcTQSTraceStepRecord
{
	int32 StepIndex = INDEX_NONE;
	FString StepName;
	uint8 StepKind = 0; // 0=Filter, 1=Score
	float Weight = 1.0f;
};

/** Record of a single item in the final query state. */
struct FArcTQSTraceItemRecord
{
	uint8 TargetType = 0;
	FVector Location = FVector::ZeroVector;
	float Score = 0.0f;
	bool bValid = false;
	uint64 EntityHandle = 0; // packed Index | (Serial << 32)
};

/** Complete record of one TQS query execution. */
struct FArcTQSTraceQueryRecord
{
	double Timestamp = 0.0;    // start time (computed: end - exec time)
	double EndTimestamp = 0.0;  // end time (trace event time)
	uint64 InstanceId = 0;     // TraceInstanceId for Rewind Debugger FObjectId mapping

	int32 QueryId = INDEX_NONE;
	FVector QuerierLocation = FVector::ZeroVector;
	FVector QuerierForward = FVector::ForwardVector;
	TArray<FVector> ContextLocations;
	FString GeneratorName;
	int32 GeneratedItemCount = 0;

	TArray<FArcTQSTraceStepRecord> Steps;

	uint8 Status = 0;
	uint8 SelectionMode = 0;
	float ExecutionTimeMs = 0.0f;
	TArray<FArcTQSTraceItemRecord> FinalItems;
	TArray<int32> ResultIndices;
};
