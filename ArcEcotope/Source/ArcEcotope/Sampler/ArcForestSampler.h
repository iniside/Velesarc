// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ArcForestGrowthEngine.h"
#include "Simulation/ArcForestWorldSnapshot.h"

struct FArcForestSamplerInput;

struct ARCECOTOPE_API FArcForestSamplerResult
{
	EArcForestRunStatus Status = EArcForestRunStatus::Failed;
	FString Error;
	FArcForestWorldSnapshot Snapshot;
};

class ARCECOTOPE_API FArcForestSampler
{
public:
	static bool Sample(
		const FArcForestSamplerInput& Input,
		const FArcForestCancellationToken& Cancellation,
		FArcForestProgressState& Progress,
		FArcForestSamplerResult& OutResult);
};
