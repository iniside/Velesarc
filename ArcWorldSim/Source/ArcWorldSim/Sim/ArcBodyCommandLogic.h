// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "Sim/ArcBodyFragments.h"

struct FMassExecutionContext;
struct FMassEntityManager;
struct FMassSmartObjectUserFragment;
struct FSmartObjectRequestResult;
class UMassSignalSubsystem;
class USmartObjectSubsystem;

namespace ArcBodyCommand
{
	ARCWORLDSIM_API bool TryAssignClaimedSlot(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context,
		USmartObjectSubsystem& SmartObjectSubsystem,
		UMassSignalSubsystem& SignalSubsystem,
		FMassEntityHandle Entity,
		FArcBodyFragment& Body,
		FMassSmartObjectUserFragment& User,
		FMassEntityHandle TargetEntity,
		const FSmartObjectRequestResult& RequestResult);

	ARCWORLDSIM_API bool TryAdoptClaimedSlot(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context,
		USmartObjectSubsystem& SmartObjectSubsystem,
		UMassSignalSubsystem& SignalSubsystem,
		FMassEntityHandle Entity,
		FArcBodyFragment& Body,
		FMassSmartObjectUserFragment& User,
		FMassEntityHandle TargetEntity,
		const FSmartObjectRequestResult& RequestResult,
		const FSmartObjectClaimHandle& ClaimHandle,
		uint32 ReplacedCommandSerial);

	ARCWORLDSIM_API bool SetBehaviorResult(
		FArcBodyFragment& Body,
		uint32 CommandSerial,
		EArcBodyCommandResult Result);

	ARCWORLDSIM_API bool FinalizeCommand(
		FMassExecutionContext& Context,
		USmartObjectSubsystem& SmartObjectSubsystem,
		UMassSignalSubsystem& SignalSubsystem,
		FMassEntityHandle Entity,
		FArcBodyFragment& Body,
		FMassSmartObjectUserFragment& User,
		uint32 CommandSerial,
		EArcBodyCommandResult Result);
}
