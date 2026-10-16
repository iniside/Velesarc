// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcBodySlotSelection.h"

#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Sim/ArcBodyCommandLogic.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"

namespace ArcWorldSim
{
bool FindAvailableSlot(
	FMassEntityManager& EntityManager,
	USmartObjectSubsystem& SmartObjectSubsystem,
	const FMassEntityHandle TargetEntity,
	const FGameplayTag& SlotTag,
	FSmartObjectRequestResult& OutRequestResult)
{
	OutRequestResult = FSmartObjectRequestResult(FSmartObjectHandle());
	if (!EntityManager.IsEntityValid(TargetEntity) || !SlotTag.IsValid())
	{
		return false;
	}

	const FMassEntityView TargetView(EntityManager, TargetEntity);
	const FArcSmartObjectOwnerFragment* Owner = TargetView.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>();
	if (!Owner || !Owner->SmartObjectHandle.IsValid())
	{
		return false;
	}

	FSmartObjectRequestFilter Filter;
	Filter.ActivityRequirements = FGameplayTagQuery::MakeQuery_MatchTag(SlotTag);
	TArray<FSmartObjectSlotHandle> Slots;
	SmartObjectSubsystem.FindSlots(Owner->SmartObjectHandle, Filter, Slots);
	for (const FSmartObjectSlotHandle Slot : Slots)
	{
		if (SmartObjectSubsystem.CanBeClaimed(Slot, ESmartObjectClaimPriority::Normal))
		{
			OutRequestResult = FSmartObjectRequestResult(Owner->SmartObjectHandle, Slot);
			return true;
		}
	}
	return false;
}

bool TryAssignSlot(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	USmartObjectSubsystem& SmartObjectSubsystem,
	UMassSignalSubsystem& SignalSubsystem,
	const FMassEntityHandle UserEntity,
	FArcBodyFragment& Body,
	FMassSmartObjectUserFragment& User,
	const FMassEntityHandle TargetEntity,
	const FSmartObjectRequestResult& RequestResult)
{
	return ArcBodyCommand::TryAssignClaimedSlot(
		EntityManager,
		Context,
		SmartObjectSubsystem,
		SignalSubsystem,
		UserEntity,
		Body,
		User,
		TargetEntity,
		RequestResult);
}

bool TryAssignSlot(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	USmartObjectSubsystem& SmartObjectSubsystem,
	UMassSignalSubsystem& SignalSubsystem,
	const FMassEntityHandle UserEntity,
	FArcBodyFragment& Body,
	FMassSmartObjectUserFragment& User,
	const FMassEntityHandle TargetEntity,
	const FGameplayTag& SlotTag)
{
	FSmartObjectRequestResult RequestResult{FSmartObjectHandle()};
	if (!FindAvailableSlot(EntityManager, SmartObjectSubsystem, TargetEntity, SlotTag, RequestResult))
	{
		return false;
	}

	return TryAssignSlot(
		EntityManager,
		Context,
		SmartObjectSubsystem,
		SignalSubsystem,
		UserEntity,
		Body,
		User,
		TargetEntity,
		RequestResult);
}
}
