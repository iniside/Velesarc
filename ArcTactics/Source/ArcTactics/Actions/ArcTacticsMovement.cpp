// Copyright Lukasz Baran. All Rights Reserved.

#include "Actions/ArcTacticsMovement.h"

#include "ArcTQSQuerySubsystem.h"
#include "Debugger/ArcTacticsTrace.h"
#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassMovementTypes.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassNavMeshNavigationUtils.h"
#include "Navigation/ArcPathFollow.h"

namespace ArcTacticsMovementPrivate
{
	ArcPathFollow::FParams MakePathParams(const FArcTacticsMovementSettings& Settings)
	{
		ArcPathFollow::FParams Params;
		Params.MovementStyle = Settings.MovementStyle;
		Params.SpeedScale = Settings.SpeedScale;
		Params.CorridorWidth = Settings.CorridorWidth;
		Params.OffsetFromBoundaries = Settings.OffsetFromBoundaries;
		Params.EndDistanceThreshold = Settings.EndDistanceThreshold;
		Params.StopShortDistance = FMath::Max(0.0f, Settings.ArrivalRange - Settings.EndDistanceThreshold);
		Params.VisibilityGridIndex = Settings.VisibilityGridIndex;
		Params.VisibilityCostMultiplier = Settings.VisibilityCostMultiplier;
		return Params;
	}

	bool OwnsAction(
		const uint16 ActionID,
		const FMassMoveTargetFragment& MoveTarget,
		const EMassMovementAction Action)
	{
		return MoveTarget.GetCurrentAction() == Action
			&& MoveTarget.GetCurrentActionID() == ActionID;
	}

	bool ActivateStand(
		const ArcPathFollow::FContext& PathContext,
		FArcTacticsMovementRuntimeData& RuntimeData,
		FArcTacticsMovementHoldState& HoldState,
		const bool bOwnStand)
	{
		PathContext.MoveTarget.Center = PathContext.Transform.GetTransform().GetLocation();
		PathContext.MoveTarget.CreateNewAction(EMassMovementAction::Stand, PathContext.World);
		RuntimeData.OwnedActionID = 0;
		RuntimeData.bOwnsMove = false;
		RuntimeData.bOwnsStand = false;
		HoldState = FArcTacticsMovementHoldState();
		if (!UE::MassNavigation::ActivateActionStand(
			&PathContext.World,
			PathContext.Entity,
			PathContext.MovementParams.DefaultDesiredSpeed,
			PathContext.MoveTarget,
			PathContext.ShortPath))
		{
			return false;
		}

		RuntimeData.OwnedActionID = bOwnStand ? PathContext.MoveTarget.GetCurrentActionID() : 0;
		RuntimeData.bOwnsStand = bOwnStand;
		return true;
	}

	void SetArrivalIntent(
		const ArcPathFollow::FContext& PathContext,
		const FArcTacticsMovementRuntimeData& RuntimeData)
	{
		PathContext.MoveTarget.IntentAtGoal = PathContext.ShortPath.bPartialResult || RuntimeData.bNavPathPartial
			? EMassMovementAction::Move
			: EMassMovementAction::Stand;
	}

	EArcTacticsActionStatus RequestApproachPath(
		const ArcPathFollow::FContext& PathContext,
		const FArcTacticsMovementSettings& Settings,
		const FVector& TargetLocation,
		FArcTacticsMovementRuntimeData& RuntimeData,
		FArcTacticsMovementHoldState& HoldState,
		const uint32 ActionSerial,
		const bool bRepath)
	{
		const uint16 PreviousActionID = PathContext.MoveTarget.GetCurrentActionID();
		const ArcPathFollow::FParams Params = MakePathParams(Settings);
		ArcPathFollow::FRequestPathResult PathResult;
		const bool bPathFound = ArcPathFollow::RequestPath(PathContext, Params, TargetLocation, false, &PathResult);
		const bool bMoveStarted = bPathFound
			&& PathContext.MoveTarget.GetCurrentAction() == EMassMovementAction::Move
			&& PathContext.MoveTarget.GetCurrentActionID() != PreviousActionID;
		TRACE_ARCTACTICS_PATH_REQUESTED(
			&PathContext.World, PathContext.Entity, ActionSerial, bRepath,
			PathContext.Transform.GetTransform().GetLocation(), TargetLocation,
			bMoveStarted ? PathContext.CachedPath.NavPath.Get() : nullptr);
		if (bPathFound && PathResult.bAlreadyAtEnd)
		{
			return EArcTacticsActionStatus::Succeeded;
		}
		if (!bMoveStarted)
		{
			return EArcTacticsActionStatus::Failed;
		}

		RuntimeData.RequestedDestination = TargetLocation;
		RuntimeData.bNavPathPartial = PathResult.bNavPathPartial;
		RuntimeData.OwnedActionID = PathContext.MoveTarget.GetCurrentActionID();
		RuntimeData.bOwnsMove = true;
		RuntimeData.bOwnsStand = false;
		HoldState = FArcTacticsMovementHoldState();
		RuntimeData.bFacingStarted = false;
		if (PathContext.ShortPath.NumPoints == 0)
		{
			ArcTacticsMovement::Stop(PathContext, RuntimeData, HoldState);
			return EArcTacticsActionStatus::Failed;
		}
		SetArrivalIntent(PathContext, RuntimeData);
		return EArcTacticsActionStatus::Running;
	}

	bool IsFacingTarget(
		const FTransformFragment& Transform,
		const FVector& TargetLocation,
		const float ToleranceDegrees)
	{
		const FVector Location = Transform.GetTransform().GetLocation();
		FVector DesiredForward = TargetLocation - Location;
		DesiredForward.Z = 0.0;
		if (DesiredForward.IsNearlyZero())
		{
			return true;
		}

		DesiredForward.Normalize();
		FVector CurrentForward = Transform.GetTransform().GetRotation().GetForwardVector();
		CurrentForward.Z = 0.0;
		if (!CurrentForward.Normalize())
		{
			return false;
		}

		const double Dot = FMath::Clamp(FVector::DotProduct(CurrentForward, DesiredForward), -1.0, 1.0);
		return FMath::RadiansToDegrees(FMath::Acos(Dot)) <= ToleranceDegrees;
	}

	void FaceTarget(const ArcPathFollow::FContext& PathContext, const FVector& TargetLocation)
	{
		FVector DesiredForward = TargetLocation - PathContext.Transform.GetTransform().GetLocation();
		DesiredForward.Z = 0.0;
		if (DesiredForward.Normalize())
		{
			PathContext.MoveTarget.Forward = DesiredForward;
		}
	}
}

bool ArcTacticsMovement::Validate(
	const FArcTacticsMovementSettings& Settings,
	FString& OutError)
{
	if (!FMath::IsFinite(Settings.SpeedScale) || Settings.SpeedScale <= 0.0f
		|| !FMath::IsFinite(Settings.CorridorWidth) || Settings.CorridorWidth <= 0.0f
		|| !FMath::IsFinite(Settings.OffsetFromBoundaries) || Settings.OffsetFromBoundaries < 0.0f
		|| !FMath::IsFinite(Settings.EndDistanceThreshold) || Settings.EndDistanceThreshold < 0.0f
		|| !FMath::IsFinite(Settings.ArrivalRange) || Settings.ArrivalRange < 0.0f
		|| !FMath::IsFinite(Settings.RepathInterval) || Settings.RepathInterval <= 0.0f
		|| !FMath::IsFinite(Settings.RepathDistance) || Settings.RepathDistance < 0.0f
		|| !FMath::IsFinite(Settings.ApproachTimeout) || Settings.ApproachTimeout <= 0.0f
		|| !FMath::IsFinite(Settings.VisibilityCostMultiplier) || Settings.VisibilityCostMultiplier < 0.0f)
	{
		OutError = TEXT("Movement parameters must be finite and within their configured positive ranges.");
		return false;
	}

	return true;
}

bool ArcTacticsMovement::ValidateFacing(
	const FArcTacticsFacingSettings& Settings,
	FString& OutError)
{
	if (!FMath::IsFinite(Settings.FacingToleranceDegrees) || Settings.FacingToleranceDegrees < 0.0f
		|| Settings.FacingToleranceDegrees > 180.0f
		|| !FMath::IsFinite(Settings.FacingTimeout) || Settings.FacingTimeout <= 0.0f)
	{
		OutError = TEXT("Facing tolerance and timeout must be finite and within range.");
		return false;
	}

	return true;
}

EArcTacticsActionStatus ArcTacticsMovement::StartApproach(
	const ArcPathFollow::FContext& PathContext,
	const FArcTacticsMovementSettings& Settings,
	const FVector& TargetLocation,
	FArcTacticsMovementRuntimeData& RuntimeData,
	FArcTacticsMovementHoldState& HoldState,
	const uint32 ActionSerial)
{
	const double CurrentTime = PathContext.World.GetTimeSeconds();
	if (!RuntimeData.bApproachDeadlineSet)
	{
		RuntimeData.ApproachDeadline = CurrentTime + Settings.ApproachTimeout;
		RuntimeData.bApproachDeadlineSet = true;
	}
	if (CurrentTime >= RuntimeData.ApproachDeadline)
	{
		return EArcTacticsActionStatus::Failed;
	}

	RuntimeData.NextRepathTime = CurrentTime + Settings.RepathInterval;
	return ArcTacticsMovementPrivate::RequestApproachPath(
		PathContext, Settings, TargetLocation, RuntimeData, HoldState, ActionSerial, false);
}

EArcTacticsActionStatus ArcTacticsMovement::UpdateApproach(
	const ArcPathFollow::FContext& PathContext,
	const FArcTacticsMovementSettings& Settings,
	const FVector& TargetLocation,
	FArcTacticsMovementRuntimeData& RuntimeData,
	FArcTacticsMovementHoldState& HoldState,
	const uint32 ActionSerial)
{
	if (!ArcTacticsMovementPrivate::OwnsAction(
		RuntimeData.OwnedActionID, PathContext.MoveTarget, EMassMovementAction::Move))
	{
		return EArcTacticsActionStatus::Failed;
	}

	const double RepathDistanceSquared = static_cast<double>(Settings.RepathDistance) * Settings.RepathDistance;
	const double TargetDriftSquared = FVector::DistSquared(RuntimeData.RequestedDestination, TargetLocation);
	const bool bTargetMoved = TargetDriftSquared >= RepathDistanceSquared;
	const bool bPathDone = PathContext.ShortPath.IsDone() && !PathContext.ShortPath.bPartialResult;
	const bool bReachedPathEnd = bPathDone && !RuntimeData.bNavPathPartial;
	if (bReachedPathEnd && TargetDriftSquared <= RepathDistanceSquared)
	{
		return EArcTacticsActionStatus::Succeeded;
	}

	const double CurrentTime = PathContext.World.GetTimeSeconds();
	if (CurrentTime >= RuntimeData.ApproachDeadline)
	{
		return EArcTacticsActionStatus::Failed;
	}

	if (bReachedPathEnd
		|| (CurrentTime >= RuntimeData.NextRepathTime
			&& (bTargetMoved || (bPathDone && RuntimeData.bNavPathPartial))))
	{
		RuntimeData.NextRepathTime = CurrentTime + Settings.RepathInterval;
		return ArcTacticsMovementPrivate::RequestApproachPath(
			PathContext, Settings, TargetLocation, RuntimeData, HoldState, ActionSerial, true);
	}

	if (PathContext.ShortPath.IsDone() && PathContext.ShortPath.bPartialResult)
	{
		const ArcPathFollow::FParams Params = ArcTacticsMovementPrivate::MakePathParams(Settings);
		if (!ArcPathFollow::UpdateShortPath(PathContext, Params) || PathContext.ShortPath.NumPoints == 0)
		{
			return EArcTacticsActionStatus::Failed;
		}
		ArcTacticsMovementPrivate::SetArrivalIntent(PathContext, RuntimeData);
	}

	return EArcTacticsActionStatus::Running;
}

EArcTacticsActionStatus ArcTacticsMovement::StartFacing(
	const ArcPathFollow::FContext& PathContext,
	const FArcTacticsFacingSettings& Settings,
	const FVector& TargetLocation,
	FArcTacticsMovementRuntimeData& RuntimeData,
	FArcTacticsMovementHoldState& HoldState)
{
	if (ArcTacticsMovementPrivate::IsFacingTarget(
		PathContext.Transform, TargetLocation, Settings.FacingToleranceDegrees))
	{
		return EArcTacticsActionStatus::Succeeded;
	}

	if (RuntimeData.bOwnsMove
		&& !ArcTacticsMovementPrivate::OwnsAction(
			RuntimeData.OwnedActionID, PathContext.MoveTarget, EMassMovementAction::Move))
	{
		return EArcTacticsActionStatus::Failed;
	}

	if (!Stop(PathContext, RuntimeData, HoldState))
	{
		return EArcTacticsActionStatus::Failed;
	}

	ArcTacticsMovementPrivate::FaceTarget(PathContext, TargetLocation);
	RuntimeData.FacingDeadline = PathContext.World.GetTimeSeconds() + Settings.FacingTimeout;
	RuntimeData.bFacingStarted = true;
	return EArcTacticsActionStatus::Running;
}

EArcTacticsActionStatus ArcTacticsMovement::UpdateFacing(
	const ArcPathFollow::FContext& PathContext,
	const FArcTacticsFacingSettings& Settings,
	const FVector& TargetLocation,
	FArcTacticsMovementRuntimeData& RuntimeData)
{
	if (!ArcTacticsMovementPrivate::OwnsAction(
		RuntimeData.OwnedActionID, PathContext.MoveTarget, EMassMovementAction::Stand))
	{
		return EArcTacticsActionStatus::Failed;
	}

	ArcTacticsMovementPrivate::FaceTarget(PathContext, TargetLocation);

	if (ArcTacticsMovementPrivate::IsFacingTarget(
		PathContext.Transform, TargetLocation, Settings.FacingToleranceDegrees))
	{
		return EArcTacticsActionStatus::Succeeded;
	}

	return PathContext.World.GetTimeSeconds() >= RuntimeData.FacingDeadline
		? EArcTacticsActionStatus::Failed
		: EArcTacticsActionStatus::Running;
}

bool ArcTacticsMovement::Stop(
	const ArcPathFollow::FContext& PathContext,
	FArcTacticsMovementRuntimeData& RuntimeData,
	FArcTacticsMovementHoldState& HoldState)
{
	return ArcTacticsMovementPrivate::ActivateStand(PathContext, RuntimeData, HoldState, true);
}

bool ArcTacticsMovement::AcquireHold(
	const ArcPathFollow::FContext& PathContext,
	FArcTacticsMovementRuntimeData& RuntimeData,
	FArcTacticsMovementHoldState& HoldState)
{
	if (IsHoldOwned(PathContext, HoldState))
	{
		return true;
	}

	HoldState = FArcTacticsMovementHoldState();
	PathContext.MoveTarget.CreateNewAction(EMassMovementAction::Animate, PathContext.World);
	if (!UE::MassNavigation::ActivateActionAnimate(&PathContext.World, PathContext.Entity, PathContext.MoveTarget))
	{
		return false;
	}

	RuntimeData.OwnedActionID = 0;
	HoldState.ActionID = PathContext.MoveTarget.GetCurrentActionID();
	RuntimeData.bOwnsMove = false;
	RuntimeData.bOwnsStand = false;
	HoldState.bOwned = true;
	RuntimeData.bFacingStarted = false;
	return true;
}

bool ArcTacticsMovement::IsHoldOwned(
	const ArcPathFollow::FContext& PathContext,
	const FArcTacticsMovementHoldState& HoldState)
{
	return HoldState.bOwned
		&& ArcTacticsMovementPrivate::OwnsAction(
			HoldState.ActionID, PathContext.MoveTarget, EMassMovementAction::Animate);
}

bool ArcTacticsMovement::ReleaseHold(
	const ArcPathFollow::FContext& PathContext,
	FArcTacticsMovementRuntimeData& RuntimeData,
	FArcTacticsMovementHoldState& HoldState)
{
	if (!HoldState.bOwned)
	{
		return true;
	}

	const bool bOwnsHold = IsHoldOwned(PathContext, HoldState);
	HoldState = FArcTacticsMovementHoldState();
	if (!bOwnsHold)
	{
		return true;
	}

	return ArcTacticsMovementPrivate::ActivateStand(PathContext, RuntimeData, HoldState, false);
}

void ArcTacticsMovement::AbortDestinationQuery(
	UArcTQSQuerySubsystem* QuerySubsystem,
	FArcTacticsMovementRuntimeData& RuntimeData)
{
	if (QuerySubsystem && RuntimeData.DestinationQueryID != INDEX_NONE)
	{
		QuerySubsystem->AbortQuery(RuntimeData.DestinationQueryID);
	}
	RuntimeData.DestinationQueryID = INDEX_NONE;
	RuntimeData.DestinationQueryAnchor = FVector::ZeroVector;
	RuntimeData.ResolvedDestination = FVector::ZeroVector;
	RuntimeData.bHasDestinationQueryAnchor = false;
	RuntimeData.bHasResolvedDestination = false;
}

void ArcTacticsMovement::ResetActionScratch(FArcTacticsMovementRuntimeData& RuntimeData)
{
	RuntimeData = FArcTacticsMovementRuntimeData();
}

void ArcTacticsMovement::CancelAction(
	const ArcPathFollow::FContext& PathContext,
	FArcTacticsMovementRuntimeData& RuntimeData)
{
	const bool bOwnsMove = RuntimeData.bOwnsMove
		&& ArcTacticsMovementPrivate::OwnsAction(
			RuntimeData.OwnedActionID, PathContext.MoveTarget, EMassMovementAction::Move);
	const bool bOwnsStand = RuntimeData.bOwnsStand
		&& ArcTacticsMovementPrivate::OwnsAction(
			RuntimeData.OwnedActionID, PathContext.MoveTarget, EMassMovementAction::Stand);
	if (bOwnsMove || bOwnsStand)
	{
		FArcTacticsMovementHoldState EmptyHold;
		ArcTacticsMovementPrivate::ActivateStand(PathContext, RuntimeData, EmptyHold, false);
	}
	ResetActionScratch(RuntimeData);
}

void ArcTacticsMovement::Cancel(
	const ArcPathFollow::FContext& PathContext,
	FArcTacticsMovementRuntimeData& RuntimeData,
	FArcTacticsMovementHoldState& HoldState)
{
	const bool bOwnsMove = RuntimeData.bOwnsMove
		&& ArcTacticsMovementPrivate::OwnsAction(
			RuntimeData.OwnedActionID, PathContext.MoveTarget, EMassMovementAction::Move);
	const bool bOwnsStand = RuntimeData.bOwnsStand
		&& ArcTacticsMovementPrivate::OwnsAction(
			RuntimeData.OwnedActionID, PathContext.MoveTarget, EMassMovementAction::Stand);
	const bool bOwnsHold = HoldState.bOwned
		&& ArcTacticsMovementPrivate::OwnsAction(
			HoldState.ActionID, PathContext.MoveTarget, EMassMovementAction::Animate);
	if (bOwnsMove || bOwnsStand || bOwnsHold)
	{
		ArcTacticsMovementPrivate::ActivateStand(PathContext, RuntimeData, HoldState, false);
	}
	RuntimeData = FArcTacticsMovementRuntimeData();
	HoldState = FArcTacticsMovementHoldState();
}
