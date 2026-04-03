// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcGI_TransferLootContainerTask.h"

#include "Items/Loot/ArcLootFragments.h"
#include "Mass/ArcMassItemFragments.h"
#include "MassEntitySubsystem.h"
#include "StateTreeExecutionContext.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcGITransferLoot, Log, All);

FArcGI_TransferLootContainerTask::FArcGI_TransferLootContainerTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcGI_TransferLootContainerTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FMassEntityHandle SourceEntity = InstanceData.SmartObjectEntity;
	if (!SourceEntity.IsValid())
	{
		UE_LOG(LogArcGITransferLoot, Warning, TEXT("EnterState: SmartObjectEntity (source) is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle DestEntity = InstanceData.ContextEntity;
	if (!DestEntity.IsValid())
	{
		UE_LOG(LogArcGITransferLoot, Warning, TEXT("EnterState: ContextEntity (destination) is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = Context.GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		UE_LOG(LogArcGITransferLoot, Warning, TEXT("EnterState: No UMassEntitySubsystem."));
		return EStateTreeRunStatus::Failed;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	FArcLootContainerFragment* SourceFragment = EntityManager.GetFragmentDataPtr<FArcLootContainerFragment>(SourceEntity);
	if (!SourceFragment)
	{
		UE_LOG(LogArcGITransferLoot, Warning, TEXT("EnterState: Source entity has no FArcLootContainerFragment."));
		return EStateTreeRunStatus::Failed;
	}

	FArcMassItemSpecArrayFragment* DestFragment = EntityManager.GetFragmentDataPtr<FArcMassItemSpecArrayFragment>(DestEntity);
	if (!DestFragment)
	{
		UE_LOG(LogArcGITransferLoot, Warning, TEXT("EnterState: Destination entity has no FArcMassItemSpecArrayFragment."));
		return EStateTreeRunStatus::Failed;
	}

	DestFragment->Items.Append(MoveTemp(SourceFragment->Items));
	SourceFragment->Items.Reset();

	return EStateTreeRunStatus::Succeeded;
}
