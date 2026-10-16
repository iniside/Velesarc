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

/**
 * Tests for FArcWithdrawCraftOutputCommand::Execute (ArcCraft/Commands/).
 *
 * STRATEGY:
 *   Execute is called directly on the game thread with WorldContext = &Spawner.GetWorld()
 *   (UWorld is a UObject; UWorld::GetWorld() returns this, so the resolver chain works).
 *   No deferred commands are used — Execute mutates FArcCraftOutputFragment synchronously.
 *   Item delivery uses the no-bag instigator path (degrade-to-store) to avoid the real-bag-def
 *   setup; real defs from UArcMassItemTestsSettings ensure AddItem→InitializeMass succeeds
 *   (transient defs crash InitializeMass — see the ArcCraftBagOutputTests comments).
 *
 * COVERED:
 *   1. Full withdraw (Stacks >= Available) → true; OutputItems line removed; instigator has item.
 *   2. Partial withdraw (Stacks < Available) → true; Amount decremented by placed; instigator has item.
 *   3. ItemId not present in OutputItems → false; OutputItems unchanged.
 *   4. Empty OutputItems → false immediately.
 *
 * NOT COVERED here:
 *   - Bag-spilling delivery path — covered by ArcMassBagAddItemSpillingTests.cpp.
 *   - MassSignalSubsystem OutputChanged broadcast — observable only in integration with a
 *     signal listener; pure unit scope is out.
 *
 * Unique tag prefix: TAG_WdrawOut_*
 */

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "StructUtils/InstancedStruct.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"

#include "ArcCraft/Commands/ArcWithdrawCraftOutputCommand.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemId.h"

#include "ArcMassItemTestsSettings.h"

// ---------------------------------------------------------------------------
// File-scoped helpers
// ---------------------------------------------------------------------------

namespace ArcWithdrawOutputCmdHelpers
{
	/**
	 * Create a minimal station entity carrying FArcCraftOutputFragment (regular fragment).
	 * FArcCraftStationTag is added so the entity resembles a real station.
	 */
	FMassEntityHandle BuildStationEntity(FMassEntityManager& EM)
	{
		TArray<FInstancedStruct> Frags;
		Frags.Add(FInstancedStruct::Make(FTransformFragment()));
		Frags.Add(FInstancedStruct::Make(FArcCraftOutputFragment()));
		FMassEntityHandle Entity = EM.CreateEntity(Frags);
		EM.AddTagToEntity(Entity, FArcCraftStationTag::StaticStruct());
		return Entity;
	}

	/**
	 * Create a minimal instigator entity with a sparse FArcMassItemStoreFragment.
	 * NO FArcMassBagStoreFragment → DeliverSpecToInstigator degrades to plain AddItem.
	 */
	FMassEntityHandle BuildInstigatorEntity(FMassEntityManager& EM)
	{
		TArray<FInstancedStruct> Frags;
		Frags.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle Entity = EM.CreateEntity(Frags);
		EM.AddSparseElementToEntity<FArcMassItemStoreFragment>(Entity);
		return Entity;
	}

	/**
	 * Build a FArcItemSpec stamped with a freshly generated ItemId for use in
	 * OutputItems (mirrors how the delivery path stamps output specs in practice).
	 * Uses FArcItemSpec::NewItem so both ItemDefinitionId and ItemDefinition are
	 * set (required for AddItem→InitializeMass to succeed with real defs).
	 */
	FArcItemSpec MakeStampedOutputSpec(const FArcNamedPrimaryAssetId& DefId, int32 Amount)
	{
		// FArcNamedPrimaryAssetId converts implicitly to FPrimaryAssetId.
		const FPrimaryAssetId DefAssetId = DefId;
		FArcItemSpec Spec = FArcItemSpec::NewItem(DefAssetId, 1, Amount);
		Spec.SetItemId(FArcItemId::Generate());
		return Spec;
	}
} // namespace ArcWithdrawOutputCmdHelpers

// ---------------------------------------------------------------------------
// TEST CLASS
// ---------------------------------------------------------------------------

TEST_CLASS(ArcWithdrawCraftOutputCommand, "ArcCraft.WithdrawOutput")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EM = &MES->GetMutableEntityManager();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();
	}

	// =========================================================================
	// Test 1 — Full withdraw (Stacks >= Available)
	//
	// Station OutputItems: one spec with Amount=3, stamped ItemId=SpecId.
	// Cmd(Station, Instigator, SpecId, Stacks=99).
	// Execute: ToWithdraw = min(3, 99) = 3; Placed = 3 = Available.
	//
	// Expected: returns true; OutputItems empty; instigator has 1 item entry.
	// =========================================================================
	TEST_METHOD(FullWithdraw_RemovesLine_InstigatorReceivesItem)
	{
		using namespace ArcWithdrawOutputCmdHelpers;

		FMassEntityHandle StationEntity = BuildStationEntity(*EM);
		FMassEntityHandle InstigatorEntity = BuildInstigatorEntity(*EM);

		// Seed the output fragment with one spec.
		const int32 Available = 3;
		FArcItemSpec OutputSpec = MakeStampedOutputSpec(MassSettings->SimpleBaseItem, Available);
		const FArcItemId SpecId = OutputSpec.ItemId;

		FArcCraftOutputFragment* OutputFrag = EM->GetFragmentDataPtr<FArcCraftOutputFragment>(StationEntity);
		ASSERT_THAT(IsNotNull(OutputFrag));
		OutputFrag->OutputItems.Add(OutputSpec);
		ASSERT_THAT(AreEqual(1, OutputFrag->OutputItems.Num()));

		// Execute: request more than Available — should cap to Available.
		FArcWithdrawCraftOutputCommand Cmd(StationEntity, InstigatorEntity, SpecId, /*Stacks=*/99);
		UWorld* World = &Spawner.GetWorld();
		const bool bOk = Cmd.Execute(World);

		ASSERT_THAT(IsTrue(bOk, TEXT("Execute must return true for a valid full withdraw")));

		// OutputItems must be empty (the line was removed).
		OutputFrag = EM->GetFragmentDataPtr<FArcCraftOutputFragment>(StationEntity);
		ASSERT_THAT(IsNotNull(OutputFrag));
		ASSERT_THAT(AreEqual(0, OutputFrag->OutputItems.Num(),
			TEXT("OutputItems must be empty after a full withdraw")));

		// Instigator must hold the item (1 item entry, Available stacks).
		const int32 ItemCount = ArcMassItems::CountItemsByDefinition(
			*EM, InstigatorEntity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->SimpleBaseItem);
		ASSERT_THAT(AreEqual(Available, ItemCount,
			TEXT("Instigator must receive Available stacks after full withdraw")));
	}

	// =========================================================================
	// Test 2 — Partial withdraw (Stacks < Available)
	//
	// Station OutputItems: one spec with Amount=5, stamped ItemId=SpecId.
	// Cmd(Station, Instigator, SpecId, Stacks=2).
	// Execute: ToWithdraw = min(5, 2) = 2; Placed = 2 < Available.
	//
	// Expected: returns true; OutputItems[0].Amount decremented to 3;
	// instigator has 1 item entry.
	// =========================================================================
	TEST_METHOD(PartialWithdraw_DecrementsAmount_InstigatorReceivesStacks)
	{
		using namespace ArcWithdrawOutputCmdHelpers;

		FMassEntityHandle StationEntity = BuildStationEntity(*EM);
		FMassEntityHandle InstigatorEntity = BuildInstigatorEntity(*EM);

		const int32 Available = 5;
		const int32 Requested = 2;
		FArcItemSpec OutputSpec = MakeStampedOutputSpec(MassSettings->SimpleBaseItem, Available);
		const FArcItemId SpecId = OutputSpec.ItemId;

		FArcCraftOutputFragment* OutputFrag = EM->GetFragmentDataPtr<FArcCraftOutputFragment>(StationEntity);
		ASSERT_THAT(IsNotNull(OutputFrag));
		OutputFrag->OutputItems.Add(OutputSpec);

		FArcWithdrawCraftOutputCommand Cmd(StationEntity, InstigatorEntity, SpecId, Requested);
		UWorld* World = &Spawner.GetWorld();
		const bool bOk = Cmd.Execute(World);

		ASSERT_THAT(IsTrue(bOk, TEXT("Execute must return true for a valid partial withdraw")));

		// The line must still exist with Amount decremented by Requested.
		OutputFrag = EM->GetFragmentDataPtr<FArcCraftOutputFragment>(StationEntity);
		ASSERT_THAT(IsNotNull(OutputFrag));
		ASSERT_THAT(AreEqual(1, OutputFrag->OutputItems.Num(),
			TEXT("OutputItems must still have one entry after a partial withdraw")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(Available - Requested),
			static_cast<int32>(OutputFrag->OutputItems[0].Amount),
			TEXT("OutputItems[0].Amount must be decremented by the placed count")));

		// Instigator must hold the withdrawn portion.
		const int32 ItemCount = ArcMassItems::CountItemsByDefinition(
			*EM, InstigatorEntity, FArcMassItemStoreFragment::StaticStruct(),
			MassSettings->SimpleBaseItem);
		ASSERT_THAT(AreEqual(Requested, ItemCount,
			TEXT("Instigator must have Requested stacks after partial withdraw")));
	}

	// =========================================================================
	// Test 3 — ItemId not present in OutputItems
	//
	// Station OutputItems: one spec with ItemId=SpecId.
	// Cmd(..., DifferentId, 1). ItemId not found → IndexOfByKey = INDEX_NONE.
	//
	// Expected: returns false; OutputItems unchanged.
	// =========================================================================
	TEST_METHOD(WrongItemId_ReturnsFalse_OutputUnchanged)
	{
		using namespace ArcWithdrawOutputCmdHelpers;

		FMassEntityHandle StationEntity = BuildStationEntity(*EM);
		FMassEntityHandle InstigatorEntity = BuildInstigatorEntity(*EM);

		// Seed with a spec carrying SpecId.
		FArcItemSpec OutputSpec;
		OutputSpec.SetItemId(FArcItemId::Generate());
		OutputSpec.Amount = 2;
		const FArcItemId SpecId = OutputSpec.ItemId;

		FArcCraftOutputFragment* OutputFrag = EM->GetFragmentDataPtr<FArcCraftOutputFragment>(StationEntity);
		ASSERT_THAT(IsNotNull(OutputFrag));
		OutputFrag->OutputItems.Add(OutputSpec);

		// Request with a DIFFERENT id — should not be found.
		const FArcItemId DifferentId = FArcItemId::Generate();
		ASSERT_THAT(IsFalse(DifferentId == SpecId, TEXT("DifferentId must not equal SpecId")));

		FArcWithdrawCraftOutputCommand Cmd(StationEntity, InstigatorEntity, DifferentId, 1);
		UWorld* World = &Spawner.GetWorld();
		const bool bOk = Cmd.Execute(World);

		ASSERT_THAT(IsFalse(bOk, TEXT("Execute must return false when ItemId is not in OutputItems")));

		// OutputItems must be unchanged.
		OutputFrag = EM->GetFragmentDataPtr<FArcCraftOutputFragment>(StationEntity);
		ASSERT_THAT(IsNotNull(OutputFrag));
		ASSERT_THAT(AreEqual(1, OutputFrag->OutputItems.Num(),
			TEXT("OutputItems must remain unchanged when ItemId is not found")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(OutputFrag->OutputItems[0].Amount),
			TEXT("OutputItems[0].Amount must be unchanged")));
	}

	// =========================================================================
	// Test 4 — Empty OutputItems
	//
	// Station OutputItems is empty. Cmd(..., AnyId, 1).
	// IndexOfByKey on empty array returns INDEX_NONE immediately.
	//
	// Expected: returns false.
	// =========================================================================
	TEST_METHOD(EmptyOutputItems_ReturnsFalse)
	{
		using namespace ArcWithdrawOutputCmdHelpers;

		FMassEntityHandle StationEntity = BuildStationEntity(*EM);
		FMassEntityHandle InstigatorEntity = BuildInstigatorEntity(*EM);

		// Confirm OutputItems starts empty.
		FArcCraftOutputFragment* OutputFrag = EM->GetFragmentDataPtr<FArcCraftOutputFragment>(StationEntity);
		ASSERT_THAT(IsNotNull(OutputFrag));
		ASSERT_THAT(AreEqual(0, OutputFrag->OutputItems.Num()));

		FArcWithdrawCraftOutputCommand Cmd(StationEntity, InstigatorEntity, FArcItemId::Generate(), 1);
		UWorld* World = &Spawner.GetWorld();
		const bool bOk = Cmd.Execute(World);

		ASSERT_THAT(IsFalse(bOk, TEXT("Execute must return false when OutputItems is empty")));

		// Still empty.
		OutputFrag = EM->GetFragmentDataPtr<FArcCraftOutputFragment>(StationEntity);
		ASSERT_THAT(AreEqual(0, OutputFrag->OutputItems.Num(),
			TEXT("OutputItems must remain empty after a failed withdraw")));
	}
};
