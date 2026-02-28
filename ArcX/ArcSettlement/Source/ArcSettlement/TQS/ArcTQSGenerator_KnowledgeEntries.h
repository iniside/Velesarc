// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "GameplayTagContainer.h"
#include "ArcTQSGenerator_KnowledgeEntries.generated.h"

/**
 * TQS Generator that produces target items from knowledge query results.
 * Bridges the settlement knowledge system into TQS for spatial scoring.
 *
 * Each matching knowledge entry becomes a Location-type TQS target item.
 * The knowledge query's own scoring is optionally used as the initial TQS score.
 */
USTRUCT(DisplayName = "Knowledge Entries")
struct ARCSETTLEMENT_API FArcTQSGenerator_KnowledgeEntries : public FArcTQSGenerator
{
	GENERATED_BODY()

	/** Tag query to filter knowledge entries. */
	UPROPERTY(EditAnywhere, Category = "Knowledge")
	FGameplayTagQuery TagQuery;

	/** Maximum search radius from query origin. 0 = unlimited. */
	UPROPERTY(EditAnywhere, Category = "Knowledge", meta = (ClampMin = 0.0))
	float MaxDistance = 0.0f;

	/** Maximum number of knowledge results to generate items from. */
	UPROPERTY(EditAnywhere, Category = "Knowledge", meta = (ClampMin = 1))
	int32 MaxResults = 20;

	/** If true, only include entries from the querier's settlement. */
	UPROPERTY(EditAnywhere, Category = "Knowledge")
	bool bSameSettlementOnly = false;

	/** If true, use the knowledge query score as the initial TQS item score. */
	UPROPERTY(EditAnywhere, Category = "Knowledge")
	bool bUseKnowledgeScoreAsInitial = false;

	virtual void GenerateItems(
		const FArcTQSQueryContext& QueryContext,
		TArray<FArcTQSTargetItem>& OutItems) const override;
};
