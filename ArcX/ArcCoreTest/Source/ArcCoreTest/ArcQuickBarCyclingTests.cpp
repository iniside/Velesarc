// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcQuickBarTestHelpers.h"

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

using namespace ArcQuickBarTestHelpers;

// ---------------------------------------------------------------------------
// Cycling & deselection:
//   DeselectCurrentQuickSlot  (does not require PlayerController)
//
// NOTE: CycleSlotForward / CycleSlotBackward require the owning actor to be
// a PlayerController (or PlayerState with a linked PC) because InternalCanCycle
// checks IsLocalController(). Those are skipped here and should be covered by
// PIE / integration tests with a full player login flow.
// ---------------------------------------------------------------------------
TEST_CLASS(ArcQuickBar_Cycling, "ArcCore.QuickBar.Cycling")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	// ----- DeselectCurrentQuickSlot -----------------------------------------

	TEST_METHOD(DeselectCurrentQuickSlot_DeactivatesActiveSlot)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, /*bAutoSelect=*/true) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Precondition: slot should be active")));

		QB->DeselectCurrentQuickSlot(TAG_QBTest_Bar01);

		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should be deactivated after DeselectCurrentQuickSlot")));
	}

	TEST_METHOD(DeselectCurrentQuickSlot_ItemRemainsOnSlot)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, true) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		QB->DeselectCurrentQuickSlot(TAG_QBTest_Bar01);

		// Item stays assigned even though slot is deactivated
		ASSERT_THAT(IsTrue(QB->IsItemOnQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Item should still be on slot after deselection")));
	}

	TEST_METHOD(DeselectCurrentQuickSlot_NoActiveSlot_DoesNothing)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, false) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		// No slot is active (Cyclable + no autoSelect)
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));

		// Should be a no-op — nothing to deselect
		QB->DeselectCurrentQuickSlot(TAG_QBTest_Bar01);

		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should remain inactive")));
		ASSERT_THAT(IsTrue(QB->IsItemOnQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Item should remain on slot")));
	}

	TEST_METHOD(DeselectCurrentQuickSlot_MultipleSlots_OnlyActiveDeselected)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		// Cyclable bar: slot01 auto-selects, slot02 does not
		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, true), MakeSlot(TAG_QBTest_Slot02, false) });

		FArcItemId Item1 = AddTestItem(Actor.ItemsStore);
		FArcItemId Item2 = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, Item1);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot02, Item2);

		// Only slot01 should be active
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot02)));

		QB->DeselectCurrentQuickSlot(TAG_QBTest_Bar01);

		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Active slot should be deselected")));
		// Slot02 was never active; both items still assigned
		ASSERT_THAT(IsTrue(QB->IsItemOnQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));
		ASSERT_THAT(IsTrue(QB->IsItemOnQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot02)));
	}
};
