#include "ArcMassMakeSmartObjectPlanTask.h"

#include "ArcAILogs.h"
#include "SmartObjectPlanner/ArcSmartObjectPlannerSubsystem.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanRequest.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanResponse.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"

FArcMassMakeSmartObjectPlanTask::FArcMassMakeSmartObjectPlanTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassMakeSmartObjectPlanTask::EnterState(FStateTreeExecutionContext& Context
																		 , const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	FArcSmartObjectPlanRequest Request;
	Request.RequestingEntity = MassCtx.GetEntity();
	Request.InitialTags = InstanceData.InitialTags;
	Request.Requires = InstanceData.RequiredTags;
	Request.SearchOrigin = InstanceData.SearchOrigin;
	Request.SearchRadius = InstanceData.SearchRadius;
	
	Request.CustomConditionsArray.Reset(InstanceData.CustomConditions.Num());
	for (const TInstancedStruct<FArcSmartObjectPlanConditionEvaluator>& Eval : InstanceData.CustomConditions)
	{
		Request.CustomConditionsArray.Add(FConstStructView(Eval.GetScriptStruct(), Eval.GetMemory()));	
	}
	
	UArcSmartObjectPlannerSubsystem* PreconSubsystem = Context.GetWorld()->GetSubsystem<UArcSmartObjectPlannerSubsystem>();

	
	UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	
	Request.FinishedDelegate.BindWeakLambda(PreconSubsystem, [WeakContext = Context.MakeWeakExecutionContext(), SignalSubsystem, MassSubsystem, Entity = MassCtx.GetEntity()](const FArcSmartObjectPlanResponse& Response)
	{
		const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
		
		FInstanceDataType* DataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();

		FArcSmartObjectPlanResponse* Plan = DataPtr->PlanResponse.GetInternalPropertyRef().GetPtrFromStrongExecutionContext<FArcSmartObjectPlanResponse>(StrongContext);
		*Plan = Response;
			
		FString PlanDebugString;
		for (const FArcSmartObjectPlanContainer& PlanRef : Plan->Plans)
		{
			PlanDebugString += FString::Printf(TEXT("Plan with %d steps:\n"), PlanRef.Items.Num());
			for (const FArcSmartObjectPlanStep& Item : PlanRef.Items)
			{
				PlanDebugString += FString::Printf(TEXT(" - Entity %s at %s\n"), *Item.EntityHandle.DebugGetDescription(), *Item.Location.ToString());
			}
		}
		UE_VLOG(MassSubsystem, LogGoalPlanner, Log, TEXT("%s"), *PlanDebugString);
			
		StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);

		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
		
	});
	
	PreconSubsystem->Requests.Push(Request);
	
	return EStateTreeRunStatus::Running;
}