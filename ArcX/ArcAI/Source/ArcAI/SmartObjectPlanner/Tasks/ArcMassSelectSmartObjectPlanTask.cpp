#include "ArcMassSelectSmartObjectPlanTask.h"

#include "ArcAILogs.h"
#include "MassEntitySubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "SmartObjectPlanner/ArcSmartObjectPlannerSubsystem.h"

#if ENABLE_VISUAL_LOG
#include "VisualLogger/VisualLogger.h"
#endif

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
	UArcSmartObjectPlannerSubsystem* PlannerSubsystem = Context.GetWorld()->GetSubsystem<UArcSmartObjectPlannerSubsystem>();
	
#if ENABLE_VISUAL_LOG	
	FString DebugString = "Selected Next Step :\n";
#endif
	if (InstanceData.PlanInput.Plans.IsEmpty())
	{
		
#if ENABLE_VISUAL_LOG
		DebugString = " No plans available";
		UE_VLOG(MassEntitySubsystem, LogGoalPlannerStep, Log, TEXT("%s"), *DebugString);
#endif
		//SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
		return EStateTreeRunStatus::Failed;
		
	}
	
	InstanceData.OutPlanSteps.Empty();
	
	int32 RandPlan = FMath::RandRange(0, InstanceData.PlanInput.Plans.Num() - 1);
	InstanceData.OutPlanSteps = InstanceData.PlanInput.Plans[RandPlan].Items;
	
	PlannerSubsystem->SetDebugPlan(MassCtx.GetEntity(), InstanceData.PlanInput.Plans[RandPlan]);
	
	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FArcMassSelectSmartObjectPlanTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcAI", "SelectSOPlanDesc", "Select Smart Object Plan");
}
#endif
