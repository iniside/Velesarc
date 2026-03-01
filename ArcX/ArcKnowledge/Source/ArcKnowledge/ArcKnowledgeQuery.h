// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "MassEntityTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeQuery.generated.h"

/**
 * Context passed to filters and scorers during query execution.
 * Contains the querier's state and world references.
 */
USTRUCT(BlueprintType)
struct ARCKNOWLEDGE_API FArcKnowledgeQueryContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Context")
	FVector QueryOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Context")
	FMassEntityHandle QuerierEntity;

	/** Generic context data for filters/scorers that need additional querier info. */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	FInstancedStruct QuerierContext;

	TWeakObjectPtr<UWorld> World;
	FMassEntityManager* EntityManager = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Context")
	double CurrentTime = 0.0;
};

/**
 * A knowledge query — find entries matching tags, filtered and scored by instanced struct pipelines.
 */
USTRUCT(BlueprintType)
struct ARCKNOWLEDGE_API FArcKnowledgeQuery
{
	GENERATED_BODY()

	/** Tag query for initial fast rejection. Only entries matching this are considered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	FGameplayTagQuery TagQuery;

	/** Filter pipeline — discard entries that don't pass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", meta = (BaseStruct = "/Script/ArcKnowledge.ArcKnowledgeFilter"))
	TArray<FInstancedStruct> Filters;

	/** Scorer pipeline — score entries 0-1, multiplicative. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", meta = (BaseStruct = "/Script/ArcKnowledge.ArcKnowledgeScorer"))
	TArray<FInstancedStruct> Scorers;

	/** How many results to return. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", meta = (ClampMin = 1))
	int32 MaxResults = 1;

	/** How to select from scored results. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	EArcKnowledgeSelectionMode SelectionMode = EArcKnowledgeSelectionMode::HighestScore;
};

/**
 * A single result from a knowledge query.
 */
USTRUCT(BlueprintType)
struct ARCKNOWLEDGE_API FArcKnowledgeQueryResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result")
	FArcKnowledgeEntry Entry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result")
	float Score = 0.0f;
};
