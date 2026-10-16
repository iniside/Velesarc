// Copyright Lukasz Baran. All Rights Reserved.

#include "Actions/ArcTacticsWaitAction.h"

#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsWaitAction)

const UScriptStruct* FArcTacticsWaitAction::GetInstanceDataType() const
{
	return FArcTacticsWaitActionInstanceData::StaticStruct();
}

FText FArcTacticsWaitAction::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "WaitAction", "Wait");
}

EArcTacticsActionStatus FArcTacticsWaitAction::Enter(
	FArcTacticsActionContext& Context,
	FStructView InstanceData) const
{
	FArcTacticsWaitActionInstanceData* Data = InstanceData.GetPtr<FArcTacticsWaitActionInstanceData>();
	if (!Data || !Context.World)
	{
		return EArcTacticsActionStatus::Failed;
	}
	Data->EndTime = Context.World->GetTimeSeconds() + FMath::Max(0.0f, Data->Duration);
	return Data->Duration <= 0.0f ? EArcTacticsActionStatus::Succeeded : EArcTacticsActionStatus::Running;
}

EArcTacticsActionStatus FArcTacticsWaitAction::Tick(
	FArcTacticsActionContext& Context,
	FStructView InstanceData) const
{
	const FArcTacticsWaitActionInstanceData* Data = InstanceData.GetPtr<FArcTacticsWaitActionInstanceData>();
	if (!Data || !Context.World)
	{
		return EArcTacticsActionStatus::Failed;
	}
	return Context.World->GetTimeSeconds() >= Data->EndTime
		? EArcTacticsActionStatus::Succeeded
		: EArcTacticsActionStatus::Running;
}

void FArcTacticsWaitAction::Cancel(
	FArcTacticsActionContext&,
	FStructView,
	const EArcTacticsActionCancelMode) const
{
}

EArcTacticsActionTickLane FArcTacticsWaitAction::GetTickLane(const FArcTacticsActionView&) const
{
	return EArcTacticsActionTickLane::Utility;
}

EArcTacticsExecutionRequirement FArcTacticsWaitAction::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::None;
}

bool FArcTacticsWaitAction::BlocksDecision(const FArcTacticsActionView&) const
{
	return false;
}

double FArcTacticsWaitAction::GetNextWakeTime(const FArcTacticsActionView& Action) const
{
	const FArcTacticsWaitActionInstanceData* Data = Action.ActionData.GetPtr<FArcTacticsWaitActionInstanceData>();
	return Data ? Data->EndTime : 0.0;
}

#if WITH_EDITOR
FName FArcTacticsWaitAction::GetIconName() const
{
	return TEXT("AppStyle|Icons.Toolbar.Pause");
}

FColor FArcTacticsWaitAction::GetIconColor() const
{
	return FColor(120, 170, 220);
}
#endif
