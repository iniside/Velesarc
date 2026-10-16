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

#include "NativeGameplayTags.h"

#include "ArcFrontendMVVMModelComponent.h"
#include "ViewModels/QuickBar/ArcQuickBarSlotResolver.h"
#include "ViewModels/QuickBar/ArcQuickBarViewModel.h"
#include "ViewModels/QuickBar/ArcQuickBarSlotViewModel.h"
#include "Models/QuickBar/ArcQuickBarStructModel.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarSharedFragment.h"
#include "QuickBar/ArcMassQuickBarTypes.h"

// ---------------------------------------------------------------------------
// Coverage for the two exported statics the (BarId, QuickSlotId) resolver
// design depends on:
//   - UArcQuickBarSlotResolver::ResolveSlotViewModel — composes
//     Component.FindViewModel(BarId) -> Cast<UArcQuickBarViewModel> ->
//     FindSlot(QuickSlotId).
//   - FArcQuickBarStructModel::FindBarEntry — locates a bar by BarId across
//     both UArcMassQuickBarConfig::QuickBars and Groups[].Bars.
//
// The CreateInstance widget/PlayerState path is out of scope here (needs
// owning-player plumbing, PIE territory) — see the plan's Step 4 notes.
// All fixtures are plain NewObject, no world/PIE/Mass.
// ---------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBRTest_BarA, "Test.QuickBarResolver.BarA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBRTest_BarB, "Test.QuickBarResolver.BarB");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBRTest_UnknownBar, "Test.QuickBarResolver.UnknownBar");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBRTest_Slot, "Test.QuickBarResolver.Slot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBRTest_UnknownSlot, "Test.QuickBarResolver.UnknownSlot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBRTest_SharedSlot, "Test.QuickBarResolver.SharedSlot");

TEST_CLASS(ArcQuickBarSlotResolverTests, "ArcUIModels.QuickBarSlotResolver")
{
	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
	}

	// =========================================================================
	// Test 1 — a registered bar VM with a matching slot resolves to that exact
	//          slot VM instance.
	// =========================================================================
	TEST_METHOD(ResolveSlotViewModel_RegisteredBarAndSlot_ReturnsExactSlotVM)
	{
		UArcFrontendMVVMModelComponent* Component = NewObject<UArcFrontendMVVMModelComponent>(
			GetTransientPackage(), NAME_None, RF_Transient);

		UArcQuickBarViewModel* BarVM = NewObject<UArcQuickBarViewModel>(Component, NAME_None, RF_Transient);
		BarVM->SetBarId(TAG_QBRTest_BarA);

		UArcQuickBarSlotViewModel* SlotVM = NewObject<UArcQuickBarSlotViewModel>(Component, NAME_None, RF_Transient);
		SlotVM->SetQuickSlotId(TAG_QBRTest_Slot);
		BarVM->Slots.Add(SlotVM);

		Component->AddViewModel(TAG_QBRTest_BarA, BarVM);

		UArcQuickBarSlotViewModel* Result = UArcQuickBarSlotResolver::ResolveSlotViewModel(
			*Component, TAG_QBRTest_BarA, TAG_QBRTest_Slot);

		ASSERT_THAT(IsNotNull(Result, TEXT("A registered bar with a matching slot must resolve")));
		ASSERT_THAT(AreEqual(SlotVM, Result, TEXT("The resolved slot VM must be the exact instance registered on the bar")));
	}

	// =========================================================================
	// Test 2 — an unregistered BarId resolves to nullptr.
	// =========================================================================
	TEST_METHOD(ResolveSlotViewModel_UnknownBarId_ReturnsNull)
	{
		UArcFrontendMVVMModelComponent* Component = NewObject<UArcFrontendMVVMModelComponent>(
			GetTransientPackage(), NAME_None, RF_Transient);

		UArcQuickBarViewModel* BarVM = NewObject<UArcQuickBarViewModel>(Component, NAME_None, RF_Transient);
		BarVM->SetBarId(TAG_QBRTest_BarA);

		UArcQuickBarSlotViewModel* SlotVM = NewObject<UArcQuickBarSlotViewModel>(Component, NAME_None, RF_Transient);
		SlotVM->SetQuickSlotId(TAG_QBRTest_Slot);
		BarVM->Slots.Add(SlotVM);

		Component->AddViewModel(TAG_QBRTest_BarA, BarVM);

		UArcQuickBarSlotViewModel* Result = UArcQuickBarSlotResolver::ResolveSlotViewModel(
			*Component, TAG_QBRTest_UnknownBar, TAG_QBRTest_Slot);

		ASSERT_THAT(IsNull(Result, TEXT("An unregistered BarId must resolve to nullptr")));
	}

	// =========================================================================
	// Test 3 — a known BarId but an unknown QuickSlotId resolves to nullptr.
	// =========================================================================
	TEST_METHOD(ResolveSlotViewModel_UnknownQuickSlotId_ReturnsNull)
	{
		UArcFrontendMVVMModelComponent* Component = NewObject<UArcFrontendMVVMModelComponent>(
			GetTransientPackage(), NAME_None, RF_Transient);

		UArcQuickBarViewModel* BarVM = NewObject<UArcQuickBarViewModel>(Component, NAME_None, RF_Transient);
		BarVM->SetBarId(TAG_QBRTest_BarA);

		UArcQuickBarSlotViewModel* SlotVM = NewObject<UArcQuickBarSlotViewModel>(Component, NAME_None, RF_Transient);
		SlotVM->SetQuickSlotId(TAG_QBRTest_Slot);
		BarVM->Slots.Add(SlotVM);

		Component->AddViewModel(TAG_QBRTest_BarA, BarVM);

		UArcQuickBarSlotViewModel* Result = UArcQuickBarSlotResolver::ResolveSlotViewModel(
			*Component, TAG_QBRTest_BarA, TAG_QBRTest_UnknownSlot);

		ASSERT_THAT(IsNull(Result, TEXT("A known bar with no matching slot must resolve to nullptr")));
	}

	// =========================================================================
	// Test 4 — the BarId tag is registered to a VM that is NOT a
	//          UArcQuickBarViewModel; the wrong-type guard must reject it.
	// =========================================================================
	TEST_METHOD(ResolveSlotViewModel_TagRegisteredToWrongVMType_ReturnsNull)
	{
		UArcFrontendMVVMModelComponent* Component = NewObject<UArcFrontendMVVMModelComponent>(
			GetTransientPackage(), NAME_None, RF_Transient);

		// Any non-UArcQuickBarViewModel UMVVMViewModelBase subclass works here.
		UArcQuickBarSlotViewModel* WrongTypeVM = NewObject<UArcQuickBarSlotViewModel>(
			Component, NAME_None, RF_Transient);

		Component->AddViewModel(TAG_QBRTest_BarA, WrongTypeVM);

		UArcQuickBarSlotViewModel* Result = UArcQuickBarSlotResolver::ResolveSlotViewModel(
			*Component, TAG_QBRTest_BarA, TAG_QBRTest_Slot);

		ASSERT_THAT(IsNull(Result, TEXT("A tag registered to a non-quick-bar VM must resolve to nullptr")));
	}

	// =========================================================================
	// Test 5 — two bars each contain a slot with the SAME QuickSlotId;
	//          resolving with each BarId must return that bar's own slot VM
	//          (the compound-key disambiguation this design exists for).
	// =========================================================================
	TEST_METHOD(ResolveSlotViewModel_SameQuickSlotIdAcrossTwoBars_DisambiguatesByBarId)
	{
		UArcFrontendMVVMModelComponent* Component = NewObject<UArcFrontendMVVMModelComponent>(
			GetTransientPackage(), NAME_None, RF_Transient);

		UArcQuickBarViewModel* BarAVM = NewObject<UArcQuickBarViewModel>(Component, NAME_None, RF_Transient);
		BarAVM->SetBarId(TAG_QBRTest_BarA);
		UArcQuickBarSlotViewModel* SlotAVM = NewObject<UArcQuickBarSlotViewModel>(Component, NAME_None, RF_Transient);
		SlotAVM->SetQuickSlotId(TAG_QBRTest_SharedSlot);
		BarAVM->Slots.Add(SlotAVM);
		Component->AddViewModel(TAG_QBRTest_BarA, BarAVM);

		UArcQuickBarViewModel* BarBVM = NewObject<UArcQuickBarViewModel>(Component, NAME_None, RF_Transient);
		BarBVM->SetBarId(TAG_QBRTest_BarB);
		UArcQuickBarSlotViewModel* SlotBVM = NewObject<UArcQuickBarSlotViewModel>(Component, NAME_None, RF_Transient);
		SlotBVM->SetQuickSlotId(TAG_QBRTest_SharedSlot);
		BarBVM->Slots.Add(SlotBVM);
		Component->AddViewModel(TAG_QBRTest_BarB, BarBVM);

		UArcQuickBarSlotViewModel* ResultA = UArcQuickBarSlotResolver::ResolveSlotViewModel(
			*Component, TAG_QBRTest_BarA, TAG_QBRTest_SharedSlot);
		UArcQuickBarSlotViewModel* ResultB = UArcQuickBarSlotResolver::ResolveSlotViewModel(
			*Component, TAG_QBRTest_BarB, TAG_QBRTest_SharedSlot);

		ASSERT_THAT(AreEqual(SlotAVM, ResultA, TEXT("BarA's QuickSlotId lookup must return BarA's own slot VM")));
		ASSERT_THAT(AreEqual(SlotBVM, ResultB, TEXT("BarB's QuickSlotId lookup must return BarB's own slot VM")));
		ASSERT_THAT(IsTrue(ResultA != ResultB, TEXT("The two bars' same-tagged slots must resolve to distinct VM instances")));
	}

	// =========================================================================
	// Test 6 — FArcQuickBarStructModel::FindBarEntry locates a bar by BarId
	//          across both the config's ungrouped QuickBars and Groups[].Bars.
	// =========================================================================
	TEST_METHOD(FindBarEntry_UngroupedAndGroupedBars_ResolveByBarId)
	{
		UArcMassQuickBarConfig* Config = NewObject<UArcMassQuickBarConfig>(
			GetTransientPackage(), NAME_None, RF_Transient);

		FArcMassQuickBarEntry& UngroupedBar = Config->QuickBars.AddDefaulted_GetRef();
		UngroupedBar.BarId = TAG_QBRTest_BarA;

		FArcMassQuickBarGroup& Group = Config->Groups.AddDefaulted_GetRef();
		FArcMassQuickBarEntry& GroupedBar = Group.Bars.AddDefaulted_GetRef();
		GroupedBar.BarId = TAG_QBRTest_BarB;

		Config->StoreType = FArcMassItemStoreFragment::StaticStruct();

		FArcMassQuickBarSharedFragment Shared;
		Shared.Configs.Add(Config);

		const FArcMassQuickBarEntry* FoundUngrouped = FArcQuickBarStructModel::FindBarEntry(Shared, TAG_QBRTest_BarA).Bar;
		const FArcMassQuickBarEntry* FoundGrouped = FArcQuickBarStructModel::FindBarEntry(Shared, TAG_QBRTest_BarB).Bar;
		const FArcMassQuickBarEntry* FoundUnknown = FArcQuickBarStructModel::FindBarEntry(Shared, TAG_QBRTest_UnknownBar).Bar;

		ASSERT_THAT(IsNotNull(FoundUngrouped, TEXT("FindBarEntry must resolve a bar from the ungrouped QuickBars array")));
		ASSERT_THAT(AreEqual(TAG_QBRTest_BarA.GetTag(), FoundUngrouped->BarId, TEXT("The resolved ungrouped entry's BarId must match")));

		ASSERT_THAT(IsNotNull(FoundGrouped, TEXT("FindBarEntry must resolve a bar nested inside Groups[].Bars")));
		ASSERT_THAT(AreEqual(TAG_QBRTest_BarB.GetTag(), FoundGrouped->BarId, TEXT("The resolved grouped entry's BarId must match")));

		ASSERT_THAT(IsNull(FoundUnknown, TEXT("An unknown BarId must resolve to nullptr")));
	}
};
