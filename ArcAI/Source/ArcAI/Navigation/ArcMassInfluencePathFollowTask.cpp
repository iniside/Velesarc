// Copyright Lukasz Baran. All Rights Reserved.

#include "Navigation/ArcMassInfluencePathFollowTask.h"
#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "NavCorridor.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavigationSystem.h"
#include "StateTreeLinker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassInfluencePathFollowTask)

// ---------------------------------------------------------------------------
// Link
// ---------------------------------------------------------------------------

bool FArcMassInfluencePathFollowTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(AgentRadiusHandle);
	Linker.LinkExternalData(AgentHeightHandle);
	Linker.LinkExternalData(DesiredMovementHandle);
	Linker.LinkExternalData(MovementParamsHandle);
	Linker.LinkExternalData(CachedPathHandle);
	Linker.LinkExternalData(ShortPathHandle);

	return true;
}

// ---------------------------------------------------------------------------
// RequestPath — builds FPathFindingQuery with optional custom filter
// ---------------------------------------------------------------------------

bool FArcMassInfluencePathFollowTask::RequestPath(FStateTreeExecutionContext& Context, const FMassTargetLocation& InTargetLocation) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcMassInfluencePathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcMassInfluencePathFollowTaskInstanceData>(*this);

	bool bDisplayDebug = false;
#if WITH_MASSGAMEPLAY_DEBUG
	bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(MassContext.GetEntity());
#endif

	const FAgentRadiusFragment& AgentRadius = Context.GetExternalData(AgentRadiusHandle);
	const FAgentHeightFragment& AgentHeight = Context.GetExternalData(AgentHeightHandle);
	const FVector AgentNavLocation = Context.GetExternalData(TransformHandle).GetTransform().GetLocation();
	const FNavAgentProperties NavAgentProperties(AgentRadius.Radius, AgentHeight.Height);

	UNavigationSystemV1* NavSys = Cast<UNavigationSystemV1>(Context.GetWorld()->GetNavigationSystem());
	if (!NavSys)
	{
		MASSBEHAVIOR_LOG(Warning, TEXT("Missing navigation system."));
		return false;
	}

	const ANavigationData* NavData = NavSys->GetNavDataForProps(NavAgentProperties, AgentNavLocation);
	if (!NavData || !InTargetLocation.EndOfPathPosition.IsSet())
	{
		MASSBEHAVIOR_LOG(Warning, TEXT("%s"), !NavData ? TEXT("Invalid NavData") : TEXT("EndOfPathPosition not set"));
		return false;
	}

	// Resolve the query filter — nullptr falls back to navmesh default.
	FSharedConstNavQueryFilter QueryFilter;
	if (*NavigationFilterClass)
	{
		QueryFilter = UNavigationQueryFilter::GetQueryFilter(*NavData, NavSys, NavigationFilterClass);
	}

	FPathFindingQuery Query(NavSys, *NavData, AgentNavLocation, InTargetLocation.EndOfPathPosition.GetValue(), QueryFilter);

	if (!Query.NavData.IsValid())
	{
		Query.NavData = NavSys->GetNavDataForProps(NavAgentProperties, Query.StartLocation);
	}

	FPathFindingResult Result(ENavigationQueryResult::Error);
	if (Query.NavData.IsValid())
	{
		if (bDisplayDebug)
		{
			MASSBEHAVIOR_LOG(Verbose, TEXT("Requesting synchronous path (filter: %s)"),
				*NavigationFilterClass ? *NavigationFilterClass->GetName() : TEXT("Default"));
		}

		Result = Query.NavData->FindPath(NavAgentProperties, Query);
	}

	if (Result.IsSuccessful())
	{
		Result.Path->RemoveOverlappingPoints(FNavCorridor::OverlappingPointTolerance);

		if (Result.Path.Get()->GetPathPoints().Num() > 1)
		{
			if (bDisplayDebug)
			{
				MASSBEHAVIOR_LOG(Verbose, TEXT("Path found"));
			}

			FMassNavMeshCachedPathFragment& CachedPathFragment = Context.GetExternalData(CachedPathHandle);
			CachedPathFragment.NavPath = Result.Path;
			CachedPathFragment.PathSource = EMassNavigationPathSource::NavMesh;

			// Build corridor
			CachedPathFragment.Corridor = MakeShared<FNavCorridor>();
			const FSharedConstNavQueryFilter CorridorFilter = Query.QueryFilter ? Query.QueryFilter : NavData->GetDefaultQueryFilter();

			FNavCorridorParams CorridorParams;
			CorridorParams.SetFromWidth(InstanceData.CorridorWidth);
			CorridorParams.PathOffsetFromBoundaries = InstanceData.OffsetFromBoundaries;

			CachedPathFragment.Corridor->BuildFromPath(*CachedPathFragment.NavPath, CorridorFilter, CorridorParams);

			// Update short path
			FMassNavMeshShortPathFragment& ShortPathFragment = Context.GetExternalData(ShortPathHandle);
			ShortPathFragment.RequestShortPath(CachedPathFragment.Corridor, 0, 0, InstanceData.EndDistanceThreshold);

			CachedPathFragment.NavPathNextStartIndex = (uint16)FMath::Max(
				ShortPathFragment.NumPoints - FMassNavMeshShortPathFragment::NumPointsBeyondUpdate - FMassNavMeshCachedPathFragment::NumLeadingPoints, 0);

			// Update MoveTarget
			FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
			const FMassMovementParameters& MovementParams = Context.GetExternalData(MovementParamsHandle);
			float DesiredSpeed = FMath::Min(
				MovementParams.GenerateDesiredSpeed(InstanceData.MovementStyle, MassContext.GetEntity().Index) * InstanceData.SpeedScale,
				MovementParams.MaxSpeed);

			const FMassDesiredMovementFragment& DesiredMovementFragment = Context.GetExternalData(DesiredMovementHandle);
			DesiredSpeed = FMath::Min(DesiredSpeed, DesiredMovementFragment.DesiredMaxSpeedOverride);

			MoveTarget.DesiredSpeed.Set(DesiredSpeed);
			MoveTarget.CreateNewAction(EMassMovementAction::Move, *Context.GetWorld());

			return true;
		}

		return true;
	}

	MASSBEHAVIOR_LOG(Warning, TEXT("Failed to find a path, result: %s (start: %s, end: %s)"),
		*UEnum::GetValueAsString(Result.Result), *Query.StartLocation.ToCompactString(), *Query.EndLocation.ToCompactString());

	return false;
}

// ---------------------------------------------------------------------------
// UpdateShortPath
// ---------------------------------------------------------------------------

bool FArcMassInfluencePathFollowTask::UpdateShortPath(FStateTreeExecutionContext& Context) const
{
	FArcMassInfluencePathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcMassInfluencePathFollowTaskInstanceData>(*this);

	FMassNavMeshCachedPathFragment& CachedPathFragment = Context.GetExternalData(CachedPathHandle);
	MASSBEHAVIOR_LOG(Verbose, TEXT("Updating short path, starting at index %i"), CachedPathFragment.NavPathNextStartIndex);

	FMassNavMeshShortPathFragment& ShortPathFragment = Context.GetExternalData(ShortPathHandle);
	ShortPathFragment.RequestShortPath(CachedPathFragment.Corridor, CachedPathFragment.NavPathNextStartIndex, CachedPathFragment.NumLeadingPoints, InstanceData.EndDistanceThreshold);

	CachedPathFragment.NavPathNextStartIndex += (uint16)FMath::Max(
		ShortPathFragment.NumPoints - FMassNavMeshShortPathFragment::NumPointsBeyondUpdate - FMassNavMeshCachedPathFragment::NumLeadingPoints, 0);

	return true;
}

// ---------------------------------------------------------------------------
// EnterState / Tick
// ---------------------------------------------------------------------------

EStateTreeRunStatus FArcMassInfluencePathFollowTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcMassInfluencePathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcMassInfluencePathFollowTaskInstanceData>(*this);

	bool bDisplayDebug = false;
#if WITH_MASSGAMEPLAY_DEBUG
	bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(MassContext.GetEntity());
#endif
	if (bDisplayDebug)
	{
		MASSBEHAVIOR_LOG(Verbose, TEXT("EnterState"));
	}

	if (!InstanceData.TargetLocation.EndOfPathPosition.IsSet())
	{
		MASSBEHAVIOR_LOG(Error, TEXT("Target is not defined."));
		return EStateTreeRunStatus::Failed;
	}

	if (!RequestPath(Context, InstanceData.TargetLocation))
	{
		MASSBEHAVIOR_LOG(Warning, TEXT("Failed to request path."));
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassInfluencePathFollowTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcMassInfluencePathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcMassInfluencePathFollowTaskInstanceData>(*this);

	FMassNavMeshShortPathFragment& ShortPathFragment = Context.GetExternalData(ShortPathHandle);

	bool bDisplayDebug = false;
#if WITH_MASSGAMEPLAY_DEBUG
	bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(MassContext.GetEntity());
#endif
	if (bDisplayDebug)
	{
		MASSBEHAVIOR_LOG(Verbose, TEXT("Tick"));
	}

	if (InstanceData.bTrackGoalChange)
	{
		const FMassEntityManager& EM = MassContext.GetEntityManager();
		const FTransformFragment* TrackedEntityTransform = EM.GetFragmentDataPtr<FTransformFragment>(InstanceData.TrackedEntity.GetEntityHandle());
		if (TrackedEntityTransform)
		{
			FTransformFragment* MyTransform = MassContext.GetExternalDataPtr(TransformHandle);
			const float Distance = FVector::Distance(TrackedEntityTransform->GetTransform().GetLocation(), MyTransform->GetTransform().GetLocation());
			if (Distance >= InstanceData.MinGoalDistanceToRepath)
			{
				FMassTargetLocation TargetLocation;
				TargetLocation.EndOfPathPosition = TrackedEntityTransform->GetTransform().GetLocation();
				TargetLocation.EndOfPathIntent = InstanceData.TargetLocation.EndOfPathIntent;
				RequestPath(Context, TargetLocation);
				return EStateTreeRunStatus::Running;
			}
		}
	}

	if (ShortPathFragment.IsDone() && ShortPathFragment.bPartialResult)
	{
		if (!InstanceData.TargetLocation.EndOfPathPosition.IsSet())
		{
			MASSBEHAVIOR_LOG(Error, TEXT("Target is not defined."));
			return EStateTreeRunStatus::Failed;
		}

		if (!UpdateShortPath(Context))
		{
			MASSBEHAVIOR_LOG(Error, TEXT("Failed to update short path."));
			return EStateTreeRunStatus::Failed;
		}
	}

	return ShortPathFragment.IsDone() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

// ---------------------------------------------------------------------------
// Editor description
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FText FArcMassInfluencePathFollowTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FString FilterName = *NavigationFilterClass ? NavigationFilterClass->GetName() : TEXT("Default");
	if (InstanceDataView.IsValid())
	{
		const FArcMassInfluencePathFollowTaskInstanceData* InstanceData = InstanceDataView.GetPtr<FArcMassInfluencePathFollowTaskInstanceData>();
		return FText::Format(
			NSLOCTEXT("ArcAI", "InfluencePathFollowDesc", "Influence Path Follow (Speed={0}, Filter={1})"),
			FText::AsNumber(InstanceData->SpeedScale),
			FText::FromString(FilterName));
	}
	return FMassStateTreeTaskBase::GetDescription(ID, InstanceDataView, BindingLookup, Formatting);
}
#endif
