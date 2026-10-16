// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcGI_TransferLootContainerTask.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/Loot/ArcLootFragments.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Operations/ArcMassItemOperations.h"
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

	const UScriptStruct* StoreType = InstanceData.StoreType
		? InstanceData.StoreType.Get()
		: FArcMassItemStoreFragment::StaticStruct();
	if (!StoreType->IsChildOf(FArcMassItemStoreFragment::StaticStruct()))
	{
		UE_LOG(LogArcGITransferLoot, Warning, TEXT("EnterState: StoreType '%s' must derive from FArcMassItemStoreFragment."), *StoreType->GetName());
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

	FMassEntityView SourceEntityView(EntityManager, SourceEntity);
	FArcLootContainerFragment* SourceFragment = SourceEntityView.GetSparseFragmentDataPtr<FArcLootContainerFragment>();
	if (!SourceFragment)
	{
		UE_LOG(LogArcGITransferLoot, Warning, TEXT("EnterState: Source entity has no FArcLootContainerFragment."));
		return EStateTreeRunStatus::Failed;
	}

	FStructView DestStoreView = EntityManager.GetMutableSparseElementDataForEntity(StoreType, DestEntity);
	if (!DestStoreView.GetPtr<FArcMassItemStoreFragment>())
	{
		UE_LOG(LogArcGITransferLoot, Warning, TEXT("EnterState: Destination entity has no '%s' store fragment."), *StoreType->GetName());
		return EStateTreeRunStatus::Failed;
	}

	int32 NumTransferred = 0;
	int32 WriteIdx = 0;
	for (int32 ReadIdx = 0; ReadIdx < SourceFragment->Items.Num(); ++ReadIdx)
	{
		const FArcItemId NewItemId = ArcMassItems::AddItem(EntityManager, DestEntity, StoreType, SourceFragment->Items[ReadIdx]);
		if (NewItemId.IsValid())
		{
			++NumTransferred;
			continue;
		}

		// Rejected (e.g. stacking constraint); keep this spec in the source for retry.
		if (WriteIdx != ReadIdx)
		{
			SourceFragment->Items[WriteIdx] = MoveTemp(SourceFragment->Items[ReadIdx]);
		}
		++WriteIdx;
	}
	SourceFragment->Items.SetNum(WriteIdx, EAllowShrinking::No);

	return NumTransferred > 0 ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
