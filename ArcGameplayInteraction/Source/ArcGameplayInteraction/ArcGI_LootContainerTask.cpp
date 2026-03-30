// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGI_LootContainerTask.h"

#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Commands/ArcLootItemCommand.h"

#include "StateTreeExecutionContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcGILootContainer, Log, All);

FArcGI_LootContainerTask::FArcGI_LootContainerTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcGI_LootContainerTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	AActor* InteractorActor = InstanceData.Actor;
	if (!InteractorActor)
	{
		UE_LOG(LogArcGILootContainer, Warning, TEXT("EnterState: No interactor actor."));
		return EStateTreeRunStatus::Failed;
	}

	FMassEntityHandle LootEntity = InstanceData.SmartObjectEntity;
	if (!LootEntity.IsValid())
	{
		UE_LOG(LogArcGILootContainer, Warning, TEXT("EnterState: SmartObjectEntity is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	UArcItemsStoreComponent* ItemsStore = nullptr;
	if (ItemsStoreClass)
	{
		ItemsStore = Cast<UArcItemsStoreComponent>(InteractorActor->FindComponentByClass(ItemsStoreClass));
	}
	else
	{
		ItemsStore = InteractorActor->FindComponentByClass<UArcItemsStoreComponent>();
	}

	if (!ItemsStore)
	{
		UE_LOG(LogArcGILootContainer, Warning, TEXT("EnterState: No UArcItemsStoreComponent found on interactor."));
		return EStateTreeRunStatus::Failed;
	}

	const bool bSent = Arcx::SendServerCommand<FArcLootItemCommand>(InteractorActor, ItemsStore, LootEntity);
	if (!bSent)
	{
		UE_LOG(LogArcGILootContainer, Warning, TEXT("EnterState: Failed to send FArcLootItemCommand."));
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Succeeded;
}
