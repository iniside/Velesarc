// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "ArcSettlementTypes.h"
#include "ArcKnowledgeQueryDefinition.generated.h"

/**
 * Reusable data asset defining a knowledge query pipeline.
 * NPCs reference these in their StateTree tasks.
 */
UCLASS(BlueprintType, Blueprintable)
class ARCSETTLEMENT_API UArcKnowledgeQueryDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Tag query for initial fast rejection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Query")
	FGameplayTagQuery TagQuery;

	/** Filter pipeline — discard entries that don't pass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Query", meta = (BaseStruct = "/Script/ArcSettlement.ArcKnowledgeFilter"))
	TArray<FInstancedStruct> Filters;

	/** Scorer pipeline — score entries 0-1, multiplicative. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Query", meta = (BaseStruct = "/Script/ArcSettlement.ArcKnowledgeScorer"))
	TArray<FInstancedStruct> Scorers;

	/** How many results to return. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = 1))
	int32 MaxResults = 1;

	/** How to select from scored results. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection")
	EArcKnowledgeSelectionMode SelectionMode = EArcKnowledgeSelectionMode::HighestScore;
};
