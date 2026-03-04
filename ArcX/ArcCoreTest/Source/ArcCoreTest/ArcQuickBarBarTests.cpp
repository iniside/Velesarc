// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcQuickBarTestHelpers.h"

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

using namespace ArcQuickBarTestHelpers;

// ---------------------------------------------------------------------------
// Bar-level activation / deactivation:
//   ActivateQuickBar, DeactivateQuickBar, ActivateBar, DeactivateBar
// ---------------------------------------------------------------------------
TEST_CLASS(ArcQuickBar_BarActivation, "ArcCore.QuickBar.Bar")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	// ----- ActivateQuickBar / DeactivateQuickBar ----------------------------
	// These are the simpler versions that don't check AreAllBarSlotsValid.

	TEST_METHOD(ActivateQuickBar_ActivatesAllSlots)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		// Use Cyclable with bAutoSelect=false so AddAndActivate does NOT activate
		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, false), MakeSlot(TAG_QBTest_Slot02, false) });

		FArcItemId Item1 = AddTestItem(Actor.ItemsStore);
		FArcItemId Item2 = AddTestItem(Actor.ItemsStore);

		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, Item1);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot02, Item2);

		// Precondition: nothing active
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot02)));

		QB->ActivateQuickBar(TAG_QBTest_Bar01);

		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot01 should be active")));
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot02),
			TEXT("Slot02 should be active")));
	}

	TEST_METHOD(DeactivateQuickBar_DeactivatesAllSlots)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01), MakeSlot(TAG_QBTest_Slot02) });

		FArcItemId Item1 = AddTestItem(Actor.ItemsStore);
		FArcItemId Item2 = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, Item1);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot02, Item2);

		// Precondition: both active
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot02)));

		QB->DeactivateQuickBar(TAG_QBTest_Bar01);

		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot01 should be inactive")));
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot02),
			TEXT("Slot02 should be inactive")));
	}

	TEST_METHOD(DeactivateQuickBar_ItemsStillOnSlots)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);
		QB->DeactivateQuickBar(TAG_QBTest_Bar01);

		// Item should still be assigned to the slot
		ASSERT_THAT(IsTrue(QB->IsItemOnQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Item should still be on slot after DeactivateQuickBar")));
		// But slot is not active
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));
	}

	// ----- ActivateBar / DeactivateBar --------------------------------------
	// These are the full versions with validation and QuickBarSelectedActions.
	// In standalone, GetNetMode() == NM_Standalone and GetOwnerRole() == ROLE_Authority.

	TEST_METHOD(ActivateBar_AllSlotsHaveItems_Activates)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		// ManualActivationOnly + no autoSelect so AddAndActivate does not activate
		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::ManualActivationOnly,
			{ MakeSlot(TAG_QBTest_Slot01, false), MakeSlot(TAG_QBTest_Slot02, false) });

		FArcItemId Item1 = AddTestItem(Actor.ItemsStore);
		FArcItemId Item2 = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, Item1);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot02, Item2);

		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot02)));

		QB->ActivateBar(TAG_QBTest_Bar01);

		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot01 should be active after ActivateBar")));
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot02),
			TEXT("Slot02 should be active after ActivateBar")));
	}

	TEST_METHOD(ActivateBar_SlotMissingItem_DoesNotActivate)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		// Two slots: only slot01 gets an item, slot02 stays empty
		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::ManualActivationOnly,
			{ MakeSlot(TAG_QBTest_Slot01, false), MakeSlot(TAG_QBTest_Slot02, false) });

		FArcItemId Item1 = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, Item1);
		// Slot02 intentionally left empty

		QB->ActivateBar(TAG_QBTest_Bar01);

		// ActivateBar should fail because slot02 has no item (AreAllBarSlotsValid == false)
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot01 should NOT be active when bar validation fails")));
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot02),
			TEXT("Slot02 should NOT be active")));
	}

	TEST_METHOD(DeactivateBar_DeactivatesAllSlots)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::ManualActivationOnly,
			{ MakeSlot(TAG_QBTest_Slot01, false), MakeSlot(TAG_QBTest_Slot02, false) });

		FArcItemId Item1 = AddTestItem(Actor.ItemsStore);
		FArcItemId Item2 = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, Item1);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot02, Item2);

		// Activate everything first
		QB->ActivateBar(TAG_QBTest_Bar01);
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot02)));

		QB->DeactivateBar(TAG_QBTest_Bar01);

		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot01 should be inactive after DeactivateBar")));
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot02),
			TEXT("Slot02 should be inactive after DeactivateBar")));
	}

	TEST_METHOD(ActivateBar_ThenDeactivateBar_Roundtrip)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::ManualActivationOnly,
			{ MakeSlot(TAG_QBTest_Slot01, false) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		// Activate
		QB->ActivateBar(TAG_QBTest_Bar01);
		ASSERT_THAT(IsTrue(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));

		// Deactivate
		QB->DeactivateBar(TAG_QBTest_Bar01);
		ASSERT_THAT(IsFalse(QB->IsQuickSlotActive(TAG_QBTest_Bar01, TAG_QBTest_Slot01)));

		// Item still on slot
		ASSERT_THAT(IsTrue(QB->IsItemOnQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Item should remain on slot through activate/deactivate cycle")));
	}
};
