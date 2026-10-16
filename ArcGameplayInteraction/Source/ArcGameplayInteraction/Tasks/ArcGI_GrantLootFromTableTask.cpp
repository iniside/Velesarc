// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcGI_GrantLootFromTableTask.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/Loot/ArcLootFragments.h"
#include "Items/Loot/ArcLootTable.h"
#include "Items/Loot/ArcLootDropSubsystem.h"
#include "Operations/ArcMassItemOperations.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "StateTreeExecutionContext.h"
#include "Engine/World.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"

namespace ArcLootGrantTask
{
	AActor* ResolveActor(FMassEntityManager& EM, FMassEntityHandle Entity)
	{
		if (!Entity.IsValid() || !EM.IsEntityValid(Entity))
		{
			return nullptr;
		}
		FMassActorFragment* Frag = EM.GetFragmentDataPtr<FMassActorFragment>(Entity);
		return Frag ? Frag->GetMutable() : nullptr;
	}
}

DEFINE_LOG_CATEGORY_STATIC(LogArcGIGrantLootFromTable, Log, All);

FArcGI_GrantLootFromTableTask::FArcGI_GrantLootFromTableTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcGI_GrantLootFromTableTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FMassEntityHandle SourceEntity = InstanceData.SmartObjectEntity;
	if (!SourceEntity.IsValid())
	{
		UE_LOG(LogArcGIGrantLootFromTable, Warning, TEXT("EnterState: SmartObjectEntity (source) is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle DestEntity = InstanceData.ContextEntity;
	if (!DestEntity.IsValid())
	{
		UE_LOG(LogArcGIGrantLootFromTable, Warning, TEXT("EnterState: ContextEntity (destination) is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	const UScriptStruct* StoreType = InstanceData.StoreType
		? InstanceData.StoreType.Get()
		: FArcMassItemStoreFragment::StaticStruct();
	if (!StoreType->IsChildOf(FArcMassItemStoreFragment::StaticStruct()))
	{
		UE_LOG(LogArcGIGrantLootFromTable, Warning, TEXT("EnterState: StoreType '%s' must derive from FArcMassItemStoreFragment."), *StoreType->GetName());
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = Context.GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		UE_LOG(LogArcGIGrantLootFromTable, Warning, TEXT("EnterState: No UMassEntitySubsystem."));
		return EStateTreeRunStatus::Failed;
	}

	UArcLootDropSubsystem* LootDropSubsystem = World->GetSubsystem<UArcLootDropSubsystem>();
	if (!LootDropSubsystem)
	{
		UE_LOG(LogArcGIGrantLootFromTable, Warning, TEXT("EnterState: No UArcLootDropSubsystem."));
		return EStateTreeRunStatus::Failed;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	const FMassEntityView SourceView(EntityManager, SourceEntity);
	const FArcLootTableConfigFragment* Cfg = SourceView.GetConstSharedFragmentDataPtr<FArcLootTableConfigFragment>();
	if (!Cfg)
	{
		UE_LOG(LogArcGIGrantLootFromTable, Warning, TEXT("EnterState: Source entity has no FArcLootTableConfigFragment — missing UArcLootTableTrait?"));
		return EStateTreeRunStatus::Failed;
	}

	UArcLootTable* LootTable = Cfg->LootTable.LoadSynchronous();
	if (!LootTable)
	{
		UE_LOG(LogArcGIGrantLootFromTable, Warning, TEXT("EnterState: Failed to load loot table '%s'."), *Cfg->LootTable.ToString());
		return EStateTreeRunStatus::Failed;
	}

	FStructView DestStoreView = EntityManager.GetMutableSparseElementDataForEntity(StoreType, DestEntity);
	if (!DestStoreView.GetPtr<FArcMassItemStoreFragment>())
	{
		UE_LOG(LogArcGIGrantLootFromTable, Warning, TEXT("EnterState: Destination entity has no '%s' store fragment."), *StoreType->GetName());
		return EStateTreeRunStatus::Failed;
	}

	// Build the loot context: start from the designer-provided LootContext if it derives from
	// FArcLootContext, otherwise default-construct. Then stamp Source/Requesting entities + actors.
	FInstancedStruct EffectiveCtx;
	if (InstanceData.LootContext.IsValid()
		&& InstanceData.LootContext.GetScriptStruct() != nullptr
		&& InstanceData.LootContext.GetScriptStruct()->IsChildOf(FArcLootContext::StaticStruct()))
	{
		EffectiveCtx = InstanceData.LootContext;
	}
	else
	{
		EffectiveCtx.InitializeAs<FArcLootContext>();
	}

	if (FArcLootContext* Ctx = EffectiveCtx.GetMutablePtr<FArcLootContext>())
	{
		Ctx->SelfEntity       = SourceEntity;
		Ctx->InstigatorEntity = DestEntity;
		Ctx->SelfActor        = ArcLootGrantTask::ResolveActor(EntityManager, SourceEntity);
		Ctx->InstigatorActor  = ArcLootGrantTask::ResolveActor(EntityManager, DestEntity);
	}

	const TArray<FArcLootDropResult> Drops =
		LootDropSubsystem->RollLootTable(LootTable, FConstStructView(EffectiveCtx));
	if (Drops.IsEmpty())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	int32 NumGranted = 0;
	for (const FArcLootDropResult& Drop : Drops)
	{
		const FArcItemId NewItemId = ArcMassItems::AddItem(EntityManager, DestEntity, StoreType, Drop.ItemSpec);
		if (NewItemId.IsValid())
		{
			++NumGranted;
		}
	}

	return NumGranted > 0 ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
