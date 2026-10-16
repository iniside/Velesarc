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
 * Tests for UArcCraftingViewModel::RebuildStationOutput / SetServerInput
 * (shared station-surface FieldNotify arrays + recycle-pool / prune mechanics).
 *
 * STRATEGY:
 *   RebuildStationOutput and SetServerInput take FArcItemSpec arrays directly — no Mass world required.
 *   UArcItemViewModel::RefreshFrom and UArcIngredientCandidateViewModel::RefreshFrom tolerate an
 *   unresolved def (name/icon left empty), so specs need only a stamped ItemId + Amount. FieldNotify fires into the
 *   void (no widget bindings in tests) — that is expected and harmless.
 *
 * COVERED:
 *   1. Populate: Rebuild with N specs → Num() == N; each VM carries the right ItemId + Stacks.
 *   2. Recycle: capture VM ptr for id X; rebuild again with X present → SAME pointer reused.
 *   3. Prune: rebuild without id X → X absent from array; after a 3rd rebuild with X again
 *             a NEW VM is allocated (proves X was removed from the recycle map in step 2).
 *   4. StationInput (server rows set via SetServerInput) mirrors the same behaviour
 *      (populate, recycle, prune — combined case).
 *
 * Unique tag prefix: none needed (no gameplay tags used in this file).
 */

#include "CQTest.h"

#include "Items/ArcItemSpec.h"
#include "Items/ArcItemId.h"

#include "ViewModels/ArcCraftingViewModel.h"
#include "ViewModels/ArcItemViewModel.h"
#include "ViewModels/ArcIngredientCandidateViewModel.h"

#include "Fragments/ArcMassItemStoreFragment.h"

// ---------------------------------------------------------------------------
// File-scoped helpers
// ---------------------------------------------------------------------------

namespace ArcStationSurfaceVMHelpers
{
	/**
	 * Build a minimal FArcItemSpec with a stamped ItemId and the given Amount.
	 * No item definition is set — RefreshFrom tolerates an unresolved def.
	 */
	FArcItemSpec MakeSpec(int32 Amount)
	{
		FArcItemSpec Spec;
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(Amount);
		return Spec;
	}

	/**
	 * Build a minimal FArcItemSpec carrying the provided ItemId and Amount.
	 * Used when rebuilding with an id that was captured in a previous rebuild.
	 */
	FArcItemSpec MakeSpecWithId(const FArcItemId& InId, int32 Amount)
	{
		FArcItemSpec Spec;
		Spec.SetItemId(InId);
		Spec.Amount = static_cast<uint16>(Amount);
		return Spec;
	}

	/**
	 * True if Haystack contains a VM whose GetItemId() == NeedleId.
	 */
	bool ArrayContainsId(
		const TArray<TObjectPtr<UArcItemViewModel>>& Haystack,
		const FArcItemId& NeedleId)
	{
		for (const TObjectPtr<UArcItemViewModel>& VM : Haystack)
		{
			if (VM != nullptr && VM->GetItemId() == NeedleId)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Find the first VM in Haystack whose GetItemId() == NeedleId; nullptr if absent.
	 */
	UArcItemViewModel* FindVMById(
		const TArray<TObjectPtr<UArcItemViewModel>>& Haystack,
		const FArcItemId& NeedleId)
	{
		for (const TObjectPtr<UArcItemViewModel>& VM : Haystack)
		{
			if (VM != nullptr && VM->GetItemId() == NeedleId)
			{
				return VM.Get();
			}
		}
		return nullptr;
	}

	/**
	 * True if Haystack contains a station input row whose GetItemId() == NeedleId.
	 */
	bool ArrayContainsId(
		const TArray<TObjectPtr<UArcIngredientCandidateViewModel>>& Haystack,
		const FArcItemId& NeedleId)
	{
		for (const TObjectPtr<UArcIngredientCandidateViewModel>& VM : Haystack)
		{
			if (VM != nullptr && VM->GetItemId() == NeedleId)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Find the first station input row in Haystack whose GetItemId() == NeedleId; nullptr if absent.
	 */
	UArcIngredientCandidateViewModel* FindVMById(
		const TArray<TObjectPtr<UArcIngredientCandidateViewModel>>& Haystack,
		const FArcItemId& NeedleId)
	{
		for (const TObjectPtr<UArcIngredientCandidateViewModel>& VM : Haystack)
		{
			if (VM != nullptr && VM->GetItemId() == NeedleId)
			{
				return VM.Get();
			}
		}
		return nullptr;
	}
} // namespace ArcStationSurfaceVMHelpers

// ---------------------------------------------------------------------------
// TEST CLASS
// ---------------------------------------------------------------------------

TEST_CLASS(ArcCraftingViewModelStationSurface, "ArcUIModels.CraftingVM.StationSurface")
{
	UArcCraftingViewModel* CraftVM = nullptr;

	BEFORE_EACH()
	{
		// Create a fresh VM per test — ensures recycle maps start empty.
		CraftVM = NewObject<UArcCraftingViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		check(CraftVM);
	}

	// =========================================================================
	// Test 1 — Populate: N specs → N VMs with matching ItemId and Stacks
	//
	// RebuildStationOutput with 3 specs. GetStationOutput() must have 3 VMs
	// each carrying the matching ItemId and Stacks == Spec.Amount.
	// =========================================================================
	TEST_METHOD(StationOutput_Populate_VmsMatchSpecs)
	{
		using namespace ArcStationSurfaceVMHelpers;

		FArcItemSpec SpecA = MakeSpec(5);
		FArcItemSpec SpecB = MakeSpec(10);
		FArcItemSpec SpecC = MakeSpec(3);

		TArray<FArcItemSpec> Specs;
		Specs.Add(SpecA);
		Specs.Add(SpecB);
		Specs.Add(SpecC);

		CraftVM->RebuildStationOutput(Specs, nullptr);

		const TArray<TObjectPtr<UArcItemViewModel>>& Output = CraftVM->GetStationOutput();
		ASSERT_THAT(AreEqual(3, Output.Num(),
			TEXT("GetStationOutput must have 3 VMs after rebuilding with 3 specs")));

		// Each VM carries the matching ItemId.
		ASSERT_THAT(IsTrue(ArrayContainsId(Output, SpecA.ItemId), TEXT("VM for SpecA must be present")));
		ASSERT_THAT(IsTrue(ArrayContainsId(Output, SpecB.ItemId), TEXT("VM for SpecB must be present")));
		ASSERT_THAT(IsTrue(ArrayContainsId(Output, SpecC.ItemId), TEXT("VM for SpecC must be present")));

		// Stacks on each VM match the spec Amount.
		UArcItemViewModel* VMA = FindVMById(Output, SpecA.ItemId);
		ASSERT_THAT(IsNotNull(VMA));
		ASSERT_THAT(AreEqual(5, VMA->GetStacks(), TEXT("VM for SpecA must have Stacks == 5")));

		UArcItemViewModel* VMB = FindVMById(Output, SpecB.ItemId);
		ASSERT_THAT(IsNotNull(VMB));
		ASSERT_THAT(AreEqual(10, VMB->GetStacks(), TEXT("VM for SpecB must have Stacks == 10")));
	}

	// =========================================================================
	// Test 2 — Recycle: id X present in two consecutive rebuilds → SAME VM ptr
	//
	// First rebuild: spec X (Amount=2) + spec Y (Amount=7).
	// Capture VM ptr for X. Second rebuild: X (Amount=4) + Y + new Z.
	// The VM for X must be the same object (pointer identity), and Stacks updated.
	// =========================================================================
	TEST_METHOD(StationOutput_Recycle_SameVmPtrReused)
	{
		using namespace ArcStationSurfaceVMHelpers;

		FArcItemSpec SpecX = MakeSpec(2);
		FArcItemSpec SpecY = MakeSpec(7);
		const FArcItemId IdX = SpecX.ItemId;
		const FArcItemId IdY = SpecY.ItemId;

		// First rebuild.
		{
			TArray<FArcItemSpec> Specs;
			Specs.Add(SpecX);
			Specs.Add(SpecY);
			CraftVM->RebuildStationOutput(Specs, nullptr);
		}

		// Capture the VM pointer for X.
		UArcItemViewModel* VMX_First = FindVMById(CraftVM->GetStationOutput(), IdX);
		ASSERT_THAT(IsNotNull(VMX_First, TEXT("VM for X must be present after first rebuild")));

		// Second rebuild with X (different Amount) + Y + new Z.
		FArcItemSpec SpecXUpdated = MakeSpecWithId(IdX, 4);
		FArcItemSpec SpecZ = MakeSpec(9);
		const FArcItemId IdZ = SpecZ.ItemId;
		{
			TArray<FArcItemSpec> Specs2;
			Specs2.Add(SpecXUpdated);
			Specs2.Add(SpecY);
			Specs2.Add(SpecZ);
			CraftVM->RebuildStationOutput(Specs2, nullptr);
		}

		const TArray<TObjectPtr<UArcItemViewModel>>& Output2 = CraftVM->GetStationOutput();
		ASSERT_THAT(AreEqual(3, Output2.Num(),
			TEXT("GetStationOutput must have 3 VMs after second rebuild")));

		// VM for X must be the SAME object (recycle map returned the cached VM).
		UArcItemViewModel* VMX_Second = FindVMById(Output2, IdX);
		ASSERT_THAT(IsNotNull(VMX_Second, TEXT("VM for X must still be present after second rebuild")));
		ASSERT_THAT(IsTrue(VMX_First == VMX_Second,
			TEXT("VM pointer for X must be identical (recycled, not reallocated)")));

		// Stacks must reflect the updated Amount.
		ASSERT_THAT(AreEqual(4, VMX_Second->GetStacks(),
			TEXT("Recycled VM for X must have updated Stacks == 4")));
	}

	// =========================================================================
	// Test 3 — Prune: id X dropped from specs → removed from array AND recycle map
	//
	// Rebuild 1: {X, Y}. Capture VMX ptr.
	// Rebuild 2: {Y} only — X is dropped. X must be absent from the array.
	// Rebuild 3: {X, Y} again — the VM for X must be a NEW allocation (proves the
	//            recycle map was pruned; if X were still in the map, the same ptr
	//            would be reused).
	// =========================================================================
	TEST_METHOD(StationOutput_Prune_DroppedIdAbsentAndMapCleared)
	{
		using namespace ArcStationSurfaceVMHelpers;

		FArcItemSpec SpecX = MakeSpec(3);
		FArcItemSpec SpecY = MakeSpec(6);
		const FArcItemId IdX = SpecX.ItemId;
		const FArcItemId IdY = SpecY.ItemId;

		// Rebuild 1: both X and Y.
		{
			TArray<FArcItemSpec> Specs;
			Specs.Add(SpecX);
			Specs.Add(SpecY);
			CraftVM->RebuildStationOutput(Specs, nullptr);
		}
		UArcItemViewModel* VMX_Rebuild1 = FindVMById(CraftVM->GetStationOutput(), IdX);
		ASSERT_THAT(IsNotNull(VMX_Rebuild1));

		// Rebuild 2: drop X.
		{
			TArray<FArcItemSpec> SpecsNoX;
			SpecsNoX.Add(SpecY);
			CraftVM->RebuildStationOutput(SpecsNoX, nullptr);
		}
		ASSERT_THAT(AreEqual(1, CraftVM->GetStationOutput().Num(),
			TEXT("After dropping X, Num must be 1")));
		ASSERT_THAT(IsFalse(ArrayContainsId(CraftVM->GetStationOutput(), IdX),
			TEXT("X must be absent from the output array after being dropped")));

		// Rebuild 3: add X back.
		{
			TArray<FArcItemSpec> SpecsXBack;
			SpecsXBack.Add(SpecX);
			SpecsXBack.Add(SpecY);
			CraftVM->RebuildStationOutput(SpecsXBack, nullptr);
		}
		UArcItemViewModel* VMX_Rebuild3 = FindVMById(CraftVM->GetStationOutput(), IdX);
		ASSERT_THAT(IsNotNull(VMX_Rebuild3, TEXT("VM for X must be present in rebuild 3")));

		// X must be a NEW VM (the recycle map had X pruned in rebuild 2).
		ASSERT_THAT(IsTrue(VMX_Rebuild1 != VMX_Rebuild3,
			TEXT("VM for X after re-add must be a NEW object — not the pruned one")));
	}

	// =========================================================================
	// Test 4 — StationInput mirrors the same populate + recycle + prune behavior
	//
	// Verifies SetServerInput (server rows of the StationInput view) with the same
	// mechanic in a single combined case.
	// =========================================================================
	TEST_METHOD(StationInput_PopulateRecyclePrune)
	{
		using namespace ArcStationSurfaceVMHelpers;

		FArcItemSpec SpecA = MakeSpec(4);
		FArcItemSpec SpecB = MakeSpec(8);
		const FArcItemId IdA = SpecA.ItemId;
		const FArcItemId IdB = SpecB.ItemId;

		// --- Populate ---
		{
			TArray<FArcItemSpec> Specs;
			Specs.Add(SpecA);
			Specs.Add(SpecB);
			CraftVM->SetServerInput(Specs, nullptr);
		}
		const TArray<TObjectPtr<UArcIngredientCandidateViewModel>>& Input = CraftVM->GetStationInput();
		ASSERT_THAT(AreEqual(2, Input.Num(), TEXT("StationInput must have 2 VMs after populate")));
		ASSERT_THAT(IsTrue(ArrayContainsId(Input, IdA), TEXT("VM for A must be present")));
		ASSERT_THAT(IsTrue(ArrayContainsId(Input, IdB), TEXT("VM for B must be present")));

		// Verify stacks.
		UArcIngredientCandidateViewModel* VMA = FindVMById(Input, IdA);
		ASSERT_THAT(IsNotNull(VMA));
		ASSERT_THAT(AreEqual(4, VMA->GetStacks(), TEXT("VM for A must have Stacks == 4")));

		// --- Recycle ---
		UArcIngredientCandidateViewModel* VMA_First = VMA;
		FArcItemSpec SpecAUpdated = MakeSpecWithId(IdA, 11);
		{
			TArray<FArcItemSpec> Specs2;
			Specs2.Add(SpecAUpdated);
			Specs2.Add(SpecB);
			CraftVM->SetServerInput(Specs2, nullptr);
		}
		UArcIngredientCandidateViewModel* VMA_Recycled = FindVMById(CraftVM->GetStationInput(), IdA);
		ASSERT_THAT(IsNotNull(VMA_Recycled));
		ASSERT_THAT(IsTrue(VMA_First == VMA_Recycled,
			TEXT("VM for A must be recycled (same pointer) on second rebuild")));
		ASSERT_THAT(AreEqual(11, VMA_Recycled->GetStacks(),
			TEXT("Recycled VM for A must carry updated Stacks == 11")));

		// --- Prune ---
		{
			TArray<FArcItemSpec> SpecsBOnly;
			SpecsBOnly.Add(SpecB);
			CraftVM->SetServerInput(SpecsBOnly, nullptr);
		}
		ASSERT_THAT(AreEqual(1, CraftVM->GetStationInput().Num(),
			TEXT("After pruning A, StationInput must have 1 VM")));
		ASSERT_THAT(IsFalse(ArrayContainsId(CraftVM->GetStationInput(), IdA),
			TEXT("A must be absent from StationInput after pruning")));

		// Rebuild with A again → new VM (A was removed from the recycle map).
		{
			TArray<FArcItemSpec> SpecsAAndB;
			SpecsAAndB.Add(SpecA);
			SpecsAAndB.Add(SpecB);
			CraftVM->SetServerInput(SpecsAAndB, nullptr);
		}
		UArcIngredientCandidateViewModel* VMA_ReAdded = FindVMById(CraftVM->GetStationInput(), IdA);
		ASSERT_THAT(IsNotNull(VMA_ReAdded));
		ASSERT_THAT(IsTrue(VMA_First != VMA_ReAdded,
			TEXT("Re-added A must be a NEW VM — the old one was pruned from the recycle map")));
	}

	TEST_METHOD(StationInput_PreviewAndServerRowWithSameItemId_AreSeparateAndRecycled)
	{
		using namespace ArcStationSurfaceVMHelpers;

		const FArcItemSpec Inventory = MakeSpec(5);
		const FArcItemId SharedId = Inventory.ItemId;

		FArcCraftStoreItemRow InventoryRow;
		InventoryRow.OwnerEntity = FMassEntityHandle(7, 1);
		InventoryRow.StoreType = FArcMassItemStoreFragment::StaticStruct();
		InventoryRow.Spec = Inventory;
		InventoryRow.Stacks = 5;
		TArray<FArcCraftStoreItemRow> InventoryRows;
		InventoryRows.Add(InventoryRow);
		CraftVM->SetInventoryRows(MoveTemp(InventoryRows));
		CraftVM->SetbSupportsManualDeposit(true);

		UArcItemViewModel* InventoryItem = NewObject<UArcItemViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
		InventoryItem->RefreshFrom(Inventory, 5, nullptr);
		ASSERT_THAT(IsTrue(CraftVM->TransferItemToStation(InventoryItem, 2)));

		// The server input keeps the inventory ItemId, so a part already deposited shares it with the preview row.
		{
			TArray<FArcItemSpec> Server;
			Server.Add(MakeSpecWithId(SharedId, 1));
			CraftVM->SetServerInput(Server, nullptr);
		}

		const TArray<TObjectPtr<UArcIngredientCandidateViewModel>>& Input = CraftVM->GetStationInput();
		ASSERT_THAT(AreEqual(2, Input.Num(), TEXT("A server row and a preview row with one ItemId are two rows")));
		UArcIngredientCandidateViewModel* ServerRow = nullptr;
		UArcIngredientCandidateViewModel* PreviewRow = nullptr;
		for (const TObjectPtr<UArcIngredientCandidateViewModel>& Row : Input)
		{
			ASSERT_THAT(IsNotNull(Row.Get()));
			ASSERT_THAT(IsTrue(Row->GetItemId() == SharedId));
			if (Row->GetbPreview())
			{
				PreviewRow = Row.Get();
			}
			else
			{
				ServerRow = Row.Get();
			}
		}
		ASSERT_THAT(IsNotNull(ServerRow, TEXT("One row is the server row")));
		ASSERT_THAT(IsNotNull(PreviewRow, TEXT("One row is the preview row")));
		ASSERT_THAT(IsTrue(ServerRow != PreviewRow, TEXT("The two rows are separate view models")));
		ASSERT_THAT(AreEqual(1, ServerRow->GetStacks()));
		ASSERT_THAT(AreEqual(2, PreviewRow->GetStacks()));
		ASSERT_THAT(IsTrue(PreviewRow->GetOwnerEntity() == FMassEntityHandle(7, 1), TEXT("The preview row carries its owner")));

		{
			TArray<FArcItemSpec> Server;
			Server.Add(MakeSpecWithId(SharedId, 3));
			CraftVM->SetServerInput(Server, nullptr);
		}

		ASSERT_THAT(AreEqual(2, CraftVM->GetStationInput().Num()));
		UArcIngredientCandidateViewModel* ServerRowAfter = nullptr;
		UArcIngredientCandidateViewModel* PreviewRowAfter = nullptr;
		for (const TObjectPtr<UArcIngredientCandidateViewModel>& Row : CraftVM->GetStationInput())
		{
			if (Row->GetbPreview())
			{
				PreviewRowAfter = Row.Get();
			}
			else
			{
				ServerRowAfter = Row.Get();
			}
		}
		ASSERT_THAT(IsTrue(ServerRowAfter == ServerRow, TEXT("The server row recycles its own view model")));
		ASSERT_THAT(IsTrue(PreviewRowAfter == PreviewRow, TEXT("The preview row recycles its own view model")));
		ASSERT_THAT(AreEqual(3, ServerRowAfter->GetStacks()));
		ASSERT_THAT(AreEqual(2, PreviewRowAfter->GetStacks(), TEXT("The preview row is not overwritten by the server row")));
	}
};
