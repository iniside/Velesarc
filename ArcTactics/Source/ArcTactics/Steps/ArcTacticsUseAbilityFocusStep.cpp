// Copyright Lukasz Baran. All Rights Reserved.

#include "Steps/ArcTacticsUseAbilityFocusStep.h"

#include "Steps/ArcTacticsStepHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsUseAbilityFocusStep)

const UScriptStruct* FArcTacticsUseAbilityFocusStep::GetInstanceDataType() const
{
	return FArcTacticsUseAbilityFocusStepInstanceData::StaticStruct();
}

FText FArcTacticsUseAbilityFocusStep::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "UseAbilityFocusStep", "Use Ability Focus");
}

EArcTacticsActionStatus FArcTacticsUseAbilityFocusStep::Tick(
	FArcTacticsStepContext& StepContext,
	FStructView InstanceData) const
{
	FArcTacticsUseAbilityFocusStepInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsUseAbilityFocusStepInstanceData>();
	FArcTacticsActionContext& Context = StepContext.Action;
	if (!Data)
	{
		return EArcTacticsActionStatus::Failed;
	}
	FVector TargetLocation = FVector::ZeroVector;
	if (!ArcTacticsStepHelpers::ResolveTargetLocation(
		Context, Data->Target, Data->TargetLocation, Data->bUseTargetLocation, TargetLocation))
	{
		return EArcTacticsActionStatus::Failed;
	}
	if (Data->Stage == StageFacing)
	{
		if (!Context.PathContext)
		{
			return EArcTacticsActionStatus::Running;
		}
		if (!Context.HoldState)
		{
			return EArcTacticsActionStatus::Failed;
		}
		const EArcTacticsActionStatus Status = Data->MovementRuntime.bFacingStarted
			? ArcTacticsMovement::UpdateFacing(
				*Context.PathContext, Data->Facing, TargetLocation, Data->MovementRuntime)
			: ArcTacticsMovement::StartFacing(
				*Context.PathContext, Data->Facing, TargetLocation, Data->MovementRuntime, *Context.HoldState);
		if (Status != EArcTacticsActionStatus::Succeeded)
		{
			return Status;
		}
		Data->Stage = StageAcquireHold;
	}
	return ArcTacticsStepHelpers::FinishAbilityStep(
		Context,
		*Data,
		ArcTacticsStepHelpers::TickAbility(Context, *Data, StageAcquireHold, StageWaitingForStart, StageUsing));
}

EArcTacticsActionTickLane FArcTacticsUseAbilityFocusStep::GetTickLane(const FConstStructView InstanceData) const
{
	const FArcTacticsUseAbilityFocusStepInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsUseAbilityFocusStepInstanceData>();
	if (!Data)
	{
		return EArcTacticsActionTickLane::None;
	}
	return Data->Stage <= StageAcquireHold
		? EArcTacticsActionTickLane::Movement
		: EArcTacticsActionTickLane::Utility;
}

bool FArcTacticsUseAbilityFocusStep::BlocksDecision(const FConstStructView InstanceData) const
{
	const FArcTacticsUseAbilityFocusStepInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsUseAbilityFocusStepInstanceData>();
	return Data && Data->Stage != StageFacing;
}
