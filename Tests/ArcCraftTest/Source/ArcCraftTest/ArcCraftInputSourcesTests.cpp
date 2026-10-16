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
#include "ArcMassItemTestHelpers.h"
#include "Components/ActorTestSpawner.h"

#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"

#include "ArcCraft/Commands/ArcCraftFromStationInputCommand.h"
#include "ArcCraft/Mass/ArcCraftDeliveryConfigFragment.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InSrc_Slot, "Test.InSrc.Slot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InSrc_Socket, "Test.InSrc.Socket");

namespace ArcCraftInputSourcesTestsLocal
{
	const UScriptStruct* BaseStore()
	{
		return FArcMassItemStoreFragment::StaticStruct();
	}

	const UScriptStruct* SecondaryStore()
	{
		return FArcTestSecondaryItemStoreFragment::StaticStruct();
	}

	FMassEntityHandle BuildStation(FMassEntityManager& EM, bool bEntityStoreSource)
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcCraftInputFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcCraftQueueFragment()));

		FMassEntityHandle Entity = EM.CreateEntity(Fragments);
		EM.AddTagToEntity(Entity, FArcCraftStationTag::StaticStruct());

		FArcCraftDeliveryConfigFragment Delivery;
		if (bEntityStoreSource)
		{
			Delivery.IngredientSource.InitializeAs<FArcCraftItemSource_EntityStore>();
		}
		else
		{
			Delivery.IngredientSource.InitializeAs<FArcCraftItemSource>();
		}
		EM.AddConstSharedFragmentToEntity(Entity, EM.GetOrCreateConstSharedFragment(Delivery));
		return Entity;
	}

	FMassEntityHandle BuildPlayer(FMassEntityManager& EM, bool bWithSecondaryStore)
	{
		UE::Mass::FEntityBuilder Builder(EM);
		Builder.Add<FTransformFragment>().Add<FArcMassItemStoreFragment>();
		if (bWithSecondaryStore)
		{
			Builder.Add<FArcTestSecondaryItemStoreFragment>();
		}
		return Builder.Commit();
	}

	FArcItemId AddLooseItem(FMassEntityManager& EM, FMassEntityHandle Owner, const UScriptStruct* StoreType, const FPrimaryAssetId& Def, int32 Stacks)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(Def);
		Spec.SetItemId(FArcItemId::Generate());
		Spec.Amount = static_cast<uint16>(Stacks);
		return ArcMassItems::AddItem(EM, Owner, StoreType, Spec);
	}

	int32 StacksOf(FMassEntityManager& EM, FMassEntityHandle Owner, const UScriptStruct* StoreType, const FArcItemId& ItemId)
	{
		const FArcItemData* Data = ArcMassItems::FindItemById(EM, Owner, StoreType, ItemId);
		return Data != nullptr ? static_cast<int32>(Data->GetStacks()) : 0;
	}

	const TArray<FArcItemSpec>& InputOf(FMassEntityManager& EM, FMassEntityHandle Station)
	{
		return EM.GetFragmentDataPtr<FArcCraftInputFragment>(Station)->InputItems;
	}

	FArcCraftInputSourceGroup MakeGroup(FMassEntityHandle Owner, const UScriptStruct* StoreType)
	{
		FArcCraftInputSourceGroup Group;
		Group.OwnerEntity = Owner;
		Group.StoreType = const_cast<UScriptStruct*>(StoreType);
		return Group;
	}

	void AddRow(FArcCraftInputSourceGroup& Group, const FArcItemId& ItemId, int32 Stacks)
	{
		FArcCraftInputItem& Row = Group.Items.AddDefaulted_GetRef();
		Row.ItemId = ItemId;
		Row.Stacks = Stacks;
	}
}

// ---------------------------------------------------------------------------
// FArcCraftItemSource::DepositItem_Mass — synchronous move from a player store into the station input
// ---------------------------------------------------------------------------

TEST_CLASS(ArcCraftDepositToInput, "ArcCraft.InputSources.Deposit")
{
	FArcItemTestRecordFiles ItemRecords;
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;
	FPrimaryAssetId Def;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EM = &MES->GetMutableEntityManager();

		Def = ItemRecords.Write(FArcItemDefinitionData());
	}

	TEST_METHOD(PartialDeposit_KeepsItemId_MovesStacks_LeavesRemainder)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FMassEntityHandle Station = BuildStation(*EM, true);
		const FMassEntityHandle Player = BuildPlayer(*EM, false);
		const FArcItemId ItemId = AddLooseItem(*EM, Player, BaseStore(), Def, 5);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		FArcCraftItemSource_EntityStore Source;
		ASSERT_THAT(IsTrue(Source.DepositItem_Mass(*EM, Station, Player, BaseStore(), ItemId, 3),
			TEXT("A deposit of 3 out of 5 held stacks succeeds")));

		const TArray<FArcItemSpec>& Input = InputOf(*EM, Station);
		ASSERT_THAT(AreEqual(1, Input.Num(), TEXT("The deposit appears synchronously, no flush needed")));
		ASSERT_THAT(IsTrue(Input[0].ItemId == ItemId, TEXT("The station input spec keeps the inventory ItemId")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(3), Input[0].Amount, TEXT("Amount equals the deposited stacks")));
		ASSERT_THAT(IsTrue(Input[0].GetItemDefinitionId() == Def));
		ASSERT_THAT(AreEqual(2, StacksOf(*EM, Player, BaseStore(), ItemId), TEXT("The remainder stays in the player store")));
	}

	TEST_METHOD(FullDeposit_RemovesItemFromStore)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FMassEntityHandle Station = BuildStation(*EM, true);
		const FMassEntityHandle Player = BuildPlayer(*EM, false);
		const FArcItemId ItemId = AddLooseItem(*EM, Player, BaseStore(), Def, 2);

		FArcCraftItemSource_EntityStore Source;
		ASSERT_THAT(IsTrue(Source.DepositItem_Mass(*EM, Station, Player, BaseStore(), ItemId, 2)));

		ASSERT_THAT(AreEqual(static_cast<uint16>(2), InputOf(*EM, Station)[0].Amount));
		ASSERT_THAT(IsNull(ArcMassItems::FindItemById(*EM, Player, BaseStore(), ItemId),
			TEXT("Depositing every stack removes the item from the store")));
	}

	TEST_METHOD(NonStoreStoreType_Rejected_BothSidesUnchanged)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FMassEntityHandle Station = BuildStation(*EM, true);
		const FMassEntityHandle Player = BuildPlayer(*EM, false);
		const FArcItemId ItemId = AddLooseItem(*EM, Player, BaseStore(), Def, 2);

		FArcCraftItemSource_EntityStore Source;
		ASSERT_THAT(IsFalse(Source.DepositItem_Mass(*EM, Station, Player, FTransformFragment::StaticStruct(), ItemId, 1),
			TEXT("A struct that is not an item store is refused without reaching the store checkf")));
		ASSERT_THAT(IsFalse(Source.DepositItem_Mass(*EM, Station, Player, nullptr, ItemId, 1),
			TEXT("A null store type is refused")));

		ASSERT_THAT(AreEqual(0, InputOf(*EM, Station).Num()));
		ASSERT_THAT(AreEqual(2, StacksOf(*EM, Player, BaseStore(), ItemId)));
	}

	TEST_METHOD(InsufficientStacks_Rejected_BothSidesUnchanged)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FMassEntityHandle Station = BuildStation(*EM, true);
		const FMassEntityHandle Player = BuildPlayer(*EM, false);
		const FArcItemId ItemId = AddLooseItem(*EM, Player, BaseStore(), Def, 1);

		FArcCraftItemSource_EntityStore Source;
		ASSERT_THAT(IsFalse(Source.DepositItem_Mass(*EM, Station, Player, BaseStore(), ItemId, 2),
			TEXT("Asking for 2 stacks of a 1-stack item is refused")));

		ASSERT_THAT(AreEqual(0, InputOf(*EM, Station).Num(), TEXT("Nothing reaches the input")));
		ASSERT_THAT(AreEqual(1, StacksOf(*EM, Player, BaseStore(), ItemId), TEXT("The item keeps its stack")));
	}

	TEST_METHOD(BaseSource_RefusesDeposits_BothSidesUnchanged)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FMassEntityHandle Station = BuildStation(*EM, false);
		const FMassEntityHandle Player = BuildPlayer(*EM, false);
		const FArcItemId ItemId = AddLooseItem(*EM, Player, BaseStore(), Def, 2);

		FArcCraftItemSource BaseSource;
		ASSERT_THAT(IsFalse(BaseSource.AcceptsDeposits()));
		ASSERT_THAT(IsFalse(BaseSource.DepositItem_Mass(*EM, Station, Player, BaseStore(), ItemId, 1)));

		ASSERT_THAT(AreEqual(0, InputOf(*EM, Station).Num()));
		ASSERT_THAT(AreEqual(2, StacksOf(*EM, Player, BaseStore(), ItemId)));
	}
};

// ---------------------------------------------------------------------------
// ArcCraftFromStationInput::ValidateSources / MoveSourcesToInput — all-or-nothing
// ---------------------------------------------------------------------------

TEST_CLASS(ArcCraftValidateSources, "ArcCraft.InputSources.Validate")
{
	FArcItemTestRecordFiles ItemRecords;
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;
	FPrimaryAssetId Def;
	FMassEntityHandle Station;
	FMassEntityHandle Player;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EM = &MES->GetMutableEntityManager();

		Def = ItemRecords.Write(FArcItemDefinitionData());
		Station = ArcCraftInputSourcesTestsLocal::BuildStation(*EM, true);
		Player = ArcCraftInputSourcesTestsLocal::BuildPlayer(*EM, false);
	}

	bool RejectsAndMovesNothing(TConstArrayView<FArcCraftInputSourceGroup> Sources)
	{
		const bool bValid = ArcCraftFromStationInput::ValidateSources(*EM, Station, Player, Sources);
		const bool bMoved = ArcCraftFromStationInput::MoveSourcesToInput(*EM, Station, Player, Sources);
		return !bValid && !bMoved && ArcCraftInputSourcesTestsLocal::InputOf(*EM, Station).IsEmpty();
	}

	TEST_METHOD(OwnerIsNotInstigator_Rejected)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FMassEntityHandle OtherPlayer = BuildPlayer(*EM, false);
		const FArcItemId ItemId = AddLooseItem(*EM, OtherPlayer, BaseStore(), Def, 2);
		FArcCraftInputSourceGroup Group = MakeGroup(OtherPlayer, BaseStore());
		AddRow(Group, ItemId, 1);

		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ Group }), TEXT("Another player's entity is not a valid source")));
		ASSERT_THAT(AreEqual(2, StacksOf(*EM, OtherPlayer, BaseStore(), ItemId), TEXT("The other player's item is untouched")));
	}

	TEST_METHOD(NullOrNonStoreType_Rejected)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcItemId ItemId = AddLooseItem(*EM, Player, BaseStore(), Def, 2);

		FArcCraftInputSourceGroup NullType = MakeGroup(Player, nullptr);
		AddRow(NullType, ItemId, 1);
		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ NullType }), TEXT("A null store type is rejected")));

		FArcCraftInputSourceGroup NotAStore = MakeGroup(Player, FTransformFragment::StaticStruct());
		AddRow(NotAStore, ItemId, 1);
		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ NotAStore }), TEXT("A non-store struct is rejected without a checkf")));

		ASSERT_THAT(AreEqual(2, StacksOf(*EM, Player, BaseStore(), ItemId)));
	}

	TEST_METHOD(StoreMissingOnOwner_Rejected)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcItemId ItemId = AddLooseItem(*EM, Player, BaseStore(), Def, 2);
		FArcCraftInputSourceGroup Group = MakeGroup(Player, SecondaryStore());
		AddRow(Group, ItemId, 1);

		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ Group }), TEXT("A store type the owner does not carry is rejected")));
		ASSERT_THAT(AreEqual(2, StacksOf(*EM, Player, BaseStore(), ItemId)));
	}

	TEST_METHOD(RowWithFewerThanOneStack_Rejected)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcItemId ItemId = AddLooseItem(*EM, Player, BaseStore(), Def, 2);

		FArcCraftInputSourceGroup Zero = MakeGroup(Player, BaseStore());
		AddRow(Zero, ItemId, 0);
		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ Zero }), TEXT("A 0-stack row is rejected")));

		FArcCraftInputSourceGroup Negative = MakeGroup(Player, BaseStore());
		AddRow(Negative, ItemId, -3);
		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ Negative }), TEXT("A negative row cannot add stacks back")));

		ASSERT_THAT(AreEqual(2, StacksOf(*EM, Player, BaseStore(), ItemId)));
	}

	TEST_METHOD(ItemMissingFromStore_Rejected)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcItemId Held = AddLooseItem(*EM, Player, BaseStore(), Def, 2);
		FArcCraftInputSourceGroup Group = MakeGroup(Player, BaseStore());
		AddRow(Group, Held, 1);
		AddRow(Group, FArcItemId::Generate(), 1);

		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ Group }), TEXT("One unknown item rejects the whole command")));
		ASSERT_THAT(AreEqual(2, StacksOf(*EM, Player, BaseStore(), Held), TEXT("The valid row before it does not move either")));
	}

	TEST_METHOD(SlottedItem_Rejected)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcItemId ItemId = AddLooseItem(*EM, Player, BaseStore(), Def, 1);
		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EM, Player, BaseStore(), ItemId, TAG_InSrc_Slot)));
		FArcCraftInputSourceGroup Group = MakeGroup(Player, BaseStore());
		AddRow(Group, ItemId, 1);

		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ Group }), TEXT("An equipped / quick-slotted item is rejected")));
		ASSERT_THAT(AreEqual(1, StacksOf(*EM, Player, BaseStore(), ItemId)));
	}

	TEST_METHOD(AttachedItemAndItsOwner_Rejected)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcItemId Weapon = AddLooseItem(*EM, Player, BaseStore(), Def, 1);
		const FArcItemId Gem = AddLooseItem(*EM, Player, BaseStore(), Def, 1);
		ASSERT_THAT(IsTrue(ArcMassItems::AttachItem(*EM, Player, BaseStore(), Weapon, Gem, TAG_InSrc_Socket)));

		FArcCraftInputSourceGroup GemGroup = MakeGroup(Player, BaseStore());
		AddRow(GemGroup, Gem, 1);
		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ GemGroup }), TEXT("A socketed gem (OwnerId set) is rejected")));

		FArcCraftInputSourceGroup WeaponGroup = MakeGroup(Player, BaseStore());
		AddRow(WeaponGroup, Weapon, 1);
		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ WeaponGroup }), TEXT("An item holding attached items is rejected")));

		ASSERT_THAT(AreEqual(1, StacksOf(*EM, Player, BaseStore(), Gem)));
		ASSERT_THAT(AreEqual(1, StacksOf(*EM, Player, BaseStore(), Weapon)));
	}

	TEST_METHOD(DuplicateRowsSummingAboveHeld_Rejected)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcItemId ItemId = AddLooseItem(*EM, Player, BaseStore(), Def, 3);

		FArcCraftInputSourceGroup Group = MakeGroup(Player, BaseStore());
		AddRow(Group, ItemId, 2);
		AddRow(Group, ItemId, 2);
		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ Group }), TEXT("Rows {X,2} and {X,2} with X holding 3 sum to 4 > 3")));

		FArcCraftInputSourceGroup First = MakeGroup(Player, BaseStore());
		AddRow(First, ItemId, 2);
		FArcCraftInputSourceGroup Second = MakeGroup(Player, BaseStore());
		AddRow(Second, ItemId, 2);
		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ First, Second }), TEXT("The sum is taken across groups too")));

		ASSERT_THAT(AreEqual(3, StacksOf(*EM, Player, BaseStore(), ItemId), TEXT("No stacks are created or taken")));
	}

	TEST_METHOD(StationSourceRefusesDeposits_Rejected)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		Station = BuildStation(*EM, false);
		const FArcItemId ItemId = AddLooseItem(*EM, Player, BaseStore(), Def, 2);
		FArcCraftInputSourceGroup Group = MakeGroup(Player, BaseStore());
		AddRow(Group, ItemId, 1);

		ASSERT_THAT(IsTrue(RejectsAndMovesNothing({ Group }), TEXT("A station whose source refuses deposits rejects sources")));
		ASSERT_THAT(AreEqual(2, StacksOf(*EM, Player, BaseStore(), ItemId)));
	}

	TEST_METHOD(EmptySources_Valid_NothingMoves)
	{
		ASSERT_THAT(IsTrue(ArcCraftFromStationInput::ValidateSources(*EM, Station, Player, {})));
		ASSERT_THAT(IsTrue(ArcCraftFromStationInput::MoveSourcesToInput(*EM, Station, Player, {})));
		ASSERT_THAT(AreEqual(0, ArcCraftInputSourcesTestsLocal::InputOf(*EM, Station).Num()));
	}

	TEST_METHOD(MoveSourcesToInput_TwoStoresAndSplitRows_AllMoved)
	{
		using namespace ArcCraftInputSourcesTestsLocal;
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		Player = BuildPlayer(*EM, true);
		const FArcItemId Leather = AddLooseItem(*EM, Player, BaseStore(), Def, 4);
		const FArcItemId Thread = AddLooseItem(*EM, Player, SecondaryStore(), Def, 3);

		FArcCraftInputSourceGroup BaseGroup = MakeGroup(Player, BaseStore());
		AddRow(BaseGroup, Leather, 2);
		AddRow(BaseGroup, Leather, 2);
		FArcCraftInputSourceGroup BagGroup = MakeGroup(Player, SecondaryStore());
		AddRow(BagGroup, Thread, 2);
		const TArray<FArcCraftInputSourceGroup> Sources = { BaseGroup, BagGroup };

		ASSERT_THAT(IsTrue(ArcCraftFromStationInput::ValidateSources(*EM, Station, Player, Sources),
			TEXT("Rows summing exactly to the held stacks are valid")));
		ASSERT_THAT(IsTrue(ArcCraftFromStationInput::MoveSourcesToInput(*EM, Station, Player, Sources)));

		const TArray<FArcItemSpec>& Input = InputOf(*EM, Station);
		int32 LeatherInInput = 0;
		int32 ThreadInInput = 0;
		for (const FArcItemSpec& Spec : Input)
		{
			LeatherInInput += Spec.ItemId == Leather ? static_cast<int32>(Spec.Amount) : 0;
			ThreadInInput += Spec.ItemId == Thread ? static_cast<int32>(Spec.Amount) : 0;
		}
		ASSERT_THAT(AreEqual(4, LeatherInInput, TEXT("Both leather rows reach the input")));
		ASSERT_THAT(AreEqual(2, ThreadInInput, TEXT("The secondary-store row reaches the input")));
		ASSERT_THAT(IsNull(ArcMassItems::FindItemById(*EM, Player, BaseStore(), Leather), TEXT("All 4 leather left the base store")));
		ASSERT_THAT(AreEqual(1, StacksOf(*EM, Player, SecondaryStore(), Thread), TEXT("1 thread stays in the secondary store")));
	}
};
