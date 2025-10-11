#include "ArcMassMakePlanMultipleGoalsTask.h"

#include "SmartObjectPlanner/ArcSmartObjectPlannerSubsystem.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanRequest.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"

FArcMassMakePlanMultipleGoalsTask::FArcMassMakePlanMultipleGoalsTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassMakePlanMultipleGoalsTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	TArray<FArcSmartObjectPlanRequestHandle> Handles;
	for (const FGameplayTagContainer& GoalTags : InstanceData.GoalRequiredTags)
	{
		FArcSmartObjectPlanRequest Request;
		Request.InitialTags = InstanceData.InitialTags;
		Request.Requires = GoalTags;
		Request.SearchOrigin = InstanceData.SearchOrigin;
		Request.SearchRadius = InstanceData.SearchRadius;

		UArcSmartObjectPlannerSubsystem* PreconSubsystem = Context.GetWorld()->GetSubsystem<UArcSmartObjectPlannerSubsystem>();

		FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
		UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
		UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	
		Request.FinishedDelegate.BindWeakLambda(PreconSubsystem, [&Handles, WeakContext = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity = MassCtx.GetEntity()](const FArcSmartObjectPlanResponse& Response)
		{
			const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
		
			FInstanceDataType* DataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();

			FArcSmartObjectPlanResponse* Plan = DataPtr->PlanResponse.GetInternalPropertyRef().GetPtrFromStrongExecutionContext<FArcSmartObjectPlanResponse>(StrongContext);
			*Plan = Response;
			Handles.Remove(Response.Handle);
			if (Handles.IsEmpty())
			{
				StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
			}
		});
	
		FArcSmartObjectPlanRequestHandle Handle = PreconSubsystem->AddRequest(Request);
		Handles.Add(Handle);
	}

	
	return EStateTreeRunStatus::Running;
}