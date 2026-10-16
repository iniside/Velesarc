// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcGI_GrantItemsTask.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemSpec.h"
#include "Items/Loot/ArcItemGrantFragments.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Operations/ArcMassItemOperations.h"
#include "StateTreeExecutionContext.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcGIGrantItems, Log, All);

FArcGI_GrantItemsTask::FArcGI_GrantItemsTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcGI_GrantItemsTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FMassEntityHandle SourceEntity = InstanceData.SmartObjectEntity;
	if (!SourceEntity.IsValid())
	{
		UE_LOG(LogArcGIGrantItems, Warning, TEXT("EnterState: SmartObjectEntity (source) is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle DestEntity = InstanceData.ContextEntity;
	if (!DestEntity.IsValid())
	{
		UE_LOG(LogArcGIGrantItems, Warning, TEXT("EnterState: ContextEntity (destination) is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	const UScriptStruct* StoreType = InstanceData.StoreType
		? InstanceData.StoreType.Get()
		: FArcMassItemStoreFragment::StaticStruct();
	if (!StoreType->IsChildOf(FArcMassItemStoreFragment::StaticStruct()))
	{
		UE_LOG(LogArcGIGrantItems, Warning, TEXT("EnterState: StoreType '%s' must derive from FArcMassItemStoreFragment."), *StoreType->GetName());
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = Context.GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		UE_LOG(LogArcGIGrantItems, Warning, TEXT("EnterState: No UMassEntitySubsystem."));
		return EStateTreeRunStatus::Failed;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	FMassEntityView SourceEntityView(EntityManager, SourceEntity);
	FArcItemGrantFragment* SourceFragment = SourceEntityView.GetSparseFragmentDataPtr<FArcItemGrantFragment>();
	if (!SourceFragment)
	{
		UE_LOG(LogArcGIGrantItems, Warning, TEXT("EnterState: Source entity has no FArcItemGrantFragment."));
		return EStateTreeRunStatus::Failed;
	}

	FStructView DestStoreView = EntityManager.GetMutableSparseElementDataForEntity(StoreType, DestEntity);
	if (!DestStoreView.GetPtr<FArcMassItemStoreFragment>())
	{
		UE_LOG(LogArcGIGrantItems, Warning, TEXT("EnterState: Destination entity has no '%s' store fragment."), *StoreType->GetName());
		return EStateTreeRunStatus::Failed;
	}

	int32 NumGranted = 0;
	int32 WriteIdx = 0;
	for (int32 ReadIdx = 0; ReadIdx < SourceFragment->Entries.Num(); ++ReadIdx)
	{
		const FArcItemGrantEntry& Entry = SourceFragment->Entries[ReadIdx];

		const FPrimaryAssetId& DefinitionId = Entry.Definition;
		if (!DefinitionId.IsValid())
		{
			if (WriteIdx != ReadIdx)
			{
				SourceFragment->Entries[WriteIdx] = MoveTemp(SourceFragment->Entries[ReadIdx]);
			}
			++WriteIdx;
			continue;
		}

		FArcItemSpec Spec = FArcItemSpec::NewItem(DefinitionId, Entry.Level, Entry.Amount);
		const FArcItemId NewItemId = ArcMassItems::AddItem(EntityManager, DestEntity, StoreType, Spec);
		if (NewItemId.IsValid())
		{
			++NumGranted;
			continue;
		}

		if (WriteIdx != ReadIdx)
		{
			SourceFragment->Entries[WriteIdx] = MoveTemp(SourceFragment->Entries[ReadIdx]);
		}
		++WriteIdx;
	}
	SourceFragment->Entries.SetNum(WriteIdx, EAllowShrinking::No);

	return NumGranted > 0 ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
