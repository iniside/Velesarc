// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcGI_DestroyEntityTask.h"

#include "MassCommandBuffer.h"
#include "MassEntitySubsystem.h"
#include "StateTreeExecutionContext.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcGIDestroyEntity, Log, All);

FArcGI_DestroyEntityTask::FArcGI_DestroyEntityTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcGI_DestroyEntityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FMassEntityHandle Entity = InstanceData.SmartObjectEntity;
	if (!Entity.IsValid())
	{
		UE_LOG(LogArcGIDestroyEntity, Warning, TEXT("EnterState: SmartObjectEntity is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = Context.GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		UE_LOG(LogArcGIDestroyEntity, Warning, TEXT("EnterState: No UMassEntitySubsystem."));
		return EStateTreeRunStatus::Failed;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	EntityManager.Defer().DestroyEntity(Entity);

	return EStateTreeRunStatus::Succeeded;
}
