// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcKnowledgeScorer.generated.h"

struct FArcKnowledgeEntry;
struct FArcKnowledgeQueryContext;

/**
 * Base for knowledge query scorers. Subtype this to create custom scorers.
 * Scorers return 0-1 normalized values. Scores are combined multiplicatively
 * after applying the response curve and weight exponent.
 */
USTRUCT(BlueprintType, meta = (ExcludeBaseStruct))
struct ARCSETTLEMENT_API FArcKnowledgeScorer
{
	GENERATED_BODY()

	virtual ~FArcKnowledgeScorer() = default;

	/** Weight exponent for compensatory scoring. <1 softens, >1 sharpens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scorer", meta = (ClampMin = 0.01))
	float Weight = 1.0f;

	/** Return a 0-1 normalized score for the entry. */
	virtual float Score(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
	{
		return 1.0f;
	}
};

/** Scores by distance — closer entries score higher. */
USTRUCT(BlueprintType, DisplayName = "Distance")
struct ARCSETTLEMENT_API FArcKnowledgeScorer_Distance : public FArcKnowledgeScorer
{
	GENERATED_BODY()

	/** Distance at which the score reaches 0. Entries beyond this still pass (scoring, not filtering). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scorer", meta = (ClampMin = 1.0))
	float MaxDistance = 10000.0f;

	virtual float Score(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const override;
};

/** Scores by relevance — higher relevance scores higher. */
USTRUCT(BlueprintType, DisplayName = "Relevance")
struct ARCSETTLEMENT_API FArcKnowledgeScorer_Relevance : public FArcKnowledgeScorer
{
	GENERATED_BODY()

	virtual float Score(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const override;
};

/** Scores by freshness — more recent entries score higher. */
USTRUCT(BlueprintType, DisplayName = "Freshness")
struct ARCSETTLEMENT_API FArcKnowledgeScorer_Freshness : public FArcKnowledgeScorer
{
	GENERATED_BODY()

	/** Time in seconds after which an entry scores 0 for freshness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scorer", meta = (ClampMin = 1.0))
	float HalfLifeSeconds = 300.0f;

	virtual float Score(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const override;
};

/**
 * Scores knowledge entries by querying spatial influence at and around the entry's location.
 * Bridges ArcSettlement knowledge queries with ArcMass influence maps.
 *
 * Queries influence in a radius around the entry's location (not just the exact cell),
 * so threats in adjacent cells are captured.
 *
 * Example: threat influence near a mine → low score (bInvert=true).
 */
USTRUCT(BlueprintType, DisplayName = "Influence")
struct ARCSETTLEMENT_API FArcKnowledgeScorer_Influence : public FArcKnowledgeScorer
{
	GENERATED_BODY()

	/** Which grid to query (index into UArcInfluenceSettings::Grids). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scorer")
	int32 GridIndex = 0;

	/** Which channel in the grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scorer")
	int32 Channel = 0;

	/** Radius around the entry's location to sample influence.
	  * Larger radius captures influence from nearby cells. 0 = point query (exact cell only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scorer", meta = (ClampMin = "0.0"))
	float QueryRadius = 2000.0f;

	/** Influence value that maps to score 1.0. Values above are clamped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scorer", meta = (ClampMin = "0.001"))
	float MaxInfluence = 1.0f;

	/** If true, higher influence = lower score. Use for threats. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scorer")
	bool bInvert = false;

	virtual float Score(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const override;
};
