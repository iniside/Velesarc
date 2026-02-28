// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeFilter.h"
#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeQuery.h"

bool FArcKnowledgeFilter_MaxDistance::PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
{
	const float DistSq = FVector::DistSquared(Entry.Location, Context.QueryOrigin);
	return DistSq <= FMath::Square(MaxDistance);
}

bool FArcKnowledgeFilter_SameSettlement::PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
{
	return Entry.Settlement.IsValid() && Context.QuerierSettlement.IsValid()
		&& Entry.Settlement == Context.QuerierSettlement;
}

bool FArcKnowledgeFilter_MinRelevance::PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
{
	return Entry.Relevance >= MinRelevance;
}

bool FArcKnowledgeFilter_MaxAge::PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
{
	const double Age = Context.CurrentTime - Entry.Timestamp;
	return Age <= static_cast<double>(MaxAgeSeconds);
}

bool FArcKnowledgeFilter_PayloadType::PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
{
	if (!RequiredPayloadType)
	{
		return true;
	}
	return Entry.Payload.IsValid() && Entry.Payload.GetScriptStruct()->IsChildOf(RequiredPayloadType);
}

bool FArcKnowledgeFilter_NotClaimed::PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
{
	return !Entry.bClaimed;
}
