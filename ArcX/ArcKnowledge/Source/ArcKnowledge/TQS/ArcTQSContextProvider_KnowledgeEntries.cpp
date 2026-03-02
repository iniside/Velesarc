// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSContextProvider_KnowledgeEntries.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeQuery.h"
#include "ArcKnowledgeFilter.h"
#include "Engine/World.h"

void FArcTQSContextProvider_KnowledgeEntries::GenerateContextLocations(
	const FArcTQSQueryContext& QueryContext,
	TArray<FVector>& OutLocations) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return;
	}

	const UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FArcKnowledgeQueryContext KnowledgeContext;
	KnowledgeContext.QueryOrigin = QueryContext.QuerierLocation;
	KnowledgeContext.World = World;
	KnowledgeContext.CurrentTime = World->GetTimeSeconds();
	KnowledgeContext.EntityManager = QueryContext.EntityManager;
	KnowledgeContext.QuerierEntity = QueryContext.QuerierEntity;

	FArcKnowledgeQuery Query;
	Query.TagQuery = TagQuery;
	Query.MaxResults = MaxResults;

	if (MaxDistance > 0.0f)
	{
		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_MaxDistance>();
		FilterStruct.GetMutable<FArcKnowledgeFilter_MaxDistance>().MaxDistance = MaxDistance;
		Query.Filters.Add(MoveTemp(FilterStruct));
	}

	TArray<FArcKnowledgeQueryResult> Results;
	Subsystem->QueryKnowledge(Query, KnowledgeContext, Results);

	OutLocations.Reserve(OutLocations.Num() + Results.Num());
	for (const FArcKnowledgeQueryResult& Result : Results)
	{
		OutLocations.Add(Result.Entry.Location);
	}
}
