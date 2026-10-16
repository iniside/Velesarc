// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcGI_TransferLootContainerToBagTask.h"

#include "Bags/ArcBagOperations.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/Loot/ArcLootFragments.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "StateTreeExecutionContext.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcGITransferLootToBag, Log, All);

FArcGI_TransferLootContainerToBagTask::FArcGI_TransferLootContainerToBagTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcGI_TransferLootContainerToBagTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FMassEntityHandle SourceEntity = InstanceData.SmartObjectEntity;
	if (!SourceEntity.IsValid())
	{
		UE_LOG(LogArcGITransferLootToBag, Warning, TEXT("EnterState: SmartObjectEntity (source) is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle DestEntity = InstanceData.ContextEntity;
	if (!DestEntity.IsValid())
	{
		UE_LOG(LogArcGITransferLootToBag, Warning, TEXT("EnterState: ContextEntity (destination) is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = Context.GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		UE_LOG(LogArcGITransferLootToBag, Warning, TEXT("EnterState: No UMassEntitySubsystem."));
		return EStateTreeRunStatus::Failed;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	FMassEntityView SourceEntityView(EntityManager, SourceEntity);
	FArcLootContainerFragment* SourceFragment = SourceEntityView.GetSparseFragmentDataPtr<FArcLootContainerFragment>();
	if (!SourceFragment)
	{
		UE_LOG(LogArcGITransferLootToBag, Warning, TEXT("EnterState: Source entity has no FArcLootContainerFragment."));
		return EStateTreeRunStatus::Failed;
	}

	// Bag-overlay guard: the target store is derived from the bag overlay (TargetStoreType), so a missing
	// overlay means there is nowhere to route loot — every item would silently bounce. Fail fast.
	if (!EntityManager.GetMutableSparseElementDataForEntity(FArcMassBagStoreFragment::StaticStruct(), DestEntity).IsValid())
	{
		UE_LOG(LogArcGITransferLootToBag, Warning, TEXT("EnterState: recipient entity has no bag overlay — nothing will transfer."));
		return EStateTreeRunStatus::Failed;
	}

	int32 NumTransferred = 0;
	int32 WriteIdx = 0;
	for (int32 ReadIdx = 0; ReadIdx < SourceFragment->Items.Num(); ++ReadIdx)
	{
		const FArcItemId NewItemId = ArcMassItems::Bags::AddItemToFirstFreeSlot(EntityManager, DestEntity, SourceFragment->Items[ReadIdx]);
		if (NewItemId.IsValid())
		{
			++NumTransferred;
			continue;
		}

		// No free accepting bag slot (already rolled back); keep this spec in the source for retry.
		if (WriteIdx != ReadIdx)
		{
			SourceFragment->Items[WriteIdx] = MoveTemp(SourceFragment->Items[ReadIdx]);
		}
		++WriteIdx;
	}
	SourceFragment->Items.SetNum(WriteIdx, EAllowShrinking::No);

	return NumTransferred > 0 ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
