// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSLocationProvider.h"
#include "ArcTQSStep_Direction.generated.h"

/**
 * Scores or filters items based on directional alignment.
 *
 * Computes how well the direction from the querier to each item aligns with
 * the direction from the querier to a reference location. Uses dot product
 * mapped from [-1, 1] to [0, 1].
 *
 * Configure LocationConfig to set the reference location source.
 * Note: bIncludeQuerierLocation is not useful here (querier-to-querier gives 0.5).
 * Set AdditionalSource to ReferenceLocation or CustomProvider for meaningful results.
 */
USTRUCT(DisplayName = "Direction Score")
struct ARCTARGETQUERY_API FArcTQSStep_Direction : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Direction()
	{
		StepType = EArcTQSStepType::Score;
		LocationConfig.bIncludeQuerierLocation = false;
		LocationConfig.AdditionalSource = EArcTQSLocationSource::ReferenceLocation;
	}

	/** Reference location configuration for direction scoring. */
	UPROPERTY(EditAnywhere, Category = "Step")
	FArcTQSLocationConfig LocationConfig;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
