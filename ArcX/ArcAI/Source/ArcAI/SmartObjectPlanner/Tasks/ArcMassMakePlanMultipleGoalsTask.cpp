// Copyright Lukasz Baran. All Rights Reserved.

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
	InstanceData.PendingHandles.Reset();

	UArcSmartObjectPlannerSubsystem* PlannerSubsystem = Context.GetWorld()->GetSubsystem<UArcSmartObjectPlannerSubsystem>();
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();

	for (const FGameplayTagContainer& GoalTags : InstanceData.GoalRequiredTags)
	{
		FArcSmartObjectPlanRequest Request;
		Request.InitialTags = InstanceData.InitialTags;
		Request.Requires = GoalTags;
		Request.SearchOrigin = InstanceData.SearchOrigin;
		Request.SearchRadius = InstanceData.SearchRadius;

		Request.FinishedDelegate.BindWeakLambda(PlannerSubsystem,
			[WeakContext = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity = MassCtx.GetEntity()]
			(const FArcSmartObjectPlanResponse& Response)
		{
			const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();

			FInstanceDataType* DataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (!DataPtr)
			{
				return;
			}

			FArcSmartObjectPlanResponse* Plan = DataPtr->PlanResponse.GetInternalPropertyRef()
				.GetPtrFromStrongExecutionContext<FArcSmartObjectPlanResponse>(StrongContext);

			// Accumulate plans from all goals instead of overwriting
			Plan->Plans.Append(Response.Plans);
			Plan->AccumulatedTags.AppendTags(Response.AccumulatedTags);

			// Track completion via instance data (not a dangling stack reference)
			DataPtr->PendingHandles.Remove(Response.Handle);

			if (DataPtr->PendingHandles.IsEmpty())
			{
				StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
			}
		});

		FArcSmartObjectPlanRequestHandle Handle = PlannerSubsystem->AddRequest(Request);
		InstanceData.PendingHandles.Add(Handle);
	}

	return EStateTreeRunStatus::Running;
}
