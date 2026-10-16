// Copyright Lukasz Baran. All Rights Reserved.

#include "Steps/ArcTacticsMoveToStep.h"

#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryInstance.h"
#include "ArcTQSQuerySubsystem.h"
#include "Combat/ArcCombatSlots.h"
#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassCommonFragments.h"
#include "Navigation/ArcPathFollow.h"
#include "Steps/ArcTacticsStepHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsMoveToStep)

namespace ArcTacticsMoveToStepPrivate
{
	void ResetMoveToScratch(FArcTacticsMoveToStepInstanceData& Data)
	{
		Data.MovementRuntime = FArcTacticsMovementRuntimeData();
		Data.bDestinationQueryCompleted = false;
		Data.bDestinationQuerySucceeded = false;
		Data.ClaimPushTime = 0.0;
		Data.LastRenewPushTime = 0.0;
		Data.bClaimPending = false;
		Data.bFollowingClaim = false;
	}

	bool IsClaimingSlot(const FArcTacticsActionContext& Context, const FArcTacticsMoveToStepInstanceData& Data)
	{
		return Data.bClaimCombatSlot && Data.DestinationQuery.IsValid() && Context.EntityManager
			&& Context.EntityManager->IsEntityActive(Data.Target);
	}

	void ForgetSlotQuery(FArcTacticsActionContext& Context, FArcTacticsMoveToStepInstanceData& Data)
	{
		ArcTacticsMovement::AbortDestinationQuery(Context.QuerySubsystem, Data.MovementRuntime);
		Data.bDestinationQueryCompleted = false;
		Data.bDestinationQuerySucceeded = false;
		Data.bClaimPending = false;
		Data.bFollowingClaim = false;
	}

	bool PushSlotClaim(FArcTacticsActionContext& Context, FArcTacticsMoveToStepInstanceData& Data, const double Now)
	{
		FVector Centre = FVector::ZeroVector;
		if (!ArcCombatSlots::GetTargetCentre(*Context.EntityManager, Data.Target, Centre))
		{
			return false;
		}
		const FVector ToSlot = Data.MovementRuntime.ResolvedDestination - Centre;
		const float YawDegrees = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(ToSlot.Y, ToSlot.X)));
		ArcCombatSlots::PushClaim(
			*Context.EntityManager,
			Data.Target,
			Context.Entity,
			YawDegrees,
			static_cast<float>(ToSlot.Size2D()),
			Data.SlotHoldDistance,
			Data.SlotMinSeparationDegrees,
			Data.SlotMaxClaimers,
			Now);
		Data.bClaimPending = true;
		Data.ClaimPushTime = Now;
		return true;
	}

	void RenewSlotClaim(FArcTacticsActionContext& Context, FArcTacticsMoveToStepInstanceData& Data, const double Now)
	{
		if (Now - Data.LastRenewPushTime < ArcCombatSlots::LeaseSeconds * 0.5)
		{
			return;
		}
		ArcCombatSlots::PushRenew(*Context.EntityManager, Data.Target, Context.Entity, Now);
		Data.LastRenewPushTime = Now;
	}

	bool StartDestinationQuery(FArcTacticsStepContext& StepContext, FArcTacticsMoveToStepInstanceData& Data)
	{
		FArcTacticsActionContext& Context = StepContext.Action;
		if (!Context.QuerySubsystem || !Data.DestinationQuery.IsValid() || !Context.AsyncCompletion.IsBound()
			|| !Context.RequestToken.InstanceSubsystem.IsValid()
			|| !Context.RequestToken.InstanceHandle.IsValid())
		{
			return false;
		}
		FArcTQSQueryContext QueryContext;
		QueryContext.QuerierEntity = Context.Entity;
		QueryContext.World = Context.World;
		QueryContext.EntityManager = Context.EntityManager;
		if (Context.PathContext)
		{
			QueryContext.QuerierLocation = Context.PathContext->Transform.GetTransform().GetLocation();
			QueryContext.QuerierForward = Context.PathContext->Transform.GetTransform().GetRotation().GetForwardVector();
		}
		FVector TargetLocation = FVector::ZeroVector;
		if (!ArcTacticsStepHelpers::ResolveTargetLocation(
			Context, Data.Target, Data.TargetLocation, Data.bUseTargetLocation, TargetLocation))
		{
			return false;
		}
		if (!Context.PathContext || !Context.HoldState)
		{
			return false;
		}
		if (!ArcTacticsMovement::IsHoldOwned(*Context.PathContext, *Context.HoldState)
			&& !ArcTacticsMovement::Stop(*Context.PathContext, Data.MovementRuntime, *Context.HoldState))
		{
			return false;
		}
		Data.MovementRuntime.DestinationQueryAnchor = TargetLocation;
		Data.MovementRuntime.bHasDestinationQueryAnchor = true;
		Data.bDestinationQueryCompleted = false;
		Data.bDestinationQuerySucceeded = false;
		const FArcTacticsAsyncRequestToken Token = StepContext.BeginRequest();
		const FArcTacticsActionAsyncCompletion Completion = Context.AsyncCompletion;
		FArcTQSQueryFinished Finished = FArcTQSQueryFinished::CreateLambda(
			[Completion, Token](FArcTQSQueryInstance& CompletedQuery)
			{
				Completion.ExecuteIfBound(Token, CompletedQuery);
			});
		Data.MovementRuntime.DestinationQueryID = Context.QuerySubsystem->RunQuery(
			Data.DestinationQuery, QueryContext, MoveTemp(Finished));
		return Data.MovementRuntime.DestinationQueryID != INDEX_NONE;
	}

	EArcTacticsActionStatus StopMoveTo(FArcTacticsActionContext& Context, FArcTacticsMoveToStepInstanceData& Data)
	{
		if (!Context.HoldState || !ArcTacticsMovement::Stop(*Context.PathContext, Data.MovementRuntime, *Context.HoldState))
		{
			return EArcTacticsActionStatus::Failed;
		}
		ArcTacticsMovement::AbortDestinationQuery(Context.QuerySubsystem, Data.MovementRuntime);
		ResetMoveToScratch(Data);
		return EArcTacticsActionStatus::Succeeded;
	}

	EArcTacticsActionStatus TickMoveTo(FArcTacticsStepContext& StepContext, FArcTacticsMoveToStepInstanceData& Data)
	{
		FArcTacticsActionContext& Context = StepContext.Action;
		if (!Context.PathContext)
		{
			return EArcTacticsActionStatus::Running;
		}
		FVector TargetLocation = FVector::ZeroVector;
		const bool bHasTargetLocation = ArcTacticsStepHelpers::ResolveTargetLocation(
			Context, Data.Target, Data.TargetLocation, Data.bUseTargetLocation, TargetLocation);
		const FVector BaseDestination = Data.bUseDestination ? Data.Destination : TargetLocation;
		if ((!Data.bUseDestination && !bHasTargetLocation) || BaseDestination.ContainsNaN())
		{
			return EArcTacticsActionStatus::Failed;
		}
		FVector Destination = BaseDestination;
		const bool bClaimSlot = IsClaimingSlot(Context, Data);
		bool bFollowSlot = false;
		if (bClaimSlot)
		{
			const double Now = Context.PathContext->World.GetTimeSeconds();
			FVector SlotPoint = FVector::ZeroVector;
			if (ArcCombatSlots::GetLiveSlotPoint(*Context.EntityManager, Context.Entity, Data.Target, Now, SlotPoint))
			{
				if (Data.MovementRuntime.DestinationQueryID != INDEX_NONE)
				{
					ArcTacticsMovement::AbortDestinationQuery(Context.QuerySubsystem, Data.MovementRuntime);
				}
				Data.bClaimPending = false;
				Data.bFollowingClaim = true;
				RenewSlotClaim(Context, Data, Now);
				Destination = SlotPoint;
				bFollowSlot = true;
			}
			else if (Data.bFollowingClaim
				|| (Data.bClaimPending && Now - Data.ClaimPushTime >= ArcCombatSlots::LeaseSeconds))
			{
				ForgetSlotQuery(Context, Data);
			}
		}
		if (Data.DestinationQuery.IsValid() && !bFollowSlot)
		{
			const FVector QueryAnchor = bHasTargetLocation ? TargetLocation : BaseDestination;
			if (Data.MovementRuntime.bHasDestinationQueryAnchor
				&& FVector::DistSquared(Data.MovementRuntime.DestinationQueryAnchor, QueryAnchor)
					> static_cast<double>(Data.Movement.RepathDistance) * Data.Movement.RepathDistance)
			{
				ArcTacticsMovement::AbortDestinationQuery(Context.QuerySubsystem, Data.MovementRuntime);
				ArcTacticsMovement::CancelAction(*Context.PathContext, Data.MovementRuntime);
				Data.bDestinationQueryCompleted = false;
				Data.bDestinationQuerySucceeded = false;
				Data.bClaimPending = false;
			}
			if (Data.MovementRuntime.DestinationQueryID == INDEX_NONE
				&& !Data.MovementRuntime.bHasResolvedDestination
				&& !Data.bDestinationQueryCompleted
				&& !StartDestinationQuery(StepContext, Data))
			{
				return EArcTacticsActionStatus::Failed;
			}
			if (Data.MovementRuntime.DestinationQueryID != INDEX_NONE || !Data.bDestinationQueryCompleted)
			{
				return EArcTacticsActionStatus::Running;
			}
			if (!Data.bDestinationQuerySucceeded || !Data.MovementRuntime.bHasResolvedDestination)
			{
				return EArcTacticsActionStatus::Failed;
			}
			Destination = Data.MovementRuntime.ResolvedDestination;
			if (bClaimSlot)
			{
				if (!Data.bClaimPending
					&& !PushSlotClaim(Context, Data, Context.PathContext->World.GetTimeSeconds()))
				{
					return EArcTacticsActionStatus::Failed;
				}
			}
		}
		if (!Context.HoldState)
		{
			return EArcTacticsActionStatus::Failed;
		}
		FArcTacticsMovementSettings EffectiveMovement = Data.Movement;
		if (Data.DestinationQuery.IsValid())
		{
			EffectiveMovement.ArrivalRange = Data.QueryApproachDistance;
		}
		const EArcTacticsActionStatus Status = Data.MovementRuntime.bOwnsMove
			? ArcTacticsMovement::UpdateApproach(
				*Context.PathContext, EffectiveMovement, Destination, Data.MovementRuntime, *Context.HoldState,
				Context.RequestToken.ActionSerial)
			: ArcTacticsMovement::StartApproach(
				*Context.PathContext, EffectiveMovement, Destination, Data.MovementRuntime, *Context.HoldState,
				Context.RequestToken.ActionSerial);
		if (Status != EArcTacticsActionStatus::Succeeded)
		{
			return Status;
		}
		return bClaimSlot && !bFollowSlot
			? EArcTacticsActionStatus::Running
			: StopMoveTo(Context, Data);
	}
}

const UScriptStruct* FArcTacticsMoveToStep::GetInstanceDataType() const
{
	return FArcTacticsMoveToStepInstanceData::StaticStruct();
}

FText FArcTacticsMoveToStep::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "MoveToStep", "Move To");
}

bool FArcTacticsMoveToStep::Enter(FArcTacticsStepContext&, FStructView InstanceData) const
{
	FArcTacticsMoveToStepInstanceData* Data = InstanceData.GetPtr<FArcTacticsMoveToStepInstanceData>();
	if (!Data)
	{
		return false;
	}
	ArcTacticsMoveToStepPrivate::ResetMoveToScratch(*Data);
	return true;
}

EArcTacticsActionStatus FArcTacticsMoveToStep::Tick(FArcTacticsStepContext& StepContext, FStructView InstanceData) const
{
	FArcTacticsMoveToStepInstanceData* Data = InstanceData.GetPtr<FArcTacticsMoveToStepInstanceData>();
	FArcTacticsActionContext& Context = StepContext.Action;
	if (!Data)
	{
		return EArcTacticsActionStatus::Failed;
	}
	const EArcTacticsActionStatus Status = ArcTacticsMoveToStepPrivate::TickMoveTo(StepContext, *Data);
	if (Status == EArcTacticsActionStatus::Failed
		&& (Data->bFollowingClaim || Data->bClaimPending)
		&& Context.EntityManager)
	{
		ArcCombatSlots::PushRelease(*Context.EntityManager, Data->Target, Context.Entity);
		Data->bFollowingClaim = false;
		Data->bClaimPending = false;
	}
	return Status;
}

void FArcTacticsMoveToStep::Cancel(
	FArcTacticsStepContext& StepContext,
	FStructView InstanceData,
	const EArcTacticsActionCancelMode CancelMode) const
{
	FArcTacticsMoveToStepInstanceData* Data = InstanceData.GetPtr<FArcTacticsMoveToStepInstanceData>();
	FArcTacticsActionContext& Context = StepContext.Action;
	if (!Data)
	{
		return;
	}
	ArcTacticsMovement::AbortDestinationQuery(Context.QuerySubsystem, Data->MovementRuntime);
	if (Context.PathContext)
	{
		if (CancelMode == EArcTacticsActionCancelMode::ReleaseHold && Context.HoldState)
		{
			ArcTacticsMovement::Cancel(*Context.PathContext, Data->MovementRuntime, *Context.HoldState);
		}
		else
		{
			ArcTacticsMovement::CancelAction(*Context.PathContext, Data->MovementRuntime);
		}
	}
}

void FArcTacticsMoveToStep::OnAsyncQueryCompleted(
	FArcTacticsStepContext& StepContext,
	FStructView InstanceData,
	const FArcTacticsAsyncRequestToken& Token,
	FArcTQSQueryInstance& CompletedQuery) const
{
	FArcTacticsMoveToStepInstanceData* Data = InstanceData.GetPtr<FArcTacticsMoveToStepInstanceData>();
	if (!Data || !StepContext.IsCurrentRequest(Token)
		|| CompletedQuery.QueryId != Data->MovementRuntime.DestinationQueryID)
	{
		return;
	}
	Data->MovementRuntime.DestinationQueryID = INDEX_NONE;
	Data->bDestinationQueryCompleted = true;
	Data->bDestinationQuerySucceeded = CompletedQuery.Status == EArcTQSQueryStatus::Completed
		&& !CompletedQuery.Results.IsEmpty();
	if (Data->bDestinationQuerySucceeded)
	{
		const FVector Destination = CompletedQuery.Results[0].GetLocation(StepContext.Action.EntityManager);
		Data->bDestinationQuerySucceeded = !Destination.ContainsNaN();
		if (Data->bDestinationQuerySucceeded)
		{
			Data->MovementRuntime.ResolvedDestination = Destination;
			Data->MovementRuntime.bHasResolvedDestination = true;
		}
	}
	FArcTacticsQueryReport& Report = StepContext.Action.QueryReport;
	Report.bAccepted = true;
	Report.Definition = Data->DestinationQuery.GetDefinition();
	Report.AppliedCount = Data->bDestinationQuerySucceeded ? 1 : 0;
	StepContext.Action.RequestExecution.ExecuteIfBound();
}

EArcTacticsActionTickLane FArcTacticsMoveToStep::GetTickLane(FConstStructView) const
{
	return EArcTacticsActionTickLane::Movement;
}
