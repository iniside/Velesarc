// Copyright Lukasz Baran. All Rights Reserved.

#include "Navigation/ArcMassInfluencePathFollowTask.h"
#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "Mass/Debugger.h"
#include "MassDebuggerWorld.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Navigation/ArcPathFollow.h"
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

bool FArcMassInfluencePathFollowTask::RequestPath(FStateTreeExecutionContext& Context, const FMassTargetLocation& InTargetLocation) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FArcMassInfluencePathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcMassInfluencePathFollowTaskInstanceData>(*this);

	bool bDisplayDebug = false;
#if WITH_MASSGAMEPLAY_DEBUG
	bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(MassContext.GetEntity());
#endif

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
	Params.NavigationFilterClass = NavigationFilterClass;

	return ArcPathFollow::RequestPath(PathContext, Params, InTargetLocation.EndOfPathPosition.GetValue(), bDisplayDebug);
}

bool FArcMassInfluencePathFollowTask::UpdateShortPath(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FArcMassInfluencePathFollowTaskInstanceData& InstanceData = Context.GetInstanceData<FArcMassInfluencePathFollowTaskInstanceData>(*this);

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
		MASSBEHAVIOR_LOGF(Verbose, "EnterState");
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
		MASSBEHAVIOR_LOGF(Verbose, "Tick");
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
