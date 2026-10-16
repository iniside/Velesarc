#include "Fixtures/ArcTacticsPoolTestTypes.h"

#include "Mass/ExecutionContext.h"
#include "StructUtils/StructView.h"

void FArcTacticsPoolCallbackTask::OnAsyncQueryCompleted(
	FArcTacticsTaskContext&,
	FStructView InstanceData,
	const FArcTacticsAsyncRequestToken&,
	FArcTQSQueryInstance&) const
{
	FArcTacticsPoolCallbackTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsPoolCallbackTaskInstanceData>();
	if (Data)
	{
		++Data->CallbackCount;
	}
}

void FArcTacticsPoolCallbackTask::Cancel(FArcTacticsTaskContext&, FStructView InstanceData) const
{
	FArcTacticsPoolCallbackTaskInstanceData* Data = InstanceData.GetPtr<FArcTacticsPoolCallbackTaskInstanceData>();
	if (Data && Data->CancelTarget)
	{
		++Data->CancelTarget->CancelCount;
	}
}

void FArcTacticsPoolCallbackAction::OnAsyncQueryCompleted(
	FArcTacticsActionContext&,
	FStructView InstanceData,
	const FArcTacticsAsyncRequestToken&,
	FArcTQSQueryInstance&) const
{
	FArcTacticsPoolCallbackActionInstanceData* Data = InstanceData.GetPtr<FArcTacticsPoolCallbackActionInstanceData>();
	if (Data)
	{
		++Data->CallbackCount;
	}
}

void FArcTacticsPoolCallbackAction::Cancel(
	FArcTacticsActionContext&,
	FStructView InstanceData,
	EArcTacticsActionCancelMode) const
{
	FArcTacticsPoolCallbackActionInstanceData* Data = InstanceData.GetPtr<FArcTacticsPoolCallbackActionInstanceData>();
	if (Data && Data->CancelTarget)
	{
		++Data->CancelTarget->CancelCount;
	}
}

EArcTacticsActionStatus FArcTacticsPoolLifecycleAction::Enter(
	FArcTacticsActionContext&,
	FStructView InstanceData) const
{
	FArcTacticsPoolLifecycleActionInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsPoolLifecycleActionInstanceData>();
	if (!Data)
	{
		return EArcTacticsActionStatus::Failed;
	}
	++Data->EnterCount;
	Data->Output = 100 + Data->Mode;
	if (Data->Mode == 3)
	{
		return EArcTacticsActionStatus::Failed;
	}
	if (Data->Mode != 0)
	{
		return EArcTacticsActionStatus::Running;
	}
	return EArcTacticsActionStatus::Succeeded;
}

EArcTacticsActionStatus FArcTacticsPoolLifecycleAction::Tick(
	FArcTacticsActionContext&,
	FStructView InstanceData) const
{
	FArcTacticsPoolLifecycleActionInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsPoolLifecycleActionInstanceData>();
	if (!Data)
	{
		return EArcTacticsActionStatus::Failed;
	}
	++Data->TickCount;
	Data->Output = 200 + Data->Mode;
	if (Data->Mode == 2)
	{
		return EArcTacticsActionStatus::Running;
	}
	if (Data->Mode == 4)
	{
		return EArcTacticsActionStatus::Failed;
	}
	return EArcTacticsActionStatus::Succeeded;
}

void FArcTacticsPoolLifecycleAction::Cancel(
	FArcTacticsActionContext&,
	FStructView InstanceData,
	EArcTacticsActionCancelMode) const
{
	FArcTacticsPoolLifecycleActionInstanceData* Data =
		InstanceData.GetPtr<FArcTacticsPoolLifecycleActionInstanceData>();
	if (Data)
	{
		Data->Output = 900 + Data->Mode;
		if (Data->CancelTarget)
		{
			++Data->CancelTarget->CancelCount;
		}
	}
}

void UArcTacticsPoolDrainProcessor::Execute(FMassEntityManager&, FMassExecutionContext&)
{
}
