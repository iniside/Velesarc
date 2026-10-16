// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcBodyCommandLogic.h"

#include <atomic>

#include "ArcNeedSignals.h"
#include "Mass/ExecutionContext.h"
#include "Mass/EntityManager.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectFragments.h"
#include "MassSmartObjectHandler.h"
#include "Sim/ArcSmartObjectBehaviorDefinition.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"

namespace ArcBodyCommandPrivate
{
uint32 NextCommandSerial()
{
	static std::atomic<uint32> Counter{0};
	uint32 Serial = ++Counter;
	if (Serial == 0)
	{
		Serial = ++Counter;
	}
	return Serial;
}

void ReleaseUnadoptedClaim(
	USmartObjectSubsystem& SmartObjectSubsystem,
	const FSmartObjectClaimHandle& ClaimHandle)
{
	SmartObjectSubsystem.UnregisterSlotInvalidationCallback(ClaimHandle);
	SmartObjectSubsystem.MarkSlotAsFree(ClaimHandle);
}

bool BuildCommand(
	FMassEntityManager& EntityManager,
	USmartObjectSubsystem& SmartObjectSubsystem,
	const FMassEntityHandle TargetEntity,
	const FSmartObjectRequestResult& RequestResult,
	const FSmartObjectClaimHandle& ClaimHandle,
	FArcBodyCommand& OutCommand)
{
	if (!RequestResult.IsValid() || !ClaimHandle.IsValid()
		|| RequestResult.SlotHandle != ClaimHandle.SlotHandle
		|| !SmartObjectSubsystem.IsClaimedSmartObjectValid(ClaimHandle)
		|| !EntityManager.IsEntityValid(TargetEntity))
	{
		return false;
	}

	const UArcSmartObjectMassBehaviorDefinition* BehaviorDefinition =
		SmartObjectSubsystem.GetBehaviorDefinition<UArcSmartObjectMassBehaviorDefinition>(ClaimHandle);
	const TOptional<FTransform> SlotTransform = SmartObjectSubsystem.GetSlotTransform(ClaimHandle);
	if (!BehaviorDefinition || !SlotTransform.IsSet())
	{
		return false;
	}

	OutCommand.TargetSlot = RequestResult.SlotHandle;
	OutCommand.ClaimHandle = ClaimHandle;
	OutCommand.TargetEntity = TargetEntity;
	OutCommand.TargetLocation = SlotTransform->GetLocation();
	OutCommand.Serial = NextCommandSerial();
	OutCommand.bInterruptible = BehaviorDefinition->IsInterruptible();
	return true;
}
}

bool ArcBodyCommand::TryAssignClaimedSlot(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	USmartObjectSubsystem& SmartObjectSubsystem,
	UMassSignalSubsystem& SignalSubsystem,
	const FMassEntityHandle Entity,
	FArcBodyFragment& Body,
	FMassSmartObjectUserFragment& User,
	const FMassEntityHandle TargetEntity,
	const FSmartObjectRequestResult& RequestResult)
{
	check(IsInGameThread());
	if (Body.State != EArcBodyState::Idle || Body.Command.IsActive() || !RequestResult.IsValid()
		|| !EntityManager.IsEntityValid(TargetEntity))
	{
		return false;
	}
	if (User.InteractionHandle.IsValid()
		|| User.InteractionStatus == EMassSmartObjectInteractionStatus::InProgress
		|| User.InteractionStatus == EMassSmartObjectInteractionStatus::BehaviorCompleted)
	{
		return false;
	}
	User.InteractionStatus = EMassSmartObjectInteractionStatus::Unset;
	User.InteractionHandle.Invalidate();

	const FMassSmartObjectHandler Handler(Context, SmartObjectSubsystem, SignalSubsystem);
	const FSmartObjectClaimHandle ClaimHandle = Handler.ClaimSmartObject(Entity, User, RequestResult);
	if (!ClaimHandle.IsValid())
	{
		return false;
	}

	return TryAdoptClaimedSlot(
		EntityManager,
		Context,
		SmartObjectSubsystem,
		SignalSubsystem,
		Entity,
		Body,
		User,
		TargetEntity,
		RequestResult,
		ClaimHandle,
		0);
}

bool ArcBodyCommand::TryAdoptClaimedSlot(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	USmartObjectSubsystem& SmartObjectSubsystem,
	UMassSignalSubsystem& SignalSubsystem,
	const FMassEntityHandle Entity,
	FArcBodyFragment& Body,
	FMassSmartObjectUserFragment& User,
	const FMassEntityHandle TargetEntity,
	const FSmartObjectRequestResult& RequestResult,
	const FSmartObjectClaimHandle& ClaimHandle,
	const uint32 ReplacedCommandSerial)
{
	check(IsInGameThread());
	const bool bReplacingCommand = ReplacedCommandSerial != 0;
	if (bReplacingCommand)
	{
		if (!Body.Command.IsActive() || Body.Command.Serial != ReplacedCommandSerial
			|| !Body.Command.bInterruptible)
		{
			ArcBodyCommandPrivate::ReleaseUnadoptedClaim(SmartObjectSubsystem, ClaimHandle);
			return false;
		}
	}
	else if (Body.State != EArcBodyState::Idle || Body.Command.IsActive()
		|| User.InteractionHandle.IsValid()
		|| User.InteractionStatus == EMassSmartObjectInteractionStatus::InProgress
		|| User.InteractionStatus == EMassSmartObjectInteractionStatus::BehaviorCompleted)
	{
		ArcBodyCommandPrivate::ReleaseUnadoptedClaim(SmartObjectSubsystem, ClaimHandle);
		return false;
	}

	FArcBodyCommand Command;
	if (!ArcBodyCommandPrivate::BuildCommand(
		EntityManager, SmartObjectSubsystem, TargetEntity, RequestResult, ClaimHandle, Command))
	{
		ArcBodyCommandPrivate::ReleaseUnadoptedClaim(SmartObjectSubsystem, ClaimHandle);
		return false;
	}

	if (bReplacingCommand && !FinalizeCommand(
		Context,
		SmartObjectSubsystem,
		SignalSubsystem,
		Entity,
		Body,
		User,
		ReplacedCommandSerial,
		EArcBodyCommandResult::Cancelled))
	{
		ArcBodyCommandPrivate::ReleaseUnadoptedClaim(SmartObjectSubsystem, ClaimHandle);
		return false;
	}

	User.InteractionStatus = EMassSmartObjectInteractionStatus::Unset;
	User.InteractionHandle.Invalidate();
	Body.Command = Command;
	Body.State = EArcBodyState::MoveTo;
	return true;
}

bool ArcBodyCommand::SetBehaviorResult(
	FArcBodyFragment& Body,
	const uint32 CommandSerial,
	const EArcBodyCommandResult Result)
{
	if (!Body.Command.IsActive() || Body.Command.Serial != CommandSerial || Result == EArcBodyCommandResult::None)
	{
		return false;
	}

	Body.Command.BehaviorResult = Result;
	return true;
}

bool ArcBodyCommand::FinalizeCommand(
	FMassExecutionContext& Context,
	USmartObjectSubsystem& SmartObjectSubsystem,
	UMassSignalSubsystem& SignalSubsystem,
	const FMassEntityHandle Entity,
	FArcBodyFragment& Body,
	FMassSmartObjectUserFragment& User,
	const uint32 CommandSerial,
	const EArcBodyCommandResult Result)
{
	check(IsInGameThread());
	if (!Body.Command.IsActive() || Body.Command.Serial != CommandSerial)
	{
		return false;
	}

	const FSmartObjectClaimHandle ClaimHandle = Body.Command.ClaimHandle;
	const bool bStartedUsing = Body.Command.bStartedUsing;
	Body.LastCommandSerial = CommandSerial;
	Body.LastCommandResult = Result;
	Body.Command.Reset();
	Body.State = EArcBodyState::Idle;

	const FMassSmartObjectHandler Handler(Context, SmartObjectSubsystem, SignalSubsystem);
	if (bStartedUsing)
	{
		const EMassSmartObjectInteractionStatus StopStatus = Result == EArcBodyCommandResult::Succeeded
			? EMassSmartObjectInteractionStatus::TaskCompleted
			: EMassSmartObjectInteractionStatus::Aborted;
		Handler.StopUsingSmartObject(Entity, User, StopStatus);
	}
	Handler.ReleaseSmartObject(Entity, User, ClaimHandle);
	SignalSubsystem.SignalEntity(ArcNeeds::Signals::UtilityEvaluate, Entity);
	return true;
}
