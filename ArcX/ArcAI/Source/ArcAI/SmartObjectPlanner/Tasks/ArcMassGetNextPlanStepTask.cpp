#include "ArcMassGetNextPlanStepTask.h"

#include "Engine/World.h"
#include "ArcAILogs.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "InstancedActors/ArcCoreInstancedActorsSubsystem.h"

FArcMassGetNextPlanStepTask::FArcMassGetNextPlanStepTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = true;
}

EStateTreeRunStatus FArcMassGetNextPlanStepTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();

	UMassEntitySubsystem* MassEntitySubsystem = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	
	TArray<FArcSmartObjectPlanStep>* PlanSteps = InstanceData.PlanItemsQueue.GetMutablePtr(Context);
	FString DebugString = "Selected Next Step :\n";
	if (PlanSteps->IsEmpty())
	{
		DebugString = " No plans available";
		UE_VLOG(MassEntitySubsystem, LogGoalPlannerStep, Log, TEXT("%s"), *DebugString);
		//SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
		return EStateTreeRunStatus::Failed;
	}
	
	int32* Idx = InstanceData.CurrentStepIdx.GetMutablePtr(Context);
	if (*Idx == INDEX_NONE)
	{
		*Idx = 0;
	}
	
	if (PlanSteps->IsEmpty())
	{
		DebugString = " No steps in plan available";
		UE_VLOG(MassEntitySubsystem, LogGoalPlannerStep, Log, TEXT("%s"), *DebugString);
		//SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
		return EStateTreeRunStatus::Failed;
	}

	if (*Idx >= PlanSteps->Num())
	{
		DebugString = " Plan Finished";
		UE_VLOG(MassEntitySubsystem, LogGoalPlannerStep, Log, TEXT("%s"), *DebugString);
		*Idx = INDEX_NONE;
		Context.BroadcastDelegate(InstanceData.OnPlanFinished);
		return EStateTreeRunStatus::Succeeded;
	}

	const FArcSmartObjectPlanStep& Item = (*PlanSteps)[(*Idx)];

	FArcMassEntityHandleWrapper* SmartObjectEntityHandle = InstanceData.SmartObjectEntityHandle.GetMutablePtr(Context);
	if (SmartObjectEntityHandle)
	{
		SmartObjectEntityHandle->EntityHandle = Item.EntityHandle;
	}
	
	(*Idx)++;
	FArcSmartObjectOwnerFragment* SOOwner = MassCtx.GetEntityManager().GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Item.EntityHandle);
	if (SOOwner)
	{
		FMassSmartObjectCandidateSlots* CandidateSlots = InstanceData.CandidateSlots.GetMutablePtr(Context);
		
		if (CandidateSlots)
		{
			*CandidateSlots = Item.FoundCandidateSlots;
		}
	}
	
	DebugString = FString::Printf(TEXT(" Moving to step %d / %d : Entity %s at %s"), *Idx, PlanSteps->Num(), *Item.EntityHandle.DebugGetDescription(), *Item.Location.ToString());
	UE_VLOG(MassEntitySubsystem, LogGoalPlannerStep, Log, TEXT("%s"), *DebugString);
	UE_VLOG_SPHERE(MassEntitySubsystem, LogGoalPlannerStep, Log, Item.Location, 25.0f, FColor::Green, TEXT("%s"), *DebugString);
	
	//SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
	return EStateTreeRunStatus::Succeeded;
}
