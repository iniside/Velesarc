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

#include "INotifyFieldValueChanged.h"

#include "ViewModels/QuickBar/ArcQuickBarSlotViewModel.h"

// ---------------------------------------------------------------------------
// FieldNotify broadcast regression tests for UArcQuickBarSlotViewModel.
//
// SetHasItem / SetActive route through UE_MVVM_SET_PROPERTY_VALUE, which
// broadcasts the FieldNotify on a value change AND early-outs (no broadcast)
// when the value is unchanged. These assert both halves of that contract on
// the *broadcast*, not merely the getter state.
// ---------------------------------------------------------------------------

TEST_CLASS(ArcQuickBarSlotViewModelTests, "ArcUIModels.QuickBarSlotVM")
{
	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
	}

	// =========================================================================
	// Test 1 — SetHasItem(true) broadcasts bHasItem once; a second SetHasItem
	//          with the same value must NOT re-fire (equality early-out).
	// =========================================================================
	TEST_METHOD(SetHasItem_BroadcastsOnChange_NoRefireOnSameValue)
	{
		UArcQuickBarSlotViewModel* Slot = NewObject<UArcQuickBarSlotViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		int32 FireCount = 0;
		Slot->AddFieldValueChangedDelegate(
			UArcQuickBarSlotViewModel::FFieldNotificationClassDescriptor::bHasItem,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
				[&FireCount](UObject*, UE::FieldNotification::FFieldId) { ++FireCount; }));

		// false -> true: broadcasts exactly once.
		Slot->SetHasItem(true);
		ASSERT_THAT(AreEqual(1, FireCount,
			TEXT("bHasItem must broadcast once on the false->true change")));
		ASSERT_THAT(IsTrue(Slot->GetbHasItem(),
			TEXT("bHasItem getter must read true after SetHasItem(true)")));

		// true -> true: equality early-out, no re-fire.
		Slot->SetHasItem(true);
		ASSERT_THAT(AreEqual(1, FireCount,
			TEXT("bHasItem must NOT re-fire when set to the same value")));
	}

	// =========================================================================
	// Test 2 — SetActive(true) broadcasts bIsActive once; a second SetActive
	//          with the same value must NOT re-fire (equality early-out).
	// =========================================================================
	TEST_METHOD(SetActive_BroadcastsOnChange_NoRefireOnSameValue)
	{
		UArcQuickBarSlotViewModel* Slot = NewObject<UArcQuickBarSlotViewModel>(
			GetTransientPackage(), NAME_None, RF_Transient);

		int32 FireCount = 0;
		Slot->AddFieldValueChangedDelegate(
			UArcQuickBarSlotViewModel::FFieldNotificationClassDescriptor::bIsActive,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
				[&FireCount](UObject*, UE::FieldNotification::FFieldId) { ++FireCount; }));

		// false -> true: broadcasts exactly once.
		Slot->SetActive(true);
		ASSERT_THAT(AreEqual(1, FireCount,
			TEXT("bIsActive must broadcast once on the false->true change")));
		ASSERT_THAT(IsTrue(Slot->GetbIsActive(),
			TEXT("bIsActive getter must read true after SetActive(true)")));

		// true -> true: equality early-out, no re-fire.
		Slot->SetActive(true);
		ASSERT_THAT(AreEqual(1, FireCount,
			TEXT("bIsActive must NOT re-fire when set to the same value")));
	}
};
