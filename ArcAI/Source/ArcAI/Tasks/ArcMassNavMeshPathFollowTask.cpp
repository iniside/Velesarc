// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassNavMeshPathFollowTask.h"
#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "Mass/Debugger.h"
#include "MassDebuggerWorld.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "Navigation/ArcPathFollow.h"
#include "StateTreeLinker.h"


bool FArcMassNavMeshPathFollowTask::Link(FStateTreeLinker& Linker)
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

bool FArcMassNavMeshPathFollowTask::RequestPath(FStateTreeExecutionContext& Context, const FMassTargetLocation& InTargetLocation) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FArcMassNavMeshPathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcMassNavMeshPathFollowTaskInstanceData>(*this);

	bool bDisplayDebug = false;
#if WITH_MASSGAMEPLAY_DEBUG
	bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(MassContext.GetEntity());
#endif // WITH_MASSGAMEPLAY_DEBUG

	if (!InTargetLocation.EndOfPathPosition.IsSet())
	{
		MASSBEHAVIOR_LOGF(Warning, "EndOfPathPosition not set");
		return false;
	}

	const ArcPathFollow::FContext PathContext
	{
		*Context.GetWorld(),
		MassContext.GetEntity(),
		Context.GetExternalData(TransformHandle),
		Context.GetExternalData(AgentRadiusHandle),
		Context.GetExternalData(AgentHeightHandle),
		Context.GetExternalData(DesiredMovementHandle),
		Context.GetExternalData(MovementParamsHandle),
		Context.GetExternalData(MoveTargetHandle),
		Context.GetExternalData(ShortPathHandle),
		Context.GetExternalData(CachedPathHandle)
	};

	ArcPathFollow::FParams Params;
	Params.MovementStyle = InstanceData.MovementStyle;
	Params.SpeedScale = InstanceData.SpeedScale;
	Params.CorridorWidth = InstanceData.CorridorWidth;
	Params.OffsetFromBoundaries = InstanceData.OffsetFromBoundaries;
	Params.EndDistanceThreshold = InstanceData.EndDistanceThreshold;

	return ArcPathFollow::RequestPath(PathContext, Params, InTargetLocation.EndOfPathPosition.GetValue(), bDisplayDebug);
}

bool FArcMassNavMeshPathFollowTask::UpdateShortPath(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FArcMassNavMeshPathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcMassNavMeshPathFollowTaskInstanceData>(*this);

	const ArcPathFollow::FContext PathContext
	{
		*Context.GetWorld(),
		MassContext.GetEntity(),
		Context.GetExternalData(TransformHandle),
		Context.GetExternalData(AgentRadiusHandle),
		Context.GetExternalData(AgentHeightHandle),
		Context.GetExternalData(DesiredMovementHandle),
		Context.GetExternalData(MovementParamsHandle),
		Context.GetExternalData(MoveTargetHandle),
		Context.GetExternalData(ShortPathHandle),
		Context.GetExternalData(CachedPathHandle)
	};

	ArcPathFollow::FParams Params;
	Params.EndDistanceThreshold = InstanceData.EndDistanceThreshold;

	return ArcPathFollow::UpdateShortPath(PathContext, Params);
}

EStateTreeRunStatus FArcMassNavMeshPathFollowTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcMassNavMeshPathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcMassNavMeshPathFollowTaskInstanceData>(*this);

	bool bDisplayDebug = false;
#if WITH_MASSGAMEPLAY_DEBUG
	bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(MassContext.GetEntity());
#endif // WITH_MASSGAMEPLAY_DEBUG
	if (bDisplayDebug)
	{
		MASSBEHAVIOR_LOGF(Verbose, "enterstate.");
	}

	if (!InstanceData.TargetLocation.EndOfPathPosition.IsSet())
	{
		MASSBEHAVIOR_LOGF(Error, "Target is not defined.");
		return EStateTreeRunStatus::Failed;
	}
		
	if (!RequestPath(Context, InstanceData.TargetLocation))
	{
		MASSBEHAVIOR_LOGF(Warning, "Failed to request path.");
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassNavMeshPathFollowTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	FArcMassNavMeshPathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcMassNavMeshPathFollowTaskInstanceData>(*this);

	FMassNavMeshShortPathFragment& ShortPathFragment = Context.GetExternalData(ShortPathHandle);
	
	bool bDisplayDebug = false;
#if WITH_MASSGAMEPLAY_DEBUG
	bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(MassContext.GetEntity());
#endif // WITH_MASSGAMEPLAY_DEBUG
	if (bDisplayDebug)
	{
		MASSBEHAVIOR_LOGF(Verbose, "tick");
	}
	
	if (InstanceData.bTrackGoalChange)
	{
		const FMassEntityManager& EM = MassContext.GetEntityManager();
		const FTransformFragment* TrackedEntityTransform = EM.GetFragmentDataPtr<FTransformFragment>(InstanceData.TrackedEntity.GetEntityHandle());
		if (TrackedEntityTransform)
		{
			FTransformFragment* MyTransform = MassContext.GetExternalDataPtr(TransformHandle);
			float Distance = FVector::Distance(TrackedEntityTransform->GetTransform().GetLocation(), MyTransform->GetTransform().GetLocation());
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
	
	// Current path follow is done, but it was partial (i.e. many points on a curve), try again until we get there.
	if (ShortPathFragment.IsDone() && ShortPathFragment.bPartialResult)
	{
		if (!InstanceData.TargetLocation.EndOfPathPosition.IsSet())
		{
			MASSBEHAVIOR_LOGF(Error, "Target is not defined.");
			return EStateTreeRunStatus::Failed;
		}

		if (!UpdateShortPath(Context))
		{
			MASSBEHAVIOR_LOGF(Error, "Failed to update short path.");
			return EStateTreeRunStatus::Failed;
		}
	}
	
	return ShortPathFragment.IsDone() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FArcMassNavMeshPathFollowTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FArcMassNavMeshPathFollowTaskInstanceData* InstanceData = InstanceDataView.GetPtr<FArcMassNavMeshPathFollowTaskInstanceData>();
		return FText::Format(NSLOCTEXT("ArcAI", "NavMeshPathFollowDesc", "Follow NavMesh Path (Speed={0})"), FText::AsNumber(InstanceData->SpeedScale));
	}
	return FMassStateTreeTaskBase::GetDescription(ID, InstanceDataView, BindingLookup, Formatting);
}
#endif
