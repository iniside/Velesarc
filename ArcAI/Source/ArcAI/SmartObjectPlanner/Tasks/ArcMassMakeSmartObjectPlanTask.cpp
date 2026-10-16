#include "ArcMassMakeSmartObjectPlanTask.h"

#include "ArcAILogs.h"
#include "SmartObjectPlanner/ArcSmartObjectPlannerSubsystem.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanRequest.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanResponse.h"
#include "SmartObjectPlanner/Sensors/ArcSmartObjectPlanSpatialHashSensor.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"

#if ENABLE_VISUAL_LOG
#include "VisualLogger/VisualLogger.h"
#endif

FArcMassMakeSmartObjectPlanTask::FArcMassMakeSmartObjectPlanTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassMakeSmartObjectPlanTask::EnterState(FStateTreeExecutionContext& Context
																		 , const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	const FMassEntityHandle Entity = MassCtx.GetEntity();
	const FMassEntityManager& EntityManager = MassCtx.GetEntityManager();

	FVector Origin = InstanceData.SearchOrigin;
	if (Origin.IsZero())
	{
		const FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
		if (TransformFragment)
		{
			Origin = TransformFragment->GetTransform().GetLocation();
		}
	}

	FArcSmartObjectPlanRequest Request;
	Request.RequestingEntity = Entity;
	Request.InitialTags = InstanceData.InitialTags;
	Request.Requires = InstanceData.RequiredTags;
	Request.SearchOrigin = Origin;
	Request.SearchRadius = InstanceData.SearchRadius;
	
	Request.CustomConditionsArray.Reset(InstanceData.CustomConditions.Num());
	for (const TInstancedStruct<FArcSmartObjectPlanConditionEvaluator>& Eval : InstanceData.CustomConditions)
	{
		Request.CustomConditionsArray.Add(FConstStructView(Eval.GetScriptStruct(), Eval.GetMemory()));
	}

	if (InstanceData.Sensors.IsEmpty())
	{
		Request.SensorsArray.Emplace_GetRef().InitializeAs<FArcSmartObjectPlanSpatialHashSensor>();
	}
	else
	{
		Request.SensorsArray = InstanceData.Sensors;
	}

	UArcSmartObjectPlannerSubsystem* PreconSubsystem = Context.GetWorld()->GetSubsystem<UArcSmartObjectPlannerSubsystem>();

	
	UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();

	Request.FinishedDelegate.BindWeakLambda(PreconSubsystem, [WeakContext = Context.MakeWeakExecutionContext(), SignalSubsystem, MassSubsystem, Entity](const FArcSmartObjectPlanResponse& Response)
	{
		const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
		
		FInstanceDataType* DataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
		if (!DataPtr)
		{
			return;
		}
		DataPtr->PlanResponse = Response;
			
#if ENABLE_VISUAL_LOG	
		FString PlanDebugString;
		for (const FArcSmartObjectPlanContainer& PlanRef : DataPtr->PlanResponse.Plans)
		{
			PlanDebugString += FString::Printf(TEXT("Plan with %d steps:\n"), PlanRef.Items.Num());
			for (const FArcSmartObjectPlanStep& Item : PlanRef.Items)
			{
				PlanDebugString += FString::Printf(TEXT(" - Entity %s at %s\n"), *Item.EntityHandle.DebugGetDescription(), *Item.Location.ToString());
			}
		}
		UE_VLOG(MassSubsystem, LogGoalPlanner, Log, TEXT("%s"), *PlanDebugString);
#endif
			
		StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);

		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
		
	});
	
	PreconSubsystem->AddRequest(Request);

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FArcMassMakeSmartObjectPlanTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "MakeSOPlanDesc", "Make SO Plan: {0} (R={1})"), FText::FromString(InstanceData->RequiredTags.ToStringSimple()), FText::AsNumber(InstanceData->SearchRadius));
		}
	}
	return FText::GetEmpty();
}
#endif