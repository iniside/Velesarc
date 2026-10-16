/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "CQTest.h"
#include "ArcItemTestRecordFiles.h"
#include "Components/ActorTestSpawner.h"

#include "StructUtils/InstancedStruct.h"

#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"

#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"

#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftTickProcessor.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"

// ---------------------------------------------------------------------------
// File-scoped helpers
// ---------------------------------------------------------------------------

namespace ArcCraftStationWithdrawHelpers
{
	/**
	 * Build a station entity that carries:
	 *   - FArcCraftInputFragment (regular fragment; seed InputItems after creation)
	 *   - FArcCraftStationTag, FArcCraftAutoTickTag
	 *
	 * Minimal set — no delivery config, no recipe list.
	 */
	FMassEntityHandle BuildMinimalStationEntity(FMassEntityManager& EM)
	{
		TArray<FInstancedStruct> Frags;
		Frags.Add(FInstancedStruct::Make(FTransformFragment()));
		Frags.Add(FInstancedStruct::Make(FArcCraftInputFragment()));

		FMassEntityHandle Entity = EM.CreateEntity(Frags);
		EM.AddTagToEntity(Entity, FArcCraftStationTag::StaticStruct());
		return Entity;
	}

	/**
	 * Build an instigator entity with FArcMassItemStoreFragment (sparse) and
	 * FArcMassItemEventFragment (sparse, auto-added by ArcMassItems::AddItem).
	 * Uses UE::Mass::FEntityBuilder so the sparse fragment is committed correctly.
	 */
	FMassEntityHandle BuildInstigatorEntity(FMassEntityManager& EM)
	{
		UE::Mass::FEntityBuilder Builder(EM);
		FMassEntityHandle Entity = Builder
			.Add<FTransformFragment>()
			.Add<FArcMassItemStoreFragment>()
			.Commit();
		return Entity;
	}
}

// ---------------------------------------------------------------------------
// Test class: withdraw from the station input
// ---------------------------------------------------------------------------

TEST_CLASS(ArcCraftStationWithdraw, "ArcCraft.StationWithdraw")
{
	FArcItemTestRecordFiles ItemRecords;
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;
	UArcCraftTickProcessor* FlushProcessor = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EM = &MES->GetMutableEntityManager();

		// Used only to drain the deferred command buffer via Executor::Run — the
		// withdraw SUT pushes through EntityManager.Defer(). The minimal
		// station/instigator entities never match the craft query, so the processor
		// itself is a no-op here; we rely on bFlushCommandBuffer=true to apply the
		// pending deferred commands. (Never call EntityManager.FlushCommands directly.)
		TSharedRef<FMassEntityManager> SharedEM = EM->AsShared();
		FlushProcessor = NewObject<UArcCraftTickProcessor>();
		FlushProcessor->CallInitialize(EM->GetOwner(), SharedEM);
	}

	/** Flush the entity manager's deferred command buffer by running a processor
	 *  with bFlushCommandBuffer=true (sanctioned alternative to FlushCommands). */
	void FlushDeferred()
	{
		UE::Mass::FProcessingContext ProcessingContext(*EM, 0.0f, /*bFlushCommandBuffer=*/true);
		UE::Mass::Executor::Run(*FlushProcessor, ProcessingContext);
	}

	// =========================================================================
	// Withdraw returns a station input item
	//
	// Station input pre-seeded with spec X. After WithdrawItem_Mass + flush:
	//   - Station InputItems is empty.
	//   - Instigator store contains a spec with ItemId == X.
	// =========================================================================
	TEST_METHOD(WithdrawItem_MovesItemFromStationInputToInstigatorStore)
	{
		using namespace ArcCraftStationWithdrawHelpers;

		const FPrimaryAssetId Def = ItemRecords.Write(FArcItemDefinitionData());
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcItemId ItemId = FArcItemId::Generate();

		FArcItemSpec StationSpec;
		StationSpec.SetItemDefinition(Def);
		StationSpec.SetItemId(ItemId);
		StationSpec.Amount = static_cast<uint16>(2);

		// Build station entity and seed InputItems directly (bypass AddItem).
		FMassEntityHandle StationEntity = BuildMinimalStationEntity(*EM);
		FArcCraftInputFragment* InputFrag =
			EM->GetFragmentDataPtr<FArcCraftInputFragment>(StationEntity);
		ASSERT_THAT(IsNotNull(InputFrag));
		InputFrag->InputItems.Add(StationSpec);

		// Build instigator entity (empty store — withdraw returns item here).
		FMassEntityHandle InstigatorEntity = BuildInstigatorEntity(*EM);

		FArcCraftItemSource_EntityStore Source;
		const bool bWithdrawQueued = Source.WithdrawItem_Mass(
			*EM, StationEntity, InstigatorEntity, ItemId, 2);
		ASSERT_THAT(IsTrue(bWithdrawQueued,
			TEXT("WithdrawItem_Mass must return true (deferred command queued)")));

		// Apply the deferred withdraw command.
		FlushDeferred();

		// Assert: station InputItems is now empty.
		ASSERT_THAT(AreEqual(0, InputFrag->InputItems.Num(),
			TEXT("Station InputItems must be empty after withdraw")));

		// Assert: instigator store contains the withdrawn item.
		const FArcMassItemStoreFragment* InstigatorStore =
			FMassEntityView(*EM, InstigatorEntity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		ASSERT_THAT(IsNotNull(InstigatorStore,
			TEXT("Instigator FArcMassItemStoreFragment must exist after withdraw")));
		const FArcMassReplicatedItem* ReturnedItem =
			const_cast<FArcMassReplicatedItemArray&>(InstigatorStore->ReplicatedItems).FindById(ItemId);
		ASSERT_THAT(IsNotNull(ReturnedItem,
			TEXT("Withdrawn item must appear in instigator store with matching ItemId")));
	}
};
