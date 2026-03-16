// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcQuickBarTestHelpers.h"

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

using namespace ArcQuickBarTestHelpers;

// ---------------------------------------------------------------------------
// Query & lookup methods:
//   FindQuickSlotForItem, FindQuickSlotItem, IsItemOnQuickSlot,
//   IsQuickSlotActive, GetActiveSlots, GetFirstActiveSlot, GetItemId,
//   IsItemOnAnyQuickSlot, GetItemStoreComponent
//
// Also covers locking helpers:
//   LockQuickSlots, UnlockQuickSlots, IsQuickSlotLocked,
//   SetLockQuickBar, SetUnlockQuickBar, IsQuickBarLocked
// ---------------------------------------------------------------------------
TEST_CLASS(ArcQuickBar_Queries, "ArcCore.QuickBar.Query")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	// ----- Item lookups -----------------------------------------------------

	TEST_METHOD(FindQuickSlotForItem_ReturnsCorrectSlot)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01), MakeSlot(TAG_QBTest_Slot02) });

		FArcItemId Item1 = AddTestItem(Actor.ItemsStore);
		FArcItemId Item2 = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, Item1);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot02, Item2);

		FGameplayTag FoundSlot = QB->FindQuickSlotForItem(Item2);

		ASSERT_THAT(IsTrue(FoundSlot == TAG_QBTest_Slot02,
			TEXT("Should find Slot02 for Item2")));
	}

	TEST_METHOD(FindQuickSlotForItem_InvalidItem_ReturnsEmptyTag)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FGameplayTag FoundSlot = QB->FindQuickSlotForItem(FArcItemId::InvalidId);

		ASSERT_THAT(IsFalse(FoundSlot.IsValid(),
			TEXT("Should return empty tag for invalid item")));
	}

	TEST_METHOD(FindQuickSlotItem_ReturnsItemData)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		const FArcItemData* Data = QB->FindQuickSlotItem(TAG_QBTest_Bar01, TAG_QBTest_Slot01);

		ASSERT_THAT(IsNotNull(Data, TEXT("FindQuickSlotItem should return valid data")));
	}

	TEST_METHOD(IsItemOnAnyQuickSlot_TrueWhenSlotted)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		ASSERT_THAT(IsTrue(QB->IsItemOnAnyQuickSlot(ItemId),
			TEXT("Item should be on some quick slot")));
	}

	TEST_METHOD(IsItemOnAnyQuickSlot_FalseAfterRemoval)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);
		QB->RemoveQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01);

		ASSERT_THAT(IsFalse(QB->IsItemOnAnyQuickSlot(ItemId),
			TEXT("Item should not be on any slot after removal")));
	}

	// ----- Active slot queries ----------------------------------------------

	TEST_METHOD(GetActiveSlots_ReturnsAllActive)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01), MakeSlot(TAG_QBTest_Slot02), MakeSlot(TAG_QBTest_Slot03) });

		FArcItemId Item1 = AddTestItem(Actor.ItemsStore);
		FArcItemId Item2 = AddTestItem(Actor.ItemsStore);
		FArcItemId Item3 = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, Item1);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot02, Item2);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot03, Item3);

		TArray<FGameplayTag> ActiveSlots = QB->GetActiveSlots(TAG_QBTest_Bar01);

		ASSERT_THAT(AreEqual(3, ActiveSlots.Num(), TEXT("All three slots should be active")));
	}

	TEST_METHOD(GetActiveSlots_ReturnsEmptyWhenNoneActive)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, false) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		TArray<FGameplayTag> ActiveSlots = QB->GetActiveSlots(TAG_QBTest_Bar01);

		ASSERT_THAT(AreEqual(0, ActiveSlots.Num(), TEXT("No slots should be active")));
	}

	TEST_METHOD(GetFirstActiveSlot_ReturnsFirstActive)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01), MakeSlot(TAG_QBTest_Slot02) });

		FArcItemId Item1 = AddTestItem(Actor.ItemsStore);
		FArcItemId Item2 = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, Item1);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot02, Item2);

		FGameplayTag FirstActive = QB->GetFirstActiveSlot(TAG_QBTest_Bar01);

		ASSERT_THAT(IsTrue(FirstActive.IsValid(), TEXT("Should return a valid active slot")));
	}

	TEST_METHOD(GetFirstActiveSlot_NoActive_ReturnsEmptyTag)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01, false) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		FGameplayTag FirstActive = QB->GetFirstActiveSlot(TAG_QBTest_Bar01);

		ASSERT_THAT(IsFalse(FirstActive.IsValid(),
			TEXT("Should return empty tag when nothing active")));
	}

	// ----- GetItemId --------------------------------------------------------

	TEST_METHOD(GetItemId_ReturnsCorrectId)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FArcItemId ItemId = AddTestItem(Actor.ItemsStore);
		QB->AddAndActivateQuickSlot(TAG_QBTest_Bar01, TAG_QBTest_Slot01, ItemId);

		FArcItemId Retrieved = QB->GetItemId(TAG_QBTest_Bar01, TAG_QBTest_Slot01);

		ASSERT_THAT(IsTrue(Retrieved == ItemId,
			TEXT("GetItemId should return the same item that was added")));
	}

	TEST_METHOD(GetItemId_EmptySlot_ReturnsInvalid)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		FArcItemId Retrieved = QB->GetItemId(TAG_QBTest_Bar01, TAG_QBTest_Slot01);

		ASSERT_THAT(IsFalse(Retrieved.IsValid(),
			TEXT("GetItemId should return invalid for empty slot")));
	}

	// ----- GetItemStoreComponent --------------------------------------------

	TEST_METHOD(GetItemStoreComponent_FindsCorrectStore)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::AutoActivateOnly,
			{ MakeSlot(TAG_QBTest_Slot01) });

		UArcItemsStoreComponent* Store = QB->GetItemStoreComponent(TAG_QBTest_Bar01);

		ASSERT_THAT(IsNotNull(Store, TEXT("Should find the ItemsStore component")));
		ASSERT_THAT(IsTrue(Store == Actor.ItemsStore,
			TEXT("Should resolve to the actor's ItemsStore")));
	}

	// ----- Locking ----------------------------------------------------------

	TEST_METHOD(LockQuickSlots_IsQuickSlotLocked)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01) });

		ASSERT_THAT(IsFalse(QB->IsQuickSlotLocked(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should not be locked initially")));

		QB->LockQuickSlots(TAG_QBTest_Bar01, TAG_QBTest_Slot01);

		ASSERT_THAT(IsTrue(QB->IsQuickSlotLocked(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should be locked")));
	}

	TEST_METHOD(UnlockQuickSlots_RemovesLock)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ConfigureSingleBar(QB, TAG_QBTest_Bar01, EArcQuickSlotsMode::Cyclable,
			{ MakeSlot(TAG_QBTest_Slot01) });

		QB->LockQuickSlots(TAG_QBTest_Bar01, TAG_QBTest_Slot01);
		QB->UnlockQuickSlots(TAG_QBTest_Bar01, TAG_QBTest_Slot01);

		ASSERT_THAT(IsFalse(QB->IsQuickSlotLocked(TAG_QBTest_Bar01, TAG_QBTest_Slot01),
			TEXT("Slot should be unlocked")));
	}

	TEST_METHOD(SetLockQuickBar_IsQuickBarLocked)
	{
		AArcQuickBarTestActor& Actor = Spawner.SpawnActor<AArcQuickBarTestActor>();
		UArcQuickBarTestComponent* QB = Actor.QuickBarComponent;

		ASSERT_THAT(IsFalse(QB->IsQuickBarLocked(), TEXT("QuickBar should not be locked initially")));

		QB->SetLockQuickBar();
		ASSERT_THAT(IsTrue(QB->IsQuickBarLocked(), TEXT("QuickBar should be locked")));

		QB->SetUnlockQuickBar();
		ASSERT_THAT(IsFalse(QB->IsQuickBarLocked(), TEXT("QuickBar should be unlocked")));
	}
};
