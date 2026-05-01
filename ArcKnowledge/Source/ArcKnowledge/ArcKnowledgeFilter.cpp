// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeFilter.h"
#include "ArcKnowledgeQueryCandidate.h"
#include "ArcKnowledgeQuery.h"

bool FArcKnowledgeFilter_MaxDistance::PassesFilter(const FArcKnowledgeQueryCandidate& Candidate, const FArcKnowledgeQueryContext& Context) const
{
	const float DistSq = FVector::DistSquared(Candidate.Location, Context.QueryOrigin);
	return DistSq <= FMath::Square(MaxDistance);
}

bool FArcKnowledgeFilter_MinRelevance::PassesFilter(const FArcKnowledgeQueryCandidate& Candidate, const FArcKnowledgeQueryContext& Context) const
{
	return Candidate.Relevance >= MinRelevance;
}

bool FArcKnowledgeFilter_MaxAge::PassesFilter(const FArcKnowledgeQueryCandidate& Candidate, const FArcKnowledgeQueryContext& Context) const
{
	const double Age = Context.CurrentTime - Candidate.Timestamp;
	return Age <= static_cast<double>(MaxAgeSeconds);
}

bool FArcKnowledgeFilter_PayloadType::PassesFilter(const FArcKnowledgeQueryCandidate& Candidate, const FArcKnowledgeQueryContext& Context) const
{
	if (!RequiredPayloadType)
	{
		return true;
	}
	return Candidate.Payload.IsValid() && Candidate.Payload.GetScriptStruct()->IsChildOf(RequiredPayloadType);
}

bool FArcKnowledgeFilter_NotClaimed::PassesFilter(const FArcKnowledgeQueryCandidate& Candidate, const FArcKnowledgeQueryContext& Context) const
{
	return !Candidate.bClaimed;
}
