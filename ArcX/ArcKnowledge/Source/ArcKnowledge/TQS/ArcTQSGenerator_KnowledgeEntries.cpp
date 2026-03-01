// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_KnowledgeEntries.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeQuery.h"
#include "ArcKnowledgeFilter.h"
#include "MassEntityManager.h"
#include "Engine/World.h"

void FArcTQSGenerator_KnowledgeEntries::GenerateItems(
	const FArcTQSQueryContext& QueryContext,
	TArray<FArcTQSTargetItem>& OutItems) const
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

	// Build knowledge query context
	FArcKnowledgeQueryContext KnowledgeContext;
	KnowledgeContext.QueryOrigin = QueryContext.QuerierLocation;
	KnowledgeContext.World = World;
	KnowledgeContext.CurrentTime = World->GetTimeSeconds();
	KnowledgeContext.EntityManager = QueryContext.EntityManager;
	KnowledgeContext.QuerierEntity = QueryContext.QuerierEntity;

	// Build knowledge query
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

	// Execute knowledge query
	TArray<FArcKnowledgeQueryResult> Results;
	Subsystem->QueryKnowledge(Query, KnowledgeContext, Results);

	// Convert results to TQS target items
	OutItems.Reserve(OutItems.Num() + Results.Num());

	for (const FArcKnowledgeQueryResult& Result : Results)
	{
		FArcTQSTargetItem Item;
		Item.TargetType = EArcTQSTargetType::Location;
		Item.Location = Result.Entry.Location;

		if (bUseKnowledgeScoreAsInitial)
		{
			Item.Score = Result.Score;
		}

		// If the knowledge entry has a source entity, expose it
		if (Result.Entry.SourceEntity.IsSet())
		{
			Item.EntityHandle = Result.Entry.SourceEntity;
			Item.TargetType = EArcTQSTargetType::MassEntity;
		}

		OutItems.Add(MoveTemp(Item));
	}
}
