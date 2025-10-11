#include "ArcMassSelectSmartObjectPlanTask.h"

#include "ArcAILogs.h"
#include "MassEntitySubsystem.h"
#include "MassStateTreeExecutionContext.h"

FArcMassSelectSmartObjectPlanTask::FArcMassSelectSmartObjectPlanTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassSelectSmartObjectPlanTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	UMassEntitySubsystem* MassEntitySubsystem = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	
	FString DebugString = "Selected Next Step :\n";
	if (InstanceData.PlanInput.Plans.IsEmpty())
	{
		DebugString = " No plans available";
		UE_VLOG(MassEntitySubsystem, LogGoalPlannerStep, Log, TEXT("%s"), *DebugString);
		//SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
		return EStateTreeRunStatus::Failed;
		
	}
	
	TArray<FArcSmartObjectPlanStep>* PlanSteps = InstanceData.OutPlanSteps.GetMutablePtr(Context);
	if (!PlanSteps)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	PlanSteps->Empty();
	
	int32 RandPlan = FMath::RandRange(0, InstanceData.PlanInput.Plans.Num() - 1);
	*PlanSteps = InstanceData.PlanInput.Plans[RandPlan].Items;
	
	return EStateTreeRunStatus::Succeeded;
}
