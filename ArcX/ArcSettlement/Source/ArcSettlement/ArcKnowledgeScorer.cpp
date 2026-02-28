// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeScorer.h"
#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeQuery.h"
#include "ArcMassInfluenceMapping.h"
#include "Engine/World.h"

float FArcKnowledgeScorer_Distance::Score(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
{
	const float Dist = FVector::Dist(Entry.Location, Context.QueryOrigin);
	if (MaxDistance <= 0.0f)
	{
		return 1.0f;
	}
	return FMath::Clamp(1.0f - (Dist / MaxDistance), 0.0f, 1.0f);
}

float FArcKnowledgeScorer_Relevance::Score(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
{
	return FMath::Clamp(Entry.Relevance, 0.0f, 1.0f);
}

float FArcKnowledgeScorer_Freshness::Score(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
{
	const double Age = Context.CurrentTime - Entry.Timestamp;
	if (Age <= 0.0)
	{
		return 1.0f;
	}
	// Exponential decay: score = 2^(-age/halflife)
	return FMath::Clamp(static_cast<float>(FMath::Pow(2.0, -Age / static_cast<double>(HalfLifeSeconds))), 0.0f, 1.0f);
}

float FArcKnowledgeScorer_Influence::Score(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
{
	const UWorld* World = Context.World.Get();
	if (!World)
	{
		return bInvert ? 1.0f : 0.0f;
	}

	const UArcInfluenceMappingSubsystem* InfluenceSub = World->GetSubsystem<UArcInfluenceMappingSubsystem>();
	if (!InfluenceSub || GridIndex >= InfluenceSub->GetGridCount())
	{
		return bInvert ? 1.0f : 0.0f;
	}

	float Influence;
	if (QueryRadius > 0.0f)
	{
		Influence = InfluenceSub->QueryInfluenceInRadius(GridIndex, Entry.Location, QueryRadius, Channel);
	}
	else
	{
		Influence = InfluenceSub->QueryInfluence(GridIndex, Entry.Location, Channel);
	}

	const float NormalizedScore = FMath::Clamp(Influence / MaxInfluence, 0.0f, 1.0f);
	return bInvert ? (1.0f - NormalizedScore) : NormalizedScore;
}
