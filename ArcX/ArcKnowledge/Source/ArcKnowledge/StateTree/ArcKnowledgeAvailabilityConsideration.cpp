// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeAvailabilityConsideration.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntityFragments.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeQuery.h"
#include "ArcKnowledgeFilter.h"

float FArcKnowledgeAvailabilityConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return 0.0f;
	}

	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
	if (!Transform)
	{
		return 0.0f;
	}

	FArcKnowledgeQueryContext QueryContext;
	QueryContext.QueryOrigin = Transform->GetTransform().GetLocation();
	QueryContext.World = World;
	QueryContext.CurrentTime = World->GetTimeSeconds();
	QueryContext.EntityManager = &EntityManager;
	QueryContext.QuerierEntity = Entity;

	FArcKnowledgeQuery Query;
	Query.TagQuery = InstanceData.TagQuery;
	Query.MaxResults = 5;

	if (InstanceData.MaxDistance > 0.0f)
	{
		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_MaxDistance>();
		FilterStruct.GetMutable<FArcKnowledgeFilter_MaxDistance>().MaxDistance = InstanceData.MaxDistance;
		Query.Filters.Add(MoveTemp(FilterStruct));
	}

	TArray<FArcKnowledgeQueryResult> Results;
	Subsystem->QueryKnowledge(Query, QueryContext, Results);

	if (Results.IsEmpty())
	{
		return 0.0f;
	}

	// Score: 1.0 if best result has score >= 0.5, scales linearly from there
	// Clamp to 0-1 range
	return FMath::Clamp(Results[0].Score * 2.0f, 0.0f, 1.0f);
}
