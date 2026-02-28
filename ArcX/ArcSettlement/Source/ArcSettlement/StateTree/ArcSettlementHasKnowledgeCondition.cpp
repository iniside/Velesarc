// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSettlementHasKnowledgeCondition.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntityFragments.h"
#include "ArcSettlementSubsystem.h"
#include "ArcKnowledgeQuery.h"
#include "ArcKnowledgeFilter.h"
#include "Mass/ArcSettlementFragments.h"
#include "StateTreeLinker.h"

bool FArcSettlementHasKnowledgeCondition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);
	return true;
}

bool FArcSettlementHasKnowledgeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return false;
	}

	const UArcSettlementSubsystem* Subsystem = World->GetSubsystem<UArcSettlementSubsystem>();
	if (!Subsystem)
	{
		return false;
	}

	const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	FArcKnowledgeQueryContext QueryContext;
	QueryContext.QueryOrigin = Transform.GetTransform().GetLocation();
	QueryContext.World = World;
	QueryContext.CurrentTime = World->GetTimeSeconds();
	QueryContext.EntityManager = &EntityManager;
	QueryContext.QuerierEntity = Entity;

	if (const FArcSettlementMemberFragment* MemberFrag = EntityManager.GetFragmentDataPtr<FArcSettlementMemberFragment>(Entity))
	{
		QueryContext.QuerierSettlement = MemberFrag->SettlementHandle;
	}

	// Run a minimal query — just tag match + same settlement filter, max 1 result
	FArcKnowledgeQuery Query;
	Query.TagQuery = InstanceData.TagQuery;
	Query.MaxResults = 1;

	// If the entity belongs to a settlement, filter to that settlement
	if (QueryContext.QuerierSettlement.IsValid())
	{
		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_SameSettlement>();
		Query.Filters.Add(MoveTemp(FilterStruct));
	}

	TArray<FArcKnowledgeQueryResult> Results;
	Subsystem->QueryKnowledge(Query, QueryContext, Results);

	return !Results.IsEmpty();
}
