// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_RemoveCue.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcCues/ArcCueFunctions.h"
#include "ArcCues/ArcCuesModule.h"
#include "Engine/World.h"

EStateTreeRunStatus FArcAbilityTask_RemoveCue::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	UWorld* World = AbilityContext.GetMassContext().GetWorld();
	UE_LOGF(LogArcCues, Log, "[CueLifecycle] RemoveCue task entered: Handle=%u World=%ls AbilityEntity=%ls",
		InstanceData.Handle.GetId(), *GetNameSafe(World), *AbilityContext.GetEntity().DebugGetDescription());

	if (World == nullptr || !InstanceData.Handle.IsValid())
	{
		UE_LOGF(LogArcCues, Warning, "[CueLifecycle] RemoveCue task rejected: Handle=%u World=%ls", InstanceData.Handle.GetId(), *GetNameSafe(World));
		return EStateTreeRunStatus::Failed;
	}

	ArcCues::RemoveCue(World, InstanceData.Handle);
	return EStateTreeRunStatus::Succeeded;
}
