// Copyright Lukasz Baran. All Rights Reserved.

#include "GlobalTasks/ArcTacticsGlobalTask.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsGlobalTask)

EArcTacticsNodeRole FArcTacticsGlobalTask::GetRole() const
{
	return EArcTacticsNodeRole::GlobalTask;
}

EArcTacticsTaskStatus FArcTacticsGlobalTask::Enter(FArcTacticsTaskContext&, FStructView) const
{
	return EArcTacticsTaskStatus::Succeeded;
}

EArcTacticsTaskStatus FArcTacticsGlobalTask::Tick(FArcTacticsTaskContext&, FStructView) const
{
	return EArcTacticsTaskStatus::Succeeded;
}

void FArcTacticsGlobalTask::Cancel(FArcTacticsTaskContext&, FStructView) const
{
}

void FArcTacticsGlobalTask::OnAsyncQueryCompleted(
	FArcTacticsTaskContext&,
	FStructView,
	const FArcTacticsAsyncRequestToken&,
	FArcTQSQueryInstance&) const
{
}

EArcTacticsExecutionRequirement FArcTacticsGlobalTask::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::None;
}
