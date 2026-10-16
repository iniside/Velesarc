// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSQueryInstance.h"
#include "ArcTQSQueryResult.h"

struct FArcTQSQueryReference;

struct FArcTQSSelectionOverride
{
	EArcTQSSelectionMode SelectionMode = EArcTQSSelectionMode::HighestScore;
	float MinPassingScore = 0.0f;
};

enum class EArcTQSCreateFailure : uint8
{
	None,
	NoDefinition,
	NotExecutable,
	LayoutMismatch
};

struct FArcTQSRunOptions
{
	int32 Seed = 0;
	EArcTQSWorldRequirements Unavailable = EArcTQSWorldRequirements::None;
	bool bEmitTrace = false;
};

namespace ArcTQS
{
	ARCTARGETQUERY_API TSharedPtr<FArcTQSQueryInstance> CreateQueryInstance(
		const FArcTQSQueryReference& Reference,
		const FArcTQSQueryContext& Context,
		int32 Priority,
		const FArcTQSSelectionOverride* Override,
		EArcTQSCreateFailure& OutFailure);

	ARCTARGETQUERY_API EArcTQSCreateFailure RunQuerySync(
		const FArcTQSQueryReference& Reference,
		const FArcTQSQueryContext& Context,
		const FArcTQSRunOptions& Options,
		FArcTQSQueryResult& OutResult);

	ARCTARGETQUERY_API void FillResult(const FArcTQSQueryInstance& Instance, FArcTQSQueryResult& OutResult);
}
