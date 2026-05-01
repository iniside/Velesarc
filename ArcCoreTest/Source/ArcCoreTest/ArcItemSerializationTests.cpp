// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcItemsTests.h"

#include "CQTest.h"

#include "Items/ArcItemSpec.h"
#include "Items/ArcItemsArray.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemInstance.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/Fragments/ArcItemFragment_Stacks.h"
#include "Persistence/ArcItemStoreSerializer.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "Components/ActorTestSpawner.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SerTest_Slot01, "QuickSlotId.ArcCoreTest.Serialization.01");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SerTest_Slot02, "QuickSlotId.ArcCoreTest.Serialization.02");

namespace SerializationTestHelpers
{

UArcItemDefinition* CreateTransientItemDef(const FName& Name)
{
	UArcItemDefinition* Def = NewObject<UArcItemDefinition>(
		GetTransientPackage(), Name, RF_Transient);
	Def->RegenerateItemId();
	return Def;
}

FArcItemSpec MakeSpec(const UArcItemDefinition* Def, int32 Amount = 1, uint8 Level = 1)
{
	FArcItemSpec Spec;
	Spec.SetItemDefinition(Def->GetPrimaryAssetId())
		.SetItemDefinitionAsset(Def)
		.SetAmount(Amount)
		.SetItemLevel(Level);
	return Spec;
}

void PatchDefinitions(
	TArray<FArcItemCopyContainerHelper>& Helpers,
	const TMap<FPrimaryAssetId, const UArcItemDefinition*>& DefMap)
{
	for (FArcItemCopyContainerHelper& Helper : Helpers)
	{
		if (const UArcItemDefinition* const* Found = DefMap.Find(Helper.Item.GetItemDefinitionId()))
		{
			Helper.Item.SetItemDefinitionAsset(*Found);
		}
		for (FArcAttachedItemHelper& Attached : Helper.ItemAttachments)
		{
			if (const UArcItemDefinition* const* Found = DefMap.Find(Attached.Item.GetItemDefinitionId()))
			{
				Attached.Item.SetItemDefinitionAsset(*Found);
			}
		}
	}
}

} // namespace SerializationTestHelpers

namespace
{

TArray<uint8> SaveToBytes(const TArray<FArcItemCopyContainerHelper>& Source)
{
	FArcJsonSaveArchive SaveAr;
	FArcItemStoreSerializer::Save(Source, SaveAr);
	return SaveAr.Finalize();
}

TArray<FArcItemCopyContainerHelper> LoadFromBytes(const TArray<uint8>& Data)
{
	FArcJsonLoadArchive LoadAr;
	LoadAr.InitializeFromData(Data);

	TArray<FArcItemCopyContainerHelper> Result;
	FArcItemStoreSerializer::Load(Result, LoadAr);
	return Result;
}

TArray<FArcItemCopyContainerHelper> RoundTrip(const TArray<FArcItemCopyContainerHelper>& Source)
{
	return LoadFromBytes(SaveToBytes(Source));
}

FArcItemCopyContainerHelper MakeHelper(
	const FPrimaryAssetId& DefinitionId,
	uint16 Amount = 1,
	uint8 Level = 1,
	FGameplayTag SlotId = FGameplayTag())
{
	FArcItemCopyContainerHelper Helper;
	Helper.Item.ItemId = FArcItemId::Generate();
	Helper.Item.ItemDefinitionId = DefinitionId;
	Helper.Item.Amount = Amount;
	Helper.Item.Level = Level;
	Helper.ItemId = Helper.Item.ItemId;
	Helper.SlotId = SlotId;
	return Helper;
}

} // anonymous namespace

// ============================================================================
// Basic Round-Trip Tests
// ============================================================================

TEST_CLASS(ArcItemSerialization_Basic, "ArcCore.Persistence")
{
	TEST_METHOD(EmptyArray_RoundTrips)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 0));
	}

	TEST_METHOD(SingleItem_PreservesItemId)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		Source.Add(MakeHelper(FPrimaryAssetId("ArcItemDefinition:TestSword")));

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 1));
		ASSERT_THAT(IsTrue(Loaded[0].Item.ItemId == Source[0].Item.ItemId));
		ASSERT_THAT(IsTrue(Loaded[0].ItemId == Source[0].ItemId));
	}

	TEST_METHOD(SingleItem_PreservesDefinitionId)
	{
		const FPrimaryAssetId ExpectedId("ArcItemDefinition:TestSword");

		TArray<FArcItemCopyContainerHelper> Source;
		Source.Add(MakeHelper(ExpectedId));

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 1));
		ASSERT_THAT(IsTrue(Loaded[0].Item.GetItemDefinitionId() == ExpectedId));
	}

	TEST_METHOD(SingleItem_PreservesAmount)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		Source.Add(MakeHelper(FPrimaryAssetId("ArcItemDefinition:TestAmmo"), 42));

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 1));
		ASSERT_THAT(AreEqual(static_cast<int32>(Loaded[0].Item.Amount), 42));
	}

	TEST_METHOD(SingleItem_PreservesLevel)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		Source.Add(MakeHelper(FPrimaryAssetId("ArcItemDefinition:TestArmor"), 1, 5));

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 1));
		ASSERT_THAT(AreEqual(static_cast<int32>(Loaded[0].Item.Level), 5));
	}

	TEST_METHOD(SingleItem_PreservesSlotId)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		Source.Add(MakeHelper(
			FPrimaryAssetId("ArcItemDefinition:TestSword"),
			1, 1, TAG_SerTest_Slot01));

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 1));
		ASSERT_THAT(IsTrue(Loaded[0].SlotId == TAG_SerTest_Slot01));
	}

	TEST_METHOD(MultipleItems_PreservesCount)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		Source.Add(MakeHelper(FPrimaryAssetId("ArcItemDefinition:Sword"), 1, 1));
		Source.Add(MakeHelper(FPrimaryAssetId("ArcItemDefinition:Shield"), 1, 2));
		Source.Add(MakeHelper(FPrimaryAssetId("ArcItemDefinition:Potion"), 5, 1));

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 3));

		// Verify each item preserved its data
		for (int32 i = 0; i < 3; ++i)
		{
			ASSERT_THAT(IsTrue(Loaded[i].Item.ItemId == Source[i].Item.ItemId));
			ASSERT_THAT(IsTrue(Loaded[i].Item.GetItemDefinitionId() == Source[i].Item.GetItemDefinitionId()));
			ASSERT_THAT(AreEqual(static_cast<int32>(Loaded[i].Item.Amount), static_cast<int32>(Source[i].Item.Amount)));
			ASSERT_THAT(AreEqual(static_cast<int32>(Loaded[i].Item.Level), static_cast<int32>(Source[i].Item.Level)));
		}
	}
};

// ============================================================================
// Attachment Tests
// ============================================================================

TEST_CLASS(ArcItemSerialization_Attachments, "ArcCore.Persistence")
{
	TEST_METHOD(ItemWithNoAttachments_EmptyArray)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		Source.Add(MakeHelper(FPrimaryAssetId("ArcItemDefinition:Sword")));

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 1));
		ASSERT_THAT(AreEqual(Loaded[0].ItemAttachments.Num(), 0));
	}

	TEST_METHOD(ItemWithAttachments_PreservesAttachmentCount)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		FArcItemCopyContainerHelper& Parent = Source.AddDefaulted_GetRef();
		Parent.Item.ItemId = FArcItemId::Generate();
		Parent.Item.ItemDefinitionId = FPrimaryAssetId("ArcItemDefinition:Sword");
		Parent.Item.Amount = 1;
		Parent.Item.Level = 3;
		Parent.ItemId = Parent.Item.ItemId;

		// Add two attachments
		FArcAttachedItemHelper Attachment1;
		Attachment1.Item.ItemId = FArcItemId::Generate();
		Attachment1.Item.ItemDefinitionId = FPrimaryAssetId("ArcItemDefinition:Gem_Red");
		Attachment1.Item.Amount = 1;
		Attachment1.Item.Level = 1;
		Attachment1.ItemId = Attachment1.Item.ItemId;
		Attachment1.SlotId = TAG_SerTest_Slot01;

		FArcAttachedItemHelper Attachment2;
		Attachment2.Item.ItemId = FArcItemId::Generate();
		Attachment2.Item.ItemDefinitionId = FPrimaryAssetId("ArcItemDefinition:Gem_Blue");
		Attachment2.Item.Amount = 1;
		Attachment2.Item.Level = 2;
		Attachment2.ItemId = Attachment2.Item.ItemId;
		Attachment2.SlotId = TAG_SerTest_Slot02;

		Parent.ItemAttachments.Add(Attachment1);
		Parent.ItemAttachments.Add(Attachment2);

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 1));
		ASSERT_THAT(AreEqual(Loaded[0].ItemAttachments.Num(), 2));
	}

	TEST_METHOD(ItemWithAttachments_PreservesAttachmentData)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		FArcItemCopyContainerHelper& Parent = Source.AddDefaulted_GetRef();
		Parent.Item.ItemId = FArcItemId::Generate();
		Parent.Item.ItemDefinitionId = FPrimaryAssetId("ArcItemDefinition:Sword");
		Parent.Item.Amount = 1;
		Parent.Item.Level = 1;
		Parent.ItemId = Parent.Item.ItemId;

		FArcAttachedItemHelper Attachment;
		Attachment.Item.ItemId = FArcItemId::Generate();
		Attachment.Item.ItemDefinitionId = FPrimaryAssetId("ArcItemDefinition:Enchantment");
		Attachment.Item.Amount = 3;
		Attachment.Item.Level = 7;
		Attachment.ItemId = Attachment.Item.ItemId;
		Attachment.SlotId = TAG_SerTest_Slot01;
		Parent.ItemAttachments.Add(Attachment);

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		const FArcAttachedItemHelper& LoadedAttachment = Loaded[0].ItemAttachments[0];
		ASSERT_THAT(IsTrue(LoadedAttachment.Item.ItemId == Attachment.Item.ItemId));
		ASSERT_THAT(IsTrue(LoadedAttachment.Item.GetItemDefinitionId() == FPrimaryAssetId("ArcItemDefinition:Enchantment")));
		ASSERT_THAT(AreEqual(static_cast<int32>(LoadedAttachment.Item.Amount), 3));
		ASSERT_THAT(AreEqual(static_cast<int32>(LoadedAttachment.Item.Level), 7));
		ASSERT_THAT(IsTrue(LoadedAttachment.SlotId == TAG_SerTest_Slot01));
	}
};

// ============================================================================
// Persistent Instance Tests
// ============================================================================

TEST_CLASS(ArcItemSerialization_PersistentInstances, "ArcCore.Persistence")
{
	TEST_METHOD(ItemWithPersistentInstance_PreservesInstanceType)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		FArcItemCopyContainerHelper& Helper = Source.AddDefaulted_GetRef();
		Helper.Item.ItemId = FArcItemId::Generate();
		Helper.Item.ItemDefinitionId = FPrimaryAssetId("ArcItemDefinition:StackablePotion");
		Helper.Item.Amount = 5;
		Helper.Item.Level = 1;
		Helper.ItemId = Helper.Item.ItemId;

		// Add a Stacks persistent instance
		FInstancedStruct StacksInstance = FInstancedStruct::Make<FArcItemInstance_Stacks>();
		StacksInstance.GetMutablePtr<FArcItemInstance_Stacks>()->SetStacks(10);
		Helper.Item.InitialInstanceData.Add(MoveTemp(StacksInstance));

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 1));
		ASSERT_THAT(AreEqual(Loaded[0].Item.InitialInstanceData.Num(), 1));
		ASSERT_THAT(IsTrue(Loaded[0].Item.InitialInstanceData[0].IsValid()));
		ASSERT_THAT(IsTrue(
			Loaded[0].Item.InitialInstanceData[0].GetScriptStruct() ==
			FArcItemInstance_Stacks::StaticStruct()));
	}

	TEST_METHOD(ItemWithNoPersistentInstances_EmptyArray)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		Source.Add(MakeHelper(FPrimaryAssetId("ArcItemDefinition:BasicItem")));

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 1));
		ASSERT_THAT(AreEqual(Loaded[0].Item.InitialInstanceData.Num(), 0));
	}

	TEST_METHOD(NonPersistentInstance_IsSkipped)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		FArcItemCopyContainerHelper& Helper = Source.AddDefaulted_GetRef();
		Helper.Item.ItemId = FArcItemId::Generate();
		Helper.Item.ItemDefinitionId = FPrimaryAssetId("ArcItemDefinition:BasicItem");
		Helper.Item.Amount = 1;
		Helper.Item.Level = 1;
		Helper.ItemId = Helper.Item.ItemId;

		// Add a base FArcItemInstance which has ShouldPersist() = false
		FInstancedStruct BaseInstance = FInstancedStruct::Make<FArcItemInstance>();
		Helper.Item.InitialInstanceData.Add(MoveTemp(BaseInstance));

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 1));
		// Non-persistent instance should be filtered out
		ASSERT_THAT(AreEqual(Loaded[0].Item.InitialInstanceData.Num(), 0));
	}

	TEST_METHOD(MixedPersistentAndNonPersistent_OnlyPersistentSurvive)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		FArcItemCopyContainerHelper& Helper = Source.AddDefaulted_GetRef();
		Helper.Item.ItemId = FArcItemId::Generate();
		Helper.Item.ItemDefinitionId = FPrimaryAssetId("ArcItemDefinition:MixedItem");
		Helper.Item.Amount = 1;
		Helper.Item.Level = 1;
		Helper.ItemId = Helper.Item.ItemId;

		// Non-persistent base instance
		Helper.Item.InitialInstanceData.Add(FInstancedStruct::Make<FArcItemInstance>());
		// Persistent stacks instance
		FInstancedStruct Stacks = FInstancedStruct::Make<FArcItemInstance_Stacks>();
		Stacks.GetMutablePtr<FArcItemInstance_Stacks>()->SetStacks(5);
		Helper.Item.InitialInstanceData.Add(MoveTemp(Stacks));

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		ASSERT_THAT(AreEqual(Loaded.Num(), 1));
		ASSERT_THAT(AreEqual(Loaded[0].Item.InitialInstanceData.Num(), 1));
		ASSERT_THAT(IsTrue(
			Loaded[0].Item.InitialInstanceData[0].GetScriptStruct() ==
			FArcItemInstance_Stacks::StaticStruct()));
	}
};

// ============================================================================
// Version & Data Integrity Tests
// ============================================================================

TEST_CLASS(ArcItemSerialization_DataIntegrity, "ArcCore.Persistence")
{
	TEST_METHOD(SavedData_IsNonEmpty)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		Source.Add(MakeHelper(FPrimaryAssetId("ArcItemDefinition:TestItem")));

		TArray<uint8> Data = SaveToBytes(Source);
		ASSERT_THAT(IsTrue(Data.Num() > 0));
	}

	TEST_METHOD(EmptyData_LoadsEmpty)
	{
		FArcJsonLoadArchive LoadAr;
		// Empty/invalid data should not crash
		TArray<uint8> EmptyData;
		bool bInit = LoadAr.InitializeFromData(EmptyData);

		TArray<FArcItemCopyContainerHelper> Result;
		if (bInit)
		{
			FArcItemStoreSerializer::Load(Result, LoadAr);
		}

		ASSERT_THAT(AreEqual(Result.Num(), 0));
	}

	TEST_METHOD(MultipleRoundTrips_AreStable)
	{
		TArray<FArcItemCopyContainerHelper> Source;
		Source.Add(MakeHelper(FPrimaryAssetId("ArcItemDefinition:Sword"), 3, 5, TAG_SerTest_Slot01));

		// Round-trip twice
		TArray<FArcItemCopyContainerHelper> First = RoundTrip(Source);
		TArray<FArcItemCopyContainerHelper> Second = RoundTrip(First);

		ASSERT_THAT(AreEqual(Second.Num(), 1));
		ASSERT_THAT(IsTrue(Second[0].Item.ItemId == Source[0].Item.ItemId));
		ASSERT_THAT(IsTrue(Second[0].Item.GetItemDefinitionId() == Source[0].Item.GetItemDefinitionId()));
		ASSERT_THAT(AreEqual(static_cast<int32>(Second[0].Item.Amount), 3));
		ASSERT_THAT(AreEqual(static_cast<int32>(Second[0].Item.Level), 5));
		ASSERT_THAT(IsTrue(Second[0].SlotId == TAG_SerTest_Slot01));
	}

	TEST_METHOD(ComplexScene_FullRoundTrip)
	{
		TArray<FArcItemCopyContainerHelper> Source;

		// Item 1: weapon with attachment and slot
		{
			FArcItemCopyContainerHelper& Weapon = Source.AddDefaulted_GetRef();
			Weapon.Item.ItemId = FArcItemId::Generate();
			Weapon.Item.ItemDefinitionId = FPrimaryAssetId("ArcItemDefinition:Sword");
			Weapon.Item.Amount = 1;
			Weapon.Item.Level = 10;
			Weapon.ItemId = Weapon.Item.ItemId;
			Weapon.SlotId = TAG_SerTest_Slot01;

			FArcAttachedItemHelper Gem;
			Gem.Item.ItemId = FArcItemId::Generate();
			Gem.Item.ItemDefinitionId = FPrimaryAssetId("ArcItemDefinition:Gem");
			Gem.Item.Amount = 1;
			Gem.Item.Level = 1;
			Gem.ItemId = Gem.Item.ItemId;
			Gem.SlotId = TAG_SerTest_Slot02;
			Weapon.ItemAttachments.Add(Gem);
		}

		// Item 2: stackable consumable with persistent stacks instance
		{
			FArcItemCopyContainerHelper& Potion = Source.AddDefaulted_GetRef();
			Potion.Item.ItemId = FArcItemId::Generate();
			Potion.Item.ItemDefinitionId = FPrimaryAssetId("ArcItemDefinition:Potion");
			Potion.Item.Amount = 20;
			Potion.Item.Level = 1;
			Potion.ItemId = Potion.Item.ItemId;

			FInstancedStruct Stacks = FInstancedStruct::Make<FArcItemInstance_Stacks>();
			Stacks.GetMutablePtr<FArcItemInstance_Stacks>()->SetStacks(20);
			Potion.Item.InitialInstanceData.Add(MoveTemp(Stacks));
		}

		// Item 3: simple material
		{
			Source.Add(MakeHelper(FPrimaryAssetId("ArcItemDefinition:IronOre"), 50, 1));
		}

		TArray<FArcItemCopyContainerHelper> Loaded = RoundTrip(Source);

		// Verify counts
		ASSERT_THAT(AreEqual(Loaded.Num(), 3));

		// Item 1: weapon
		ASSERT_THAT(IsTrue(Loaded[0].Item.ItemId == Source[0].Item.ItemId));
		ASSERT_THAT(AreEqual(static_cast<int32>(Loaded[0].Item.Level), 10));
		ASSERT_THAT(IsTrue(Loaded[0].SlotId == TAG_SerTest_Slot01));
		ASSERT_THAT(AreEqual(Loaded[0].ItemAttachments.Num(), 1));
		ASSERT_THAT(IsTrue(Loaded[0].ItemAttachments[0].SlotId == TAG_SerTest_Slot02));

		// Item 2: potion with stacks
		ASSERT_THAT(IsTrue(Loaded[1].Item.ItemId == Source[1].Item.ItemId));
		ASSERT_THAT(AreEqual(static_cast<int32>(Loaded[1].Item.Amount), 20));
		ASSERT_THAT(AreEqual(Loaded[1].Item.InitialInstanceData.Num(), 1));

		// Item 3: material
		ASSERT_THAT(IsTrue(Loaded[2].Item.ItemId == Source[2].Item.ItemId));
		ASSERT_THAT(AreEqual(static_cast<int32>(Loaded[2].Item.Amount), 50));
	}
};

// ============================================================================
// Store-to-Store Serialization Tests
// ============================================================================

TEST_CLASS(ArcItemSerialization_StoreToStore, "ArcCore.Persistence")
{
	FActorTestSpawner Spawner;

	TObjectPtr<UArcItemDefinition> SwordDef;
	TObjectPtr<UArcItemDefinition> ShieldDef;
	TObjectPtr<UArcItemDefinition> GemDef;

	TMap<FPrimaryAssetId, const UArcItemDefinition*> DefMap;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		SwordDef = SerializationTestHelpers::CreateTransientItemDef(TEXT("SerTest_Sword"));
		ShieldDef = SerializationTestHelpers::CreateTransientItemDef(TEXT("SerTest_Shield"));
		GemDef = SerializationTestHelpers::CreateTransientItemDef(TEXT("SerTest_Gem"));

		DefMap.Add(SwordDef->GetPrimaryAssetId(), SwordDef);
		DefMap.Add(ShieldDef->GetPrimaryAssetId(), ShieldDef);
		DefMap.Add(GemDef->GetPrimaryAssetId(), GemDef);
	}

	TEST_METHOD(SingleItem_SerializeFromStore_DeserializeToOtherStore)
	{
		FActorSpawnParameters Params;
		Params.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(Params);

		UArcItemsStoreComponent* SourceStore = Actor.ItemsStore;
		UArcTestItemsStoreComponent* DestStore = Actor.ItemsStore2;

		// Add item to source store
		FArcItemSpec Spec = SerializationTestHelpers::MakeSpec(SwordDef, 1, 3);
		FArcItemId OriginalId = SourceStore->AddItem(Spec, FArcItemId::InvalidId);
		ASSERT_THAT(IsTrue(OriginalId.IsValid()));

		// Serialize from source
		TArray<FArcItemCopyContainerHelper> SourceHelpers = SourceStore->GetAllInternalItems();
		TArray<uint8> Data = SaveToBytes(SourceHelpers);

		// Deserialize and add to dest
		TArray<FArcItemCopyContainerHelper> LoadedHelpers = LoadFromBytes(Data);
		SerializationTestHelpers::PatchDefinitions(LoadedHelpers, DefMap);

		ASSERT_THAT(AreEqual(LoadedHelpers.Num(), 1));
		for (FArcItemCopyContainerHelper& Helper : LoadedHelpers)
		{
			Helper.AddItems(DestStore);
		}

		// Verify dest store
		ASSERT_THAT(AreEqual(DestStore->GetItemNum(), 1));
		const FArcItemData* LoadedItem = DestStore->GetItemPtr(OriginalId);
		ASSERT_THAT(IsNotNull(LoadedItem));
		ASSERT_THAT(AreEqual(static_cast<int32>(LoadedItem->GetLevel()), 3));
	}

	TEST_METHOD(MultipleItems_SerializeFromStore_DeserializeToOtherStore)
	{
		FActorSpawnParameters Params;
		Params.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(Params);

		UArcItemsStoreComponent* SourceStore = Actor.ItemsStore;
		UArcTestItemsStoreComponent* DestStore = Actor.ItemsStore2;

		// Add multiple items
		FArcItemId SwordId = SourceStore->AddItem(
			SerializationTestHelpers::MakeSpec(SwordDef, 1, 5), FArcItemId::InvalidId);
		FArcItemId ShieldId = SourceStore->AddItem(
			SerializationTestHelpers::MakeSpec(ShieldDef, 2, 1), FArcItemId::InvalidId);

		ASSERT_THAT(AreEqual(SourceStore->GetItemNum(), 2));

		// Serialize → Deserialize → Add to dest
		TArray<FArcItemCopyContainerHelper> SourceHelpers = SourceStore->GetAllInternalItems();
		TArray<uint8> Data = SaveToBytes(SourceHelpers);
		TArray<FArcItemCopyContainerHelper> LoadedHelpers = LoadFromBytes(Data);
		SerializationTestHelpers::PatchDefinitions(LoadedHelpers, DefMap);

		for (FArcItemCopyContainerHelper& Helper : LoadedHelpers)
		{
			Helper.AddItems(DestStore);
		}

		// Verify dest store has both items
		ASSERT_THAT(AreEqual(DestStore->GetItemNum(), 2));

		const FArcItemData* LoadedSword = DestStore->GetItemPtr(SwordId);
		ASSERT_THAT(IsNotNull(LoadedSword));
		ASSERT_THAT(AreEqual(static_cast<int32>(LoadedSword->GetLevel()), 5));

		const FArcItemData* LoadedShield = DestStore->GetItemPtr(ShieldId);
		ASSERT_THAT(IsNotNull(LoadedShield));
		ASSERT_THAT(AreEqual(static_cast<int32>(LoadedShield->GetStacks()), 2));
	}

	TEST_METHOD(ItemWithAttachment_SerializeFromStore_DeserializeToOtherStore)
	{
		FActorSpawnParameters Params;
		Params.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(Params);

		UArcItemsStoreComponent* SourceStore = Actor.ItemsStore;
		UArcTestItemsStoreComponent* DestStore = Actor.ItemsStore2;

		// Add parent item
		FArcItemSpec SwordSpec = SerializationTestHelpers::MakeSpec(SwordDef, 1, 4);
		FArcItemId SwordId = SourceStore->AddItem(SwordSpec, FArcItemId::InvalidId);

		// Add attachment to the sword
		FArcItemSpec GemSpec = SerializationTestHelpers::MakeSpec(GemDef, 1, 1);
		FArcItemId GemId = SourceStore->AddItem(GemSpec, SwordId);
		SourceStore->InternalAttachToItem(SwordId, GemId, TAG_SerTest_Slot01);

		// Source should have 2 items total (parent + attachment)
		ASSERT_THAT(AreEqual(SourceStore->GetItemNum(), 2));

		// Serialize → Deserialize → Add to dest
		TArray<FArcItemCopyContainerHelper> SourceHelpers = SourceStore->GetAllInternalItems();
		// GetAllInternalItems skips owned items (attachments), so we get 1 helper with 1 attachment
		ASSERT_THAT(AreEqual(SourceHelpers.Num(), 1));
		ASSERT_THAT(AreEqual(SourceHelpers[0].ItemAttachments.Num(), 1));

		TArray<uint8> Data = SaveToBytes(SourceHelpers);
		TArray<FArcItemCopyContainerHelper> LoadedHelpers = LoadFromBytes(Data);
		SerializationTestHelpers::PatchDefinitions(LoadedHelpers, DefMap);

		for (FArcItemCopyContainerHelper& Helper : LoadedHelpers)
		{
			Helper.AddItems(DestStore);
		}

		// Verify dest store: parent + attachment = 2 items
		ASSERT_THAT(AreEqual(DestStore->GetItemNum(), 2));

		const FArcItemData* LoadedSword = DestStore->GetItemPtr(SwordId);
		ASSERT_THAT(IsNotNull(LoadedSword));

		const FArcItemData* LoadedGem = DestStore->GetItemPtr(GemId);
		ASSERT_THAT(IsNotNull(LoadedGem));
		ASSERT_THAT(IsTrue(LoadedGem->GetOwnerId() == SwordId));
	}

	TEST_METHOD(DestStoreIsIndependent_SourceUnaffected)
	{
		FActorSpawnParameters Params;
		Params.Name = *FGuid::NewGuid().ToString();
		AArcItemsTestActor& Actor = Spawner.SpawnActor<AArcItemsTestActor>(Params);

		UArcItemsStoreComponent* SourceStore = Actor.ItemsStore;
		UArcTestItemsStoreComponent* DestStore = Actor.ItemsStore2;

		// Add item to source
		FArcItemId SwordId = SourceStore->AddItem(
			SerializationTestHelpers::MakeSpec(SwordDef, 1, 1), FArcItemId::InvalidId);

		// Serialize from source
		TArray<FArcItemCopyContainerHelper> SourceHelpers = SourceStore->GetAllInternalItems();
		TArray<uint8> Data = SaveToBytes(SourceHelpers);
		TArray<FArcItemCopyContainerHelper> LoadedHelpers = LoadFromBytes(Data);
		SerializationTestHelpers::PatchDefinitions(LoadedHelpers, DefMap);

		for (FArcItemCopyContainerHelper& Helper : LoadedHelpers)
		{
			Helper.AddItems(DestStore);
		}

		// Remove item from dest — source should be unaffected
		DestStore->DestroyItem(SwordId);
		ASSERT_THAT(AreEqual(DestStore->GetItemNum(), 0));
		ASSERT_THAT(AreEqual(SourceStore->GetItemNum(), 1));

		const FArcItemData* SourceItem = SourceStore->GetItemPtr(SwordId);
		ASSERT_THAT(IsNotNull(SourceItem));
	}
};
