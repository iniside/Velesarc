// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSGenerator_KnowledgeEntries.h"
#include "ArcSettlementSubsystem.h"
#include "ArcKnowledgeQuery.h"
#include "ArcKnowledgeFilter.h"
#include "MassEntityManager.h"
#include "Engine/World.h"
#include "Mass/ArcSettlementFragments.h"

void FArcTQSGenerator_KnowledgeEntries::GenerateItems(
	const FArcTQSQueryContext& QueryContext,
	TArray<FArcTQSTargetItem>& OutItems) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return;
	}

	const UArcSettlementSubsystem* Subsystem = World->GetSubsystem<UArcSettlementSubsystem>();
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

	// Try to get the querier's settlement
	if (QueryContext.EntityManager && QueryContext.QuerierEntity.IsSet())
	{
		if (const FArcSettlementMemberFragment* MemberFrag =
			QueryContext.EntityManager->GetFragmentDataPtr<FArcSettlementMemberFragment>(QueryContext.QuerierEntity))
		{
			KnowledgeContext.QuerierSettlement = MemberFrag->SettlementHandle;
		}
	}

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

	if (bSameSettlementOnly && KnowledgeContext.QuerierSettlement.IsValid())
	{
		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_SameSettlement>();
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
