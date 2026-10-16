// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"

struct FArcBodyFragment;
struct FMassEntityManager;
struct FMassExecutionContext;
struct FMassSmartObjectUserFragment;
struct FSmartObjectRequestResult;
class UMassSignalSubsystem;
class USmartObjectSubsystem;

namespace ArcWorldSim
{
	bool FindAvailableSlot(
		FMassEntityManager& EntityManager,
		USmartObjectSubsystem& SmartObjectSubsystem,
		FMassEntityHandle TargetEntity,
		const FGameplayTag& SlotTag,
		FSmartObjectRequestResult& OutRequestResult);

	bool TryAssignSlot(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context,
		USmartObjectSubsystem& SmartObjectSubsystem,
		UMassSignalSubsystem& SignalSubsystem,
		FMassEntityHandle UserEntity,
		FArcBodyFragment& Body,
		FMassSmartObjectUserFragment& User,
		FMassEntityHandle TargetEntity,
		const FSmartObjectRequestResult& RequestResult);

	bool TryAssignSlot(
		FMassEntityManager& EntityManager,
		FMassExecutionContext& Context,
		USmartObjectSubsystem& SmartObjectSubsystem,
		UMassSignalSubsystem& SignalSubsystem,
		FMassEntityHandle UserEntity,
		FArcBodyFragment& Body,
		FMassSmartObjectUserFragment& User,
		FMassEntityHandle TargetEntity,
		const FGameplayTag& SlotTag);
}
