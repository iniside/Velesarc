// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassQueryKnowledgeTask.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntityFragments.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeQueryDefinition.h"
#include "StateTreeLinker.h"

bool FArcMassQueryKnowledgeTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);
	return true;
}

EStateTreeRunStatus FArcMassQueryKnowledgeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
	const FVector QuerierLocation = Transform.GetTransform().GetLocation();

	// Build query context
	FArcKnowledgeQueryContext QueryContext;
	QueryContext.QueryOrigin = QuerierLocation;
	QueryContext.World = World;
	QueryContext.CurrentTime = World->GetTimeSeconds();

	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();
	QueryContext.QuerierEntity = Entity;
	QueryContext.EntityManager = &EntityManager;

	// Execute query
	InstanceData.ResultArray.Reset();
	InstanceData.bHasResults = false;

	if (InstanceData.bUseInlineDefinition)
	{
		FArcKnowledgeQuery InlineQuery;
		InlineQuery.TagQuery = InstanceData.InlineTagQuery;
		InlineQuery.Filters = InstanceData.InlineFilters;
		InlineQuery.Scorers = InstanceData.InlineScorers;
		InlineQuery.MaxResults = InstanceData.InlineMaxResults;
		InlineQuery.SelectionMode = InstanceData.InlineSelectionMode;

		Subsystem->QueryKnowledge(InlineQuery, QueryContext, InstanceData.ResultArray);
	}
	else if (InstanceData.QueryDefinition)
	{
		Subsystem->QueryKnowledgeFromDefinition(InstanceData.QueryDefinition, QueryContext, InstanceData.ResultArray);
	}

	InstanceData.bHasResults = !InstanceData.ResultArray.IsEmpty();
	if (InstanceData.bHasResults)
	{
		InstanceData.BestResult = InstanceData.ResultArray[0];
	}
	else
	{
		InstanceData.BestResult = FArcKnowledgeQueryResult();
	}

	return EStateTreeRunStatus::Succeeded;
}
