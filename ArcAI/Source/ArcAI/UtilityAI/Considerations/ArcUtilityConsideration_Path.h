// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "ArcUtilityConsideration_Path.generated.h"

/**
 * Scores based on navigation path length from querier to target.
 * Shorter paths score higher: score = 1 - (pathLength / MaxPathLength).
 * Returns 0 if no path exists or path exceeds MaxPathLength.
 */
USTRUCT(BlueprintType, DisplayName = "Path Length Consideration")
struct ARCAI_API FArcUtilityConsideration_PathLength : public FArcUtilityConsideration
{
	GENERATED_BODY()

	/** Maximum path length for normalization. Paths longer than this score 0. */
	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 1.0))
	float MaxPathLength = 10000.0f;

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;

#if WITH_EDITOR
	virtual EArcUtilityConsiderationRequirements GetRequirements() const override
	{
		return EArcUtilityConsiderationRequirements::TargetLocation;
	}
#endif
};

/**
 * Binary check: returns 1.0 if a navigation path exists to the target, 0.0 otherwise.
 */
USTRUCT(BlueprintType, DisplayName = "Path Exists Consideration")
struct ARCAI_API FArcUtilityConsideration_PathExists : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;

#if WITH_EDITOR
	virtual EArcUtilityConsiderationRequirements GetRequirements() const override
	{
		return EArcUtilityConsiderationRequirements::TargetLocation;
	}
#endif
};
