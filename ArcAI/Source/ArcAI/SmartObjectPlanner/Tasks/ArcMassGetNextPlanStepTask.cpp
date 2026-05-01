#include "ArcMassGetNextPlanStepTask.h"

#include "Engine/World.h"
#include "ArcAILogs.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "InstancedActors/ArcCoreInstancedActorsSubsystem.h"

FArcMassGetNextPlanStepTask::FArcMassGetNextPlanStepTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = true;
}

EStateTreeRunStatus FArcMassGetNextPlanStepTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	FExecutionRuntimeDataType& ExecutionRuntimeData = Context.GetExecutionRuntimeData(*this);
	
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();

	UMassEntitySubsystem* MassEntitySubsystem = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	
#if ENABLE_VISUAL_LOG
	FString DebugString = "Selected Next Step :\n";
#endif
	if (InstanceData.PlanItemsQueue.IsEmpty())
	{
#if ENABLE_VISUAL_LOG
		DebugString = " No plans available";
		UE_VLOG(MassEntitySubsystem, LogGoalPlannerStep, Log, TEXT("%s"), *DebugString);
#endif
		//SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
		return EStateTreeRunStatus::Failed;
	}
	
	if (ExecutionRuntimeData.CurrentStep == INDEX_NONE)
	{
		ExecutionRuntimeData.CurrentStep = 0;
	}
	
	if (InstanceData.PlanItemsQueue.IsEmpty())
	{
#if ENABLE_VISUAL_LOG
		DebugString = " No steps in plan available";
		UE_VLOG(MassEntitySubsystem, LogGoalPlannerStep, Log, TEXT("%s"), *DebugString);
#endif
		//SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
		return EStateTreeRunStatus::Failed;
	}

	if (ExecutionRuntimeData.CurrentStep >= InstanceData.PlanItemsQueue.Num())
	{
#if ENABLE_VISUAL_LOG
		DebugString = " Plan Finished";
		UE_VLOG(MassEntitySubsystem, LogGoalPlannerStep, Log, TEXT("%s"), *DebugString);
#endif
		ExecutionRuntimeData.CurrentStep = INDEX_NONE;
		Context.BroadcastDelegate(InstanceData.OnPlanFinished);
		return EStateTreeRunStatus::Succeeded;
	}

	const FArcSmartObjectPlanStep& Item = InstanceData.PlanItemsQueue[ExecutionRuntimeData.CurrentStep];

	InstanceData.SmartObjectEntityHandle.EntityHandle = Item.EntityHandle;
	InstanceData.StepLocation = Item.Location;
	InstanceData.KnowledgeHandle = Item.KnowledgeHandle;
	
	ExecutionRuntimeData.CurrentStep++;
	FArcSmartObjectOwnerFragment* SOOwner = MassCtx.GetEntityManager().GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Item.EntityHandle);
	if (SOOwner)
	{
		InstanceData.CandidateSlots = Item.FoundCandidateSlots;
	}
	
#if ENABLE_VISUAL_LOG
	DebugString = FString::Printf(TEXT(" Moving to step %d / %d : Entity %s at %s"), ExecutionRuntimeData.CurrentStep, InstanceData.PlanItemsQueue.Num(), *Item.EntityHandle.DebugGetDescription(), *Item.Location.ToString());
	UE_VLOG(MassEntitySubsystem, LogGoalPlannerStep, Log, TEXT("%s"), *DebugString);
	UE_VLOG_SPHERE(MassEntitySubsystem, LogGoalPlannerStep, Log, Item.Location, 25.0f, FColor::Green, TEXT("%s"), *DebugString);
#endif
	
	//SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
	return EStateTreeRunStatus::Succeeded;
}

#if WITH_EDITOR
FText FArcMassGetNextPlanStepTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcAI", "GetNextPlanStepDesc", "Get Next Plan Step");
}
#endif
