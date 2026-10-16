// Copyright Lukasz Baran. All Rights Reserved.

#include "Steps/ArcTacticsUseAbilityStep.h"

#include "Abilities/ArcAbilityDefinition.h"
#include "Steps/ArcTacticsStepHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsUseAbilityStep)

const UScriptStruct* FArcTacticsUseAbilityStep::GetInstanceDataType() const
{
	return FArcTacticsUseAbilityStepInstanceData::StaticStruct();
}

FText FArcTacticsUseAbilityStep::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "UseAbilityStep", "Use Ability");
}

bool FArcTacticsUseAbilityStep::Enter(FArcTacticsStepContext&, FStructView InstanceData) const
{
	FArcTacticsUseAbilityStepInstanceData* Data = InstanceData.GetPtr<FArcTacticsUseAbilityStepInstanceData>();
	if (!Data)
	{
		return false;
	}
	ArcTacticsStepHelpers::ResetAbilityScratch(*Data);
	return true;
}

EArcTacticsActionStatus FArcTacticsUseAbilityStep::Tick(
	FArcTacticsStepContext& StepContext,
	FStructView InstanceData) const
{
	FArcTacticsUseAbilityStepInstanceData* Data = InstanceData.GetPtr<FArcTacticsUseAbilityStepInstanceData>();
	if (!Data)
	{
		return EArcTacticsActionStatus::Failed;
	}
	return ArcTacticsStepHelpers::FinishAbilityStep(
		StepContext.Action,
		*Data,
		ArcTacticsStepHelpers::TickAbility(
			StepContext.Action, *Data, StageAcquireHold, StageWaitingForStart, StageUsing));
}

void FArcTacticsUseAbilityStep::Cancel(
	FArcTacticsStepContext& StepContext,
	FStructView InstanceData,
	const EArcTacticsActionCancelMode CancelMode) const
{
	FArcTacticsUseAbilityStepInstanceData* Data = InstanceData.GetPtr<FArcTacticsUseAbilityStepInstanceData>();
	FArcTacticsActionContext& Context = StepContext.Action;
	if (!Data)
	{
		return;
	}
	Data->Stage = 0;
	ArcTacticsStepHelpers::CancelExactAbility(Context, *Data);
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

EArcTacticsActionTickLane FArcTacticsUseAbilityStep::GetTickLane(const FConstStructView InstanceData) const
{
	const FArcTacticsUseAbilityStepInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsUseAbilityStepInstanceData>();
	if (!Data)
	{
		return EArcTacticsActionTickLane::None;
	}
	return Data->Stage == StageAcquireHold
		? EArcTacticsActionTickLane::Movement
		: EArcTacticsActionTickLane::Utility;
}

bool FArcTacticsUseAbilityStep::BlocksDecision(FConstStructView) const
{
	return true;
}

double FArcTacticsUseAbilityStep::GetNextWakeTime(const FConstStructView InstanceData) const
{
	const FArcTacticsUseAbilityStepInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsUseAbilityStepInstanceData>();
	return Data && Data->bAbilityDeadlineSet ? Data->AbilityDeadline : 0.0;
}
