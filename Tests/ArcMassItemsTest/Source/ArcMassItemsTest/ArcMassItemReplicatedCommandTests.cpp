// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcStandalonePIEComponent.h"
#include "ArcMassItemsTestGameMode.h"
#include "Engine/Engine.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "NativeGameplayTags.h"

#include "Commands/ArcMassAddItemCommand.h"
#include "Commands/ArcMassAddItemToSlotCommand.h"
#include "Commands/ArcMassAddNewItemCommand.h"
#include "Commands/ArcMassAddNewItemToSlotCommand.h"
#include "Commands/ArcMassAssignItemSlotCommand.h"
#include "Commands/ArcMassAddItemToQuickSlotCommand.h"
#include "Commands/ArcMassAddNewItemToQuickSlotCommand.h"
#include "Commands/ArcMassDestroyItemCommand.h"
#include "Commands/ArcMassMoveItemCommand.h"
#include "Commands/ArcMassRemoveItemCommand.h"
#include "Commands/ArcMassRemoveItemFromSlotCommand.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Traits/ArcMassItemStoreTrait.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Operations/ArcMassItemOperations.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "QuickBar/ArcMassQuickBarTypes.h"

#include "ArcMassQuickBarTestHelpers.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CmdTest_Slot_Primary, "Test.Item.Slot.Primary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CmdTest_QuickBar, "Test.Item.QuickBar.Default");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CmdTest_QuickSlot_1, "Test.Item.QuickSlot.1");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CmdTest_QuickItemSlot, "Test.Item.QuickItemSlot.1");

#include "ArcMassItemTestHelpers.h"
#include "ArcMassItemTestsSettings.h"
#if 0
namespace
{
	FMassEntityHandle CreateStoreEntity(FMassEntityManager& EM)
	{
		// Build archetype with FArcMassItemStoreTag up front. UArcMassItemStoreInitObserver
		// reacts to the tag and adds the sparse FArcMassItemStoreFragment via a deferred
		// command. Manual AddSparseElementToEntity post-creation triggers archetype
		// recomposition and breaks project-level processor query caches.
		TArray<const UScriptStruct*> FragmentsAndTags;
		FragmentsAndTags.Add(FTransformFragment::StaticStruct());
		FragmentsAndTags.Add(FArcMassItemStoreTag::StaticStruct());
		const FMassArchetypeHandle Archetype = EM.CreateArchetype(FragmentsAndTags);
		FMassEntityHandle Entity = EM.CreateEntity(Archetype);
		// Observer's deferred commands aren't auto-flushed at CreateEntity; flush so the
		// sparse fragment is visible to the test body.
		EM.FlushCommands();
		return Entity;
	}

	bool StoreContains(FMassEntityManager& EM, FMassEntityHandle Entity, const FArcItemId& ItemId)
	{
		const FArcMassItemStoreFragment* Store = FMassEntityView(EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		if (Store == nullptr)
		{
			return false;
		}
		return const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId) != nullptr;
	}

	FGameplayTag GetItemSlot(FMassEntityManager& EM, FMassEntityHandle Entity, const FArcItemId& ItemId)
	{
		const FArcMassItemStoreFragment* Store = FMassEntityView(EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		if (Store == nullptr)
		{
			return FGameplayTag();
		}
		const FArcMassReplicatedItem* Wrapper = const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemId);
		const FArcItemData* Item = Wrapper ? Wrapper->ToItem() : nullptr;
		return Item ? Item->GetSlotId() : FGameplayTag();
	}

	bool QuickSlotHasItem(FMassEntityManager& EM, FMassEntityHandle Entity, FGameplayTag BarId, FGameplayTag QuickSlotId, const FArcItemId& ItemId)
	{
		const FArcMassQuickBarStateFragment* State = FMassEntityView(EM, Entity).GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		if (State == nullptr)
		{
			return false;
		}
		for (const FArcMassQuickBarActiveSlot& Active : State->ActiveSlots)
		{
			if (Active.BarId == BarId && Active.QuickSlotId == QuickSlotId && Active.AssignedItemId == ItemId)
			{
				return true;
			}
		}
		return false;
	}

	bool QuickSlotIsEmpty(FMassEntityManager& EM, FMassEntityHandle Entity, FGameplayTag BarId, FGameplayTag QuickSlotId)
	{
		const FArcMassQuickBarStateFragment* State = FMassEntityView(EM, Entity).GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		if (State == nullptr)
		{
			return true;
		}
		for (const FArcMassQuickBarActiveSlot& Active : State->ActiveSlots)
		{
			if (Active.BarId == BarId && Active.QuickSlotId == QuickSlotId)
			{
				return false;
			}
		}
		return true;
	}

	UArcMassQuickBarConfig* MakeCmdTestQuickBarConfig()
	{
		return ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([](UArcMassQuickBarConfig& Cfg)
		{
			FArcMassQuickBarEntry& BarEntry = Cfg.QuickBars.AddDefaulted_GetRef();
			BarEntry.BarId = TAG_CmdTest_QuickBar;
			FArcMassQuickBarSlot& QSlot = BarEntry.Slots.AddDefaulted_GetRef();
			QSlot.QuickBarSlotId = TAG_CmdTest_QuickSlot_1;
			QSlot.ItemSlotId = TAG_CmdTest_QuickItemSlot;
		});
	}
}

TEST_CLASS_WITH_FLAGS(ArcMassItemCmd_AddItem, "ArcMassItems.Commands.AddItem", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassItemsTestGameMode::StaticClass() };
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* Settings = nullptr;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		TestRunner->AddExpectedError(TEXT("FMassEntityQuery::CacheArchetypes"), EAutomationExpectedErrorFlags::Contains, -1);
		PIE.Then(TEXT("Setup test entity"), [this]() {
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
			Settings = GetDefault<UArcMassItemTestsSettings>();
			Entity = CreateStoreEntity(*EM);
		});
	}

	TEST_METHOD(AddItem_ValidEntityAndSpec_AddsItemToStore)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			const FArcItemId ExpectedId = Spec.ItemId;
			FArcMassAddItemCommand Cmd(Entity, Spec);

			bool bExecuted = false;
			const bool bDispatched = ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bDispatched));
			ASSERT_THAT(IsTrue(bExecuted));
			ASSERT_THAT(IsTrue(StoreContains(*EM, Entity, ExpectedId)));
		});
	}

	TEST_METHOD(AddItem_InvalidEntity_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			FArcMassAddItemCommand Cmd(FMassEntityHandle{}, Spec);

			bool bExecuted = false;
			const bool bDispatched = ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bDispatched));
			ASSERT_THAT(IsFalse(bExecuted));

			const FArcMassItemStoreFragment* Store = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(IsNotNull(Store));
			ASSERT_THAT(AreEqual(0, Store->ReplicatedItems.Items.Num()));
		});
	}
};

TEST_CLASS_WITH_FLAGS(ArcMassItemCmd_AddNewItem, "ArcMassItems.Commands.AddNewItem", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassItemsTestGameMode::StaticClass() };
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* Settings = nullptr;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		PIE.Then(TEXT("Setup test entity"), [this]() {
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
			Settings = GetDefault<UArcMassItemTestsSettings>();
			Entity = CreateStoreEntity(*EM);
		});
	}

	TEST_METHOD(AddNewItem_ValidDefinition_AddsItemToStore)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddNewItemCommand Cmd(Entity, Settings->SimpleBaseItem);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bExecuted));

			const FArcMassItemStoreFragment* Store = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(IsNotNull(Store));
			ASSERT_THAT(AreEqual(1, Store->ReplicatedItems.Items.Num()));
		});
	}

	TEST_METHOD(AddNewItem_InvalidDefinitionId_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddNewItemCommand Cmd(Entity, FPrimaryAssetId());

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));

			const FArcMassItemStoreFragment* Store = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(AreEqual(0, Store->ReplicatedItems.Items.Num()));
		});
	}

	TEST_METHOD(AddNewItem_InvalidEntity_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddNewItemCommand Cmd(FMassEntityHandle{}, Settings->SimpleBaseItem);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);
			ASSERT_THAT(IsFalse(bExecuted));

			const FArcMassItemStoreFragment* Store = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(AreEqual(0, Store->ReplicatedItems.Items.Num()));
		});
	}
};

TEST_CLASS_WITH_FLAGS(ArcMassItemCmd_RemoveItem, "ArcMassItems.Commands.RemoveItem", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassItemsTestGameMode::StaticClass() };
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* Settings = nullptr;
	FMassEntityHandle Entity;
	FArcItemId SeededItemId;

	BEFORE_EACH()
	{
		PIE.Then(TEXT("Setup test entity"), [this]() {
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
			Settings = GetDefault<UArcMassItemTestsSettings>();
			Entity = CreateStoreEntity(*EM);

			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			SeededItemId = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		});
	}

	TEST_METHOD(RemoveItem_ExistingItem_RemovesFromStore)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassRemoveItemCommand Cmd(Entity, SeededItemId);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bExecuted));
			ASSERT_THAT(IsFalse(StoreContains(*EM, Entity, SeededItemId)));
		});
	}

	TEST_METHOD(RemoveItem_MissingItemId_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassRemoveItemCommand Cmd(Entity, FArcItemId());

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			ASSERT_THAT(IsTrue(StoreContains(*EM, Entity, SeededItemId)));
		});
	}

	TEST_METHOD(RemoveItem_InvalidEntity_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassRemoveItemCommand Cmd(FMassEntityHandle{}, SeededItemId);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			ASSERT_THAT(IsTrue(StoreContains(*EM, Entity, SeededItemId)));
		});
	}
};

TEST_CLASS_WITH_FLAGS(ArcMassItemCmd_AssignItemSlot, "ArcMassItems.Commands.AssignItemSlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassItemsTestGameMode::StaticClass() };
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* Settings = nullptr;
	FMassEntityHandle Entity;
	FArcItemId SeededItemId;

	BEFORE_EACH()
	{
		PIE.Then(TEXT("Setup test entity"), [this]() {
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
			Settings = GetDefault<UArcMassItemTestsSettings>();
			Entity = CreateStoreEntity(*EM);

			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			SeededItemId = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		});
	}

	TEST_METHOD(AssignItemSlot_ValidItemAndSlot_BindsItemToSlot)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAssignItemSlotCommand Cmd(Entity, SeededItemId, TAG_CmdTest_Slot_Primary);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bExecuted));
			ASSERT_THAT(IsTrue(GetItemSlot(*EM, Entity, SeededItemId) == TAG_CmdTest_Slot_Primary));
		});
	}

	TEST_METHOD(AssignItemSlot_InvalidItem_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAssignItemSlotCommand Cmd(Entity, FArcItemId(), TAG_CmdTest_Slot_Primary);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			ASSERT_THAT(IsTrue(GetItemSlot(*EM, Entity, SeededItemId) == FGameplayTag()));
		});
	}

	TEST_METHOD(AssignItemSlot_InvalidEntity_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAssignItemSlotCommand Cmd(FMassEntityHandle{}, SeededItemId, TAG_CmdTest_Slot_Primary);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			ASSERT_THAT(IsTrue(GetItemSlot(*EM, Entity, SeededItemId) == FGameplayTag()));
		});
	}
};

TEST_CLASS_WITH_FLAGS(ArcMassItemCmd_AddItemToSlot, "ArcMassItems.Commands.AddItemToSlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassItemsTestGameMode::StaticClass() };
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* Settings = nullptr;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		PIE.Then(TEXT("Setup test entity"), [this]() {
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
			Settings = GetDefault<UArcMassItemTestsSettings>();
			Entity = CreateStoreEntity(*EM);
		});
	}

	TEST_METHOD(AddItemToSlot_ValidSpecAndFreeSlot_AddsAndBinds)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			const FArcItemId ExpectedId = Spec.ItemId;
			FArcMassAddItemToSlotCommand Cmd(Entity, Spec, TAG_CmdTest_Slot_Primary);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bExecuted));
			ASSERT_THAT(IsTrue(StoreContains(*EM, Entity, ExpectedId)));
			ASSERT_THAT(IsTrue(GetItemSlot(*EM, Entity, ExpectedId) == TAG_CmdTest_Slot_Primary));
		});
	}

	TEST_METHOD(AddItemToSlot_OccupiedSlot_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcItemSpec FirstSpec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			const FArcItemId FirstId = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), FirstSpec);
			ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), FirstId, TAG_CmdTest_Slot_Primary);

			FArcItemSpec AttemptSpec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			const FArcItemId AttemptedId = AttemptSpec.ItemId;
			FArcMassAddItemToSlotCommand Cmd(Entity, AttemptSpec, TAG_CmdTest_Slot_Primary);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			ASSERT_THAT(IsFalse(StoreContains(*EM, Entity, AttemptedId)));
			ASSERT_THAT(IsTrue(GetItemSlot(*EM, Entity, FirstId) == TAG_CmdTest_Slot_Primary));
		});
	}

	TEST_METHOD(AddItemToSlot_InvalidEntity_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			FArcMassAddItemToSlotCommand Cmd(FMassEntityHandle{}, Spec, TAG_CmdTest_Slot_Primary);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			const FArcMassItemStoreFragment* Store = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(AreEqual(0, Store->ReplicatedItems.Items.Num()));
		});
	}
};

TEST_CLASS_WITH_FLAGS(ArcMassItemCmd_AddNewItemToSlot, "ArcMassItems.Commands.AddNewItemToSlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassItemsTestGameMode::StaticClass() };
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* Settings = nullptr;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		PIE.Then(TEXT("Setup test entity"), [this]() {
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
			Settings = GetDefault<UArcMassItemTestsSettings>();
			Entity = CreateStoreEntity(*EM);
		});
	}

	TEST_METHOD(AddNewItemToSlot_ValidDefinitionAndFreeSlot_AddsAndBinds)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddNewItemToSlotCommand Cmd(Entity, Settings->SimpleBaseItem, TAG_CmdTest_Slot_Primary);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bExecuted));

			const FArcMassItemStoreFragment* Store = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(AreEqual(1, Store->ReplicatedItems.Items.Num()));

			const FArcItemData* Created = ArcMassItems::FindItemByDefinition(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Settings->SimpleBaseItem.AssetId);
			ASSERT_THAT(IsNotNull(Created));
			ASSERT_THAT(IsTrue(Created->GetSlotId() == TAG_CmdTest_Slot_Primary));
		});
	}

	TEST_METHOD(AddNewItemToSlot_InvalidDefinition_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddNewItemToSlotCommand Cmd(Entity, FPrimaryAssetId(), TAG_CmdTest_Slot_Primary);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			const FArcMassItemStoreFragment* Store = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(AreEqual(0, Store->ReplicatedItems.Items.Num()));
		});
	}

	TEST_METHOD(AddNewItemToSlot_InvalidEntity_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddNewItemToSlotCommand Cmd(FMassEntityHandle{}, Settings->SimpleBaseItem, TAG_CmdTest_Slot_Primary);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);
			ASSERT_THAT(IsFalse(bExecuted));

			const FArcMassItemStoreFragment* Store = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(AreEqual(0, Store->ReplicatedItems.Items.Num()));
		});
	}
};

TEST_CLASS_WITH_FLAGS(ArcMassItemCmd_RemoveItemFromSlot, "ArcMassItems.Commands.RemoveItemFromSlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassItemsTestGameMode::StaticClass() };
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* Settings = nullptr;
	FMassEntityHandle Entity;
	FArcItemId SeededItemId;

	BEFORE_EACH()
	{
		PIE.Then(TEXT("Setup test entity"), [this]() {
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
			Settings = GetDefault<UArcMassItemTestsSettings>();
			Entity = CreateStoreEntity(*EM);

			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			SeededItemId = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
			ArcMassItems::AddItemToSlot(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), SeededItemId, TAG_CmdTest_Slot_Primary);
		});
	}

	TEST_METHOD(RemoveItemFromSlot_BoundItem_ClearsSlot_ItemRemains)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassRemoveItemFromSlotCommand Cmd(Entity, SeededItemId);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bExecuted));
			ASSERT_THAT(IsTrue(StoreContains(*EM, Entity, SeededItemId)));
			ASSERT_THAT(IsTrue(GetItemSlot(*EM, Entity, SeededItemId) == FGameplayTag()));
		});
	}

	TEST_METHOD(RemoveItemFromSlot_UnknownItem_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassRemoveItemFromSlotCommand Cmd(Entity, FArcItemId());

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			ASSERT_THAT(IsTrue(GetItemSlot(*EM, Entity, SeededItemId) == TAG_CmdTest_Slot_Primary));
		});
	}

	TEST_METHOD(RemoveItemFromSlot_InvalidEntity_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassRemoveItemFromSlotCommand Cmd(FMassEntityHandle{}, SeededItemId);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			ASSERT_THAT(IsTrue(GetItemSlot(*EM, Entity, SeededItemId) == TAG_CmdTest_Slot_Primary));
		});
	}
};

TEST_CLASS_WITH_FLAGS(ArcMassItemCmd_AddItemToQuickSlot, "ArcMassItems.Commands.AddItemToQuickSlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassItemsTestGameMode::StaticClass() };
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* Settings = nullptr;
	FMassEntityHandle Entity;
	FArcItemId SeededItemId;

	BEFORE_EACH()
	{
		PIE.Then(TEXT("Setup test entity"), [this]() {
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
			Settings = GetDefault<UArcMassItemTestsSettings>();

			Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EM, MakeCmdTestQuickBarConfig());

			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			SeededItemId = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		});
	}

	TEST_METHOD(AddItemToQuickSlot_ValidItemBarSlot_BindsItem)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddItemToQuickSlotCommand Cmd(Entity, SeededItemId, TAG_CmdTest_QuickBar, TAG_CmdTest_QuickSlot_1);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bExecuted));
			ASSERT_THAT(IsTrue(QuickSlotHasItem(*EM, Entity, TAG_CmdTest_QuickBar, TAG_CmdTest_QuickSlot_1, SeededItemId)));
		});
	}

	TEST_METHOD(AddItemToQuickSlot_UnknownItem_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddItemToQuickSlotCommand Cmd(Entity, FArcItemId(), TAG_CmdTest_QuickBar, TAG_CmdTest_QuickSlot_1);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			ASSERT_THAT(IsTrue(QuickSlotIsEmpty(*EM, Entity, TAG_CmdTest_QuickBar, TAG_CmdTest_QuickSlot_1)));
		});
	}

	TEST_METHOD(AddItemToQuickSlot_UnknownBar_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddItemToQuickSlotCommand Cmd(Entity, SeededItemId, FGameplayTag(), TAG_CmdTest_QuickSlot_1);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
		});
	}
};

TEST_CLASS_WITH_FLAGS(ArcMassItemCmd_AddNewItemToQuickSlot, "ArcMassItems.Commands.AddNewItemToQuickSlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassItemsTestGameMode::StaticClass() };
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* Settings = nullptr;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		PIE.Then(TEXT("Setup test entity"), [this]() {
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
			Settings = GetDefault<UArcMassItemTestsSettings>();
			Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EM, MakeCmdTestQuickBarConfig());
		});
	}

	TEST_METHOD(AddNewItemToQuickSlot_ValidDefinition_CreatesAndBinds)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddNewItemToQuickSlotCommand Cmd(Entity, Settings->SimpleBaseItem, TAG_CmdTest_QuickBar, TAG_CmdTest_QuickSlot_1);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bExecuted));

			const FArcMassItemStoreFragment* Store = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(AreEqual(1, Store->ReplicatedItems.Items.Num()));

			const FArcItemData* Created = ArcMassItems::FindItemByDefinition(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Settings->SimpleBaseItem.AssetId);
			ASSERT_THAT(IsNotNull(Created));
			ASSERT_THAT(IsTrue(QuickSlotHasItem(*EM, Entity, TAG_CmdTest_QuickBar, TAG_CmdTest_QuickSlot_1, Created->GetItemId())));
		});
	}

	TEST_METHOD(AddNewItemToQuickSlot_InvalidDefinition_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddNewItemToQuickSlotCommand Cmd(Entity, FPrimaryAssetId(), TAG_CmdTest_QuickBar, TAG_CmdTest_QuickSlot_1);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			const FArcMassItemStoreFragment* Store = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(AreEqual(0, Store->ReplicatedItems.Items.Num()));
			ASSERT_THAT(IsTrue(QuickSlotIsEmpty(*EM, Entity, TAG_CmdTest_QuickBar, TAG_CmdTest_QuickSlot_1)));
		});
	}

	TEST_METHOD(AddNewItemToQuickSlot_UnknownBar_ReturnsFalse_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassAddNewItemToQuickSlotCommand Cmd(Entity, Settings->SimpleBaseItem, FGameplayTag(), TAG_CmdTest_QuickSlot_1);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			const FArcMassItemStoreFragment* Store = FMassEntityView(*EM, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(AreEqual(0, Store->ReplicatedItems.Items.Num()));
		});
	}
};

TEST_CLASS_WITH_FLAGS(ArcMassItemCmd_DestroyItem, "ArcMassItems.Commands.DestroyItem", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassItemsTestGameMode::StaticClass() };
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* Settings = nullptr;
	FMassEntityHandle Entity;
	FArcItemId SeededItemId;

	BEFORE_EACH()
	{
		PIE.Then(TEXT("Setup test entity"), [this]() {
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
			Settings = GetDefault<UArcMassItemTestsSettings>();
			Entity = CreateStoreEntity(*EM);

			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			SeededItemId = ArcMassItems::AddItem(*EM, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		});
	}

	TEST_METHOD(DestroyItemCommand_RemovesItem)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassDestroyItemCommand Cmd(Entity, SeededItemId);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bExecuted));
			ASSERT_THAT(IsFalse(StoreContains(*EM, Entity, SeededItemId)));
		});
	}

	TEST_METHOD(DestroyItemCommand_InvalidEntity_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassDestroyItemCommand Cmd(FMassEntityHandle{}, SeededItemId);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			ASSERT_THAT(IsTrue(StoreContains(*EM, Entity, SeededItemId)));
		});
	}
};

TEST_CLASS_WITH_FLAGS(ArcMassItemCmd_MoveItem, "ArcMassItems.Commands.MoveItem", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FArcStandalonePIEComponent PIE{ TestRunner, TestCommandBuilder, bInitializing,
		AArcMassItemsTestGameMode::StaticClass() };
	UWorld* World = nullptr;
	FMassEntityManager* EM = nullptr;
	const UArcMassItemTestsSettings* Settings = nullptr;
	FMassEntityHandle SourceEntity;
	FMassEntityHandle TargetEntity;
	FArcItemId SeededItemId;

	BEFORE_EACH()
	{
		PIE.Then(TEXT("Setup test entities"), [this]() {
			FAutomationTestBase::bSuppressLogWarnings = true;
			World = PIE.GetState().World;
			UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
			check(MES);
			EM = &MES->GetMutableEntityManager();
			Settings = GetDefault<UArcMassItemTestsSettings>();
			SourceEntity = CreateStoreEntity(*EM);
			TargetEntity = CreateStoreEntity(*EM);

			FArcItemSpec Spec = FArcItemSpec::NewItem(Settings->SimpleBaseItem, 1, 1);
			SeededItemId = ArcMassItems::AddItem(*EM, SourceEntity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		});
	}

	TEST_METHOD(MoveItemCommand_MovesItemBetweenEntities)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassMoveItemCommand Cmd(SourceEntity, SeededItemId, TargetEntity);

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsTrue(bExecuted));
			ASSERT_THAT(IsFalse(StoreContains(*EM, SourceEntity, SeededItemId)));
			ASSERT_THAT(IsTrue(StoreContains(*EM, TargetEntity, SeededItemId)));
		});
	}

	TEST_METHOD(MoveItemCommand_InvalidTarget_NoMutation)
	{
		PIE.Then(TEXT("Test body"), [this]() {
			FArcMassMoveItemCommand Cmd(SourceEntity, SeededItemId, FMassEntityHandle{});

			bool bExecuted = false;
			ArcMassItems::TestHelpers::SendMassItemCommand(World, MoveTemp(Cmd), bExecuted);

			ASSERT_THAT(IsFalse(bExecuted));
			ASSERT_THAT(IsTrue(StoreContains(*EM, SourceEntity, SeededItemId)));
		});
	}
};
#endif