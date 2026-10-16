// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcGI_GrantLootFromTableToBagTask.h"

#include "Bags/ArcBagOperations.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemSpec.h"
#include "Items/Loot/ArcLootFragments.h"
#include "Items/Loot/ArcLootTable.h"
#include "Items/Loot/ArcLootDropSubsystem.h"
#include "Operations/ArcMassItemOperations.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "StateTreeExecutionContext.h"
#include "Engine/World.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"

namespace ArcLootGrantToBagTask
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

	// Resolve a spawn transform for the spill container. The looted container (PreferEntity)
	// is the reliable transform holder; the recipient (FallbackEntity) may be a store-only
	// entity without one. No origin/identity fallback — returns false if neither has a transform.
	bool ResolveSpawnXform(FMassEntityManager& EntityManager, FMassEntityHandle PreferEntity, FMassEntityHandle FallbackEntity, FTransform& Out)
	{
		if (const FTransformFragment* PreferFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(PreferEntity))
		{
			Out = PreferFrag->GetTransform();
			return true;
		}
		if (const FTransformFragment* FallbackFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(FallbackEntity))
		{
			Out = FallbackFrag->GetTransform();
			return true;
		}
		return false;
	}
}

DEFINE_LOG_CATEGORY_STATIC(LogArcGIGrantLootToBag, Log, All);

FArcGI_GrantLootFromTableToBagTask::FArcGI_GrantLootFromTableToBagTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcGI_GrantLootFromTableToBagTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FMassEntityHandle SourceEntity = InstanceData.SmartObjectEntity;
	if (!SourceEntity.IsValid())
	{
		UE_LOG(LogArcGIGrantLootToBag, Warning, TEXT("EnterState: SmartObjectEntity (source) is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle DestEntity = InstanceData.ContextEntity;
	if (!DestEntity.IsValid())
	{
		UE_LOG(LogArcGIGrantLootToBag, Warning, TEXT("EnterState: ContextEntity (destination) is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = Context.GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		UE_LOG(LogArcGIGrantLootToBag, Warning, TEXT("EnterState: No UMassEntitySubsystem."));
		return EStateTreeRunStatus::Failed;
	}

	UArcLootDropSubsystem* LootDropSubsystem = World->GetSubsystem<UArcLootDropSubsystem>();
	if (!LootDropSubsystem)
	{
		UE_LOG(LogArcGIGrantLootToBag, Warning, TEXT("EnterState: No UArcLootDropSubsystem."));
		return EStateTreeRunStatus::Failed;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	const FMassEntityView SourceView(EntityManager, SourceEntity);
	const FArcLootTableConfigFragment* Cfg = SourceView.GetConstSharedFragmentDataPtr<FArcLootTableConfigFragment>();
	if (!Cfg)
	{
		UE_LOG(LogArcGIGrantLootToBag, Warning, TEXT("EnterState: Source entity has no FArcLootTableConfigFragment — missing UArcLootTableTrait?"));
		return EStateTreeRunStatus::Failed;
	}

	UArcLootTable* LootTable = Cfg->LootTable.LoadSynchronous();
	if (!LootTable)
	{
		UE_LOG(LogArcGIGrantLootToBag, Warning, TEXT("EnterState: Failed to load loot table '%s'."), *Cfg->LootTable.ToString());
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
		Ctx->SelfActor        = ArcLootGrantToBagTask::ResolveActor(EntityManager, SourceEntity);
		Ctx->InstigatorActor  = ArcLootGrantToBagTask::ResolveActor(EntityManager, DestEntity);
	}

	const TArray<FArcLootDropResult> Drops =
		LootDropSubsystem->RollLootTable(LootTable, FConstStructView(EffectiveCtx));
	if (Drops.IsEmpty())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// Bag-overlay guard: without a bag overlay every AddItemToFirstFreeSlot would roll back
	// (nothing accepts the item), so nothing would ever land in the recipient — fail loudly.
	if (!EntityManager.GetMutableSparseElementDataForEntity(FArcMassBagStoreFragment::StaticStruct(), DestEntity).IsValid())
	{
		UE_LOG(LogArcGIGrantLootToBag, Warning, TEXT("EnterState: recipient has no bag overlay — nothing will be granted to bag."));
		return EStateTreeRunStatus::Failed;
	}

	TArray<FArcItemSpec> Overflow;
	int32 NumGranted = 0;
	for (const FArcLootDropResult& Drop : Drops)
	{
		const FArcItemId Id = ArcMassItems::Bags::AddItemToFirstFreeSlot(EntityManager, DestEntity, Drop.ItemSpec);
		if (Id.IsValid())
		{
			++NumGranted;
		}
		else
		{
			Overflow.Add(Drop.ItemSpec);
		}
	}

	if (Overflow.Num() > 0 && InstanceData.SpillContainerConfig)
	{
		FTransform SpillXform;
		if (ArcLootGrantToBagTask::ResolveSpawnXform(EntityManager, SourceEntity, DestEntity, SpillXform))
		{
			const int32 OverflowCount = Overflow.Num();
			const FMassEntityHandle Spilled = LootDropSubsystem->SpawnLootContainer(InstanceData.SpillContainerConfig, SpillXform, MoveTemp(Overflow));
			if (!Spilled.IsValid())
			{
				UE_LOG(LogArcGIGrantLootToBag, Warning, TEXT("EnterState: spill container spawn failed — %d overflow item(s) lost."), OverflowCount);
			}
		}
		else
		{
			UE_LOG(LogArcGIGrantLootToBag, Warning, TEXT("EnterState: no transform for spill location — %d overflow item(s) dropped."), Overflow.Num());
		}
	}
	// Overflow with no SpillContainerConfig (or a failed spill) is dropped: succeed iff ≥1 placed.

	return NumGranted > 0 ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
