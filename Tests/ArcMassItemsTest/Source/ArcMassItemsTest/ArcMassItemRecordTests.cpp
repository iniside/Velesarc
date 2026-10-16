// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcItemTestRecordFiles.h"
#include "ArcMassItemTestHelpers.h"
#include "Commands/ArcMassAddNewItemCommand.h"
#include "Commands/ArcMassAddNewItemToSlotCommand.h"
#include "Components/ActorTestSpawner.h"
#include "Core/ArcCoreAssetManager.h"
#include "Engine/World.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcMassItemRecordSyncSubsystem.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassEntitySubsystem.h"
#include "NativeGameplayTags.h"
#include "Operations/ArcMassItemOperations.h"
#include "Persistence/ArcItemSpecSerializer.h"
#include "Serialization/ArcJsonLoadArchive.h"
#include "Serialization/ArcJsonSaveArchive.h"
#include "StructUtils/InstancedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemRec_TagA, "Test.ItemRec.TagA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemRec_TagB, "Test.ItemRec.TagB");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemRec_ScalarA, "Test.ItemRec.ScalarA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemRec_ScalarB, "Test.ItemRec.ScalarB");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemRec_Slot, "Test.Item.Slot.Primary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemRec_Socket, "Test.Item.Socket.Barrel");

namespace ArcMassItemRecordTestsImpl
{
	FArcItemDefinitionData MakeRecord(const FGameplayTag& ItemTag, const FGameplayTag& ScalarTag)
	{
		FArcItemDefinitionData Record;

		if (ItemTag.IsValid())
		{
			FArcItemFragment_Tags TagsFragment;
			TagsFragment.ItemTags.AddTag(ItemTag);
			ArcItemTestRecords::AddFragment(Record, TagsFragment);
		}

		if (ScalarTag.IsValid())
		{
			FArcScalableFloatItemFragment_Test ScalableFragment;
			ScalableFragment.DataName = ScalarTag;
			ArcItemTestRecords::AddScalableFragment(Record, ScalableFragment);
		}
		return Record;
	}

	FPrimaryAssetId MakeUnindexedId()
	{
		return FPrimaryAssetId(FArcItemDefinitionData::JsonPrimaryAssetType(), FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	const FArcItemDefinitionData* FindRecord(const FPrimaryAssetId& Id)
	{
		return UArcCoreAssetManager::Get().FindJsonRecord<FArcItemDefinitionData>(Id);
	}

	const FArcItemData* FindItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemId& ItemId)
	{
		return ArcMassItems::FindItemById(EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId);
	}

	FArcItemId AddItemOf(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FPrimaryAssetId& Id)
	{
		return ArcMassItems::AddItem(EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), FArcItemSpec::NewItem(Id, 1, 1));
	}
}

TEST_CLASS(ArcMassItemRecordCreation, "ArcMassItems.Records.Creation")
{
	FArcItemTestRecordFiles Records;
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MassSubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MassSubsystem);
		EntityManager = &MassSubsystem->GetMutableEntityManager();

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		Entity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(Entity);
	}

	TEST_METHOD(AddItem_KnownId_ItemCachesThatRecord)
	{
		const FPrimaryAssetId Id = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(TAG_ItemRec_TagA, TAG_ItemRec_ScalarA));
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));
		const FArcItemDefinitionData* Record = ArcMassItemRecordTestsImpl::FindRecord(Id);
		ASSERT_THAT(IsNotNull(Record, TEXT("Record must resolve by id")));

		const FArcItemId ItemId = ArcMassItemRecordTestsImpl::AddItemOf(*EntityManager, Entity, Id);
		ASSERT_THAT(IsTrue(ItemId.IsValid(), TEXT("AddItem must return an item id")));

		const FArcItemData* Item = ArcMassItemRecordTestsImpl::FindItem(*EntityManager, Entity, ItemId);
		ASSERT_THAT(IsNotNull(Item, TEXT("Item must be in the store")));
		ASSERT_THAT(IsTrue(Item->GetItemDefinitionId() == Id, TEXT("Item must keep the record id")));
		ASSERT_THAT(IsTrue(Item->GetItemDefinition() == Record, TEXT("Item must cache the indexed record, not a copy")));
		ASSERT_THAT(IsTrue(Item->IsValid(), TEXT("Item with a resolved record must be valid")));
		ASSERT_THAT(IsTrue(Item->GetItemAggregatedTags().HasTagExact(TAG_ItemRec_TagA), TEXT("Aggregated tags must come from the record's tags fragment")));

		const FArcScalableFloatItemFragment_Test* Scalable = Item->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>();
		ASSERT_THAT(IsNotNull(Scalable, TEXT("Scalable cache must hold the record's fragment")));
		ASSERT_THAT(IsTrue(Scalable == Record->GetScalableFloatFragment<FArcScalableFloatItemFragment_Test>(), TEXT("Scalable cache must point into the record")));
	}

	TEST_METHOD(AddItem_UnknownId_ItemHasNoDefinitionAndIsInvalid)
	{
		const FPrimaryAssetId UnknownId = ArcMassItemRecordTestsImpl::MakeUnindexedId();
		ASSERT_THAT(IsNull(ArcMassItemRecordTestsImpl::FindRecord(UnknownId), TEXT("The id must not be indexed")));

		const FArcItemId ItemId = ArcMassItemRecordTestsImpl::AddItemOf(*EntityManager, Entity, UnknownId);
		ASSERT_THAT(IsTrue(ItemId.IsValid(), TEXT("AddItem creates the item entry for an unindexed id")));

		const FArcItemData* Item = ArcMassItemRecordTestsImpl::FindItem(*EntityManager, Entity, ItemId);
		ASSERT_THAT(IsNotNull(Item, TEXT("Item must be in the store")));
		ASSERT_THAT(IsTrue(Item->GetItemDefinitionId() == UnknownId, TEXT("Item keeps the requested id")));
		ASSERT_THAT(IsNull(Item->GetItemDefinition(), TEXT("No record is cached for an unindexed id")));
		ASSERT_THAT(IsFalse(Item->IsValid(), TEXT("Item without a resolved record is invalid")));
		ASSERT_THAT(IsTrue(Item->GetItemAggregatedTags().IsEmpty(), TEXT("Item without a record has no aggregated tags")));
		ASSERT_THAT(AreEqual(0, Item->ScalableFloatFragments.Num(), TEXT("Item without a record has no scalable fragments")));
	}

	TEST_METHOD(NewFromSpec_KnownId_CachesRecordAndGeneratesItemId)
	{
		const FPrimaryAssetId Id = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(TAG_ItemRec_TagA, FGameplayTag()));
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));

		FArcItemSpec Spec;
		Spec.SetItemDefinition(Id);
		Spec.Level = 4;

		FInstancedStruct Created = FArcItemData::NewFromSpec(Spec);
		const FArcItemData* Item = Created.GetPtr<FArcItemData>();
		ASSERT_THAT(IsNotNull(Item, TEXT("NewFromSpec must produce item data")));
		ASSERT_THAT(IsTrue(Item->ItemDefinition == ArcMassItemRecordTestsImpl::FindRecord(Id), TEXT("NewFromSpec must resolve the record from the spec id")));
		ASSERT_THAT(IsTrue(Item->ItemId.IsValid(), TEXT("NewFromSpec must generate an item id when the spec has none")));
		ASSERT_THAT(AreEqual(static_cast<uint8>(4), Item->GetLevel(), TEXT("NewFromSpec must carry the level")));
	}

	TEST_METHOD(NewFromSpec_UnknownId_HasNoDefinition)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(ArcMassItemRecordTestsImpl::MakeUnindexedId());

		FInstancedStruct Created = FArcItemData::NewFromSpec(Spec);
		const FArcItemData* Item = Created.GetPtr<FArcItemData>();
		ASSERT_THAT(IsNotNull(Item, TEXT("NewFromSpec must produce item data")));
		ASSERT_THAT(IsNull(Item->ItemDefinition, TEXT("An unindexed id must not resolve")));
	}

	TEST_METHOD(Spec_ResolvesRecordOnEachCall)
	{
		const FPrimaryAssetId Id = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(TAG_ItemRec_TagA, FGameplayTag()));
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));

		const FArcItemSpec Spec = FArcItemSpec::NewItem(Id, 1, 1);
		ASSERT_THAT(IsTrue(Spec.GetItemDefinition() == ArcMassItemRecordTestsImpl::FindRecord(Id), TEXT("Spec must resolve the indexed record")));

		ASSERT_THAT(IsTrue(Records.Remove(Id), TEXT("Record must be removed")));
		ASSERT_THAT(IsNull(Spec.GetItemDefinition(), TEXT("Spec stores no pointer: a removed record no longer resolves")));
	}
};

TEST_CLASS(ArcMassItemRecordCommands, "ArcMassItems.Records.Commands")
{
	FArcItemTestRecordFiles Records;
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MassSubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MassSubsystem);
		EntityManager = &MassSubsystem->GetMutableEntityManager();

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		Entity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(Entity);
	}

	int32 StoreItemCount() const
	{
		const FArcMassItemStoreFragment* Store = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
		return Store != nullptr ? Store->ReplicatedItems.Items.Num() : -1;
	}

	TEST_METHOD(AddNewItem_Execute_KnownId_AddsItemWithThatRecord)
	{
		const FPrimaryAssetId Id = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(TAG_ItemRec_TagA, FGameplayTag()));
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));

		FArcMassAddNewItemCommand Command(Entity, Id);
		ASSERT_THAT(IsTrue(Command.CanSendCommand(), TEXT("A command with an entity and an id can be sent")));
		ASSERT_THAT(IsTrue(Command.Execute(&Spawner.GetWorld()), TEXT("Execute must accept an indexed record id")));
		ASSERT_THAT(AreEqual(1, StoreItemCount(), TEXT("One item must be added")));

		const FArcItemData* Item = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>()->ReplicatedItems.Items[0].ToItem();
		ASSERT_THAT(IsNotNull(Item));
		ASSERT_THAT(IsTrue(Item->GetItemDefinitionId() == Id, TEXT("Added item must carry the command's record id")));
		ASSERT_THAT(IsTrue(Item->GetItemDefinition() == ArcMassItemRecordTestsImpl::FindRecord(Id), TEXT("Added item must cache the record")));
	}

	TEST_METHOD(AddNewItem_Execute_UnknownId_IsRejectedAndAddsNothing)
	{
		FArcMassAddNewItemCommand Command(Entity, ArcMassItemRecordTestsImpl::MakeUnindexedId());
		ASSERT_THAT(IsFalse(Command.Execute(&Spawner.GetWorld()), TEXT("Execute must reject an unindexed record id")));
		ASSERT_THAT(AreEqual(0, StoreItemCount(), TEXT("No item may be added for an unindexed id")));
	}

	TEST_METHOD(AddNewItem_CanSend_InvalidId_IsFalse)
	{
		FArcMassAddNewItemCommand Command(Entity, FPrimaryAssetId());
		ASSERT_THAT(IsFalse(Command.CanSendCommand(), TEXT("A command without an id must not be sent")));
	}

	TEST_METHOD(AddNewItemToSlot_Execute_KnownId_AddsItemInSlot)
	{
		const FPrimaryAssetId Id = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(TAG_ItemRec_TagA, FGameplayTag()));
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));

		FArcMassAddNewItemToSlotCommand Command(Entity, Id, TAG_ItemRec_Slot);
		ASSERT_THAT(IsTrue(Command.Execute(&Spawner.GetWorld()), TEXT("Execute must accept an indexed record id")));
		ASSERT_THAT(AreEqual(1, StoreItemCount(), TEXT("One item must be added")));

		const FArcItemData* Item = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemStoreFragment>()->ReplicatedItems.Items[0].ToItem();
		ASSERT_THAT(IsNotNull(Item));
		ASSERT_THAT(IsTrue(Item->GetItemDefinitionId() == Id, TEXT("Added item must carry the command's record id")));
		ASSERT_THAT(IsTrue(Item->GetSlotId() == TAG_ItemRec_Slot.GetTag(), TEXT("Added item must sit in the command's slot")));
	}

	TEST_METHOD(AddNewItemToSlot_Execute_UnknownId_IsRejectedAndAddsNothing)
	{
		FArcMassAddNewItemToSlotCommand Command(Entity, ArcMassItemRecordTestsImpl::MakeUnindexedId(), TAG_ItemRec_Slot);
		ASSERT_THAT(IsFalse(Command.Execute(&Spawner.GetWorld()), TEXT("Execute must reject an unindexed record id")));
		ASSERT_THAT(AreEqual(0, StoreItemCount(), TEXT("No item may be added for an unindexed id")));
	}
};

TEST_CLASS(ArcMassItemRecordSync, "ArcMassItems.Records.Sync")
{
	FArcItemTestRecordFiles Records;
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MassSubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MassSubsystem);
		EntityManager = &MassSubsystem->GetMutableEntityManager();

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		Entity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcMassItemStoreFragment>(Entity);
	}

	TEST_METHOD(WorldHasRecordSyncSubsystem)
	{
		ASSERT_THAT(IsNotNull(Spawner.GetWorld().GetSubsystem<UArcMassItemRecordSyncSubsystem>(), TEXT("The test world must create the record sync subsystem")));
	}

	TEST_METHOD(RewriteScalableFragment_ItemCacheReadsNewData)
	{
		const FPrimaryAssetId Id = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(FGameplayTag(), TAG_ItemRec_ScalarA));
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));
		const FArcItemId ItemId = ArcMassItemRecordTestsImpl::AddItemOf(*EntityManager, Entity, Id);
		const FArcItemData* Item = ArcMassItemRecordTestsImpl::FindItem(*EntityManager, Entity, ItemId);
		ASSERT_THAT(IsNotNull(Item, TEXT("Item must be in the store")));

		const FArcScalableFloatItemFragment_Test* Before = Item->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>();
		ASSERT_THAT(IsNotNull(Before, TEXT("Scalable cache must be built on add")));
		ASSERT_THAT(IsTrue(Before->DataName == TAG_ItemRec_ScalarA.GetTag(), TEXT("Scalable cache must start with the original data")));
		ASSERT_THAT(AreEqual(1, Item->ScalableFloatFragments.Num(), TEXT("Scalable cache must start with one fragment type")));
		ASSERT_THAT(IsNull(Item->FindScalableItemFragment<FArcScalableFloatItemFragment_TestB>(), TEXT("The second fragment type must not exist before the rewrite")));

		const bool bRewritten = Records.Rewrite(Id, [](FArcItemDefinitionData& Record)
		{
			Record.ScalableFloatFragmentSet.Reset();
			FArcScalableFloatItemFragment_Test Replacement;
			Replacement.DataName = TAG_ItemRec_ScalarB;
			ArcItemTestRecords::AddScalableFragment(Record, Replacement);
			FArcScalableFloatItemFragment_TestB Added;
			Added.DataName = TAG_ItemRec_ScalarA;
			ArcItemTestRecords::AddScalableFragment(Record, Added);
		});
		ASSERT_THAT(IsTrue(bRewritten, TEXT("Record file must be rewritten and reindexed")));

		const FArcItemDefinitionData* Record = ArcMassItemRecordTestsImpl::FindRecord(Id);
		ASSERT_THAT(IsNotNull(Record, TEXT("Record must still resolve after the reindex")));
		ASSERT_THAT(IsTrue(Item->GetItemDefinition() == Record, TEXT("Item must keep caching the record")));

		const FArcScalableFloatItemFragment_Test* After = Item->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>();
		ASSERT_THAT(IsNotNull(After, TEXT("Scalable cache must be rebuilt")));
		ASSERT_THAT(IsTrue(After == Record->GetScalableFloatFragment<FArcScalableFloatItemFragment_Test>(), TEXT("Scalable cache must point into the reloaded record")));
		ASSERT_THAT(IsTrue(After->DataName == TAG_ItemRec_ScalarB.GetTag(), TEXT("Scalable cache must read the rewritten data")));
		ASSERT_THAT(IsTrue(Item->ScalableFloatContext.ItemDefinition == Record, TEXT("Scalable float context must point at the record")));

		const FArcScalableFloatItemFragment_TestB* Added = Item->FindScalableItemFragment<FArcScalableFloatItemFragment_TestB>();
		ASSERT_THAT(IsNotNull(Added, TEXT("Scalable cache must gain the fragment type the rewrite added")));
		ASSERT_THAT(IsTrue(Added == Record->GetScalableFloatFragment<FArcScalableFloatItemFragment_TestB>(), TEXT("The added fragment must point into the reloaded record")));
		ASSERT_THAT(AreEqual(2, Item->ScalableFloatFragments.Num(), TEXT("Scalable cache must hold both fragment types")));
	}

	TEST_METHOD(RewriteWithoutScalableFragment_CacheEntryDropped)
	{
		const FPrimaryAssetId Id = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(FGameplayTag(), TAG_ItemRec_ScalarA));
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));
		const FArcItemId ItemId = ArcMassItemRecordTestsImpl::AddItemOf(*EntityManager, Entity, Id);
		const FArcItemData* Item = ArcMassItemRecordTestsImpl::FindItem(*EntityManager, Entity, ItemId);
		ASSERT_THAT(IsNotNull(Item, TEXT("Item must be in the store")));
		ASSERT_THAT(IsNotNull(Item->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>(), TEXT("Scalable cache must be built on add")));

		const bool bRewritten = Records.Rewrite(Id, [](FArcItemDefinitionData& Record)
		{
			Record.ScalableFloatFragmentSet.Reset();
		});
		ASSERT_THAT(IsTrue(bRewritten, TEXT("Record file must be rewritten and reindexed")));

		ASSERT_THAT(IsNull(Item->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>(), TEXT("Cache must not keep a pointer into the fragment the rewrite removed")));
		ASSERT_THAT(AreEqual(0, Item->ScalableFloatFragments.Num(), TEXT("Scalable cache must be empty")));
		ASSERT_THAT(IsTrue(Item->IsValid(), TEXT("Item must stay valid: the record still resolves")));
	}

	TEST_METHOD(RewriteTagsFragment_AggregatedTagsRebuilt)
	{
		const FPrimaryAssetId Id = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(TAG_ItemRec_TagA, FGameplayTag()));
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));
		const FArcItemId ItemId = ArcMassItemRecordTestsImpl::AddItemOf(*EntityManager, Entity, Id);
		const FArcItemData* Item = ArcMassItemRecordTestsImpl::FindItem(*EntityManager, Entity, ItemId);
		ASSERT_THAT(IsNotNull(Item, TEXT("Item must be in the store")));
		ASSERT_THAT(IsTrue(Item->GetItemAggregatedTags().HasTagExact(TAG_ItemRec_TagA), TEXT("Aggregated tags must start with the original tag")));

		const bool bRewritten = Records.Rewrite(Id, [](FArcItemDefinitionData& Record)
		{
			Record.FragmentSet.Reset();
			FArcItemFragment_Tags TagsFragment;
			TagsFragment.ItemTags.AddTag(TAG_ItemRec_TagB);
			ArcItemTestRecords::AddFragment(Record, TagsFragment);
		});
		ASSERT_THAT(IsTrue(bRewritten, TEXT("Record file must be rewritten and reindexed")));

		ASSERT_THAT(IsTrue(Item->GetItemAggregatedTags().HasTagExact(TAG_ItemRec_TagB), TEXT("Aggregated tags must hold the rewritten tag")));
		ASSERT_THAT(IsFalse(Item->GetItemAggregatedTags().HasTagExact(TAG_ItemRec_TagA), TEXT("Aggregated tags must drop the replaced tag")));
	}

	TEST_METHOD(RewriteSocketedItemRecord_OwnerMergedFragmentsRebuilt)
	{
		const FPrimaryAssetId OwnerRecordId = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(TAG_ItemRec_TagA, FGameplayTag()));
		const FPrimaryAssetId SocketRecordId = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(FGameplayTag(), TAG_ItemRec_ScalarA));
		ASSERT_THAT(IsTrue(OwnerRecordId.IsValid() && SocketRecordId.IsValid(), TEXT("Item records must be written and indexed")));

		const FArcItemId OwnerId = ArcMassItemRecordTestsImpl::AddItemOf(*EntityManager, Entity, OwnerRecordId);
		const FArcItemId SocketId = ArcMassItemRecordTestsImpl::AddItemOf(*EntityManager, Entity, SocketRecordId);
		ASSERT_THAT(IsTrue(ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, SocketId, TAG_ItemRec_Socket), TEXT("Socket item must attach")));

		const FArcItemData* Owner = ArcMassItemRecordTestsImpl::FindItem(*EntityManager, Entity, OwnerId);
		ASSERT_THAT(IsNotNull(Owner, TEXT("Owner must be in the store")));
		const FArcScalableFloatItemFragment_Test* Merged = Owner->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>();
		ASSERT_THAT(IsNotNull(Merged, TEXT("Owner must merge the socket item's scalable fragment")));
		ASSERT_THAT(IsTrue(Merged->DataName == TAG_ItemRec_ScalarA.GetTag(), TEXT("Merged fragment must start with the original data")));
		ASSERT_THAT(AreEqual(1, Owner->ScalableFloatFragments.Num(), TEXT("Owner must start with one merged fragment type")));
		ASSERT_THAT(IsNull(Owner->FindScalableItemFragment<FArcScalableFloatItemFragment_TestB>(), TEXT("The second fragment type must not exist before the rewrite")));

		const bool bRewritten = Records.Rewrite(SocketRecordId, [](FArcItemDefinitionData& Record)
		{
			Record.ScalableFloatFragmentSet.Reset();
			FArcScalableFloatItemFragment_Test Replacement;
			Replacement.DataName = TAG_ItemRec_ScalarB;
			ArcItemTestRecords::AddScalableFragment(Record, Replacement);
			FArcScalableFloatItemFragment_TestB Added;
			Added.DataName = TAG_ItemRec_ScalarA;
			ArcItemTestRecords::AddScalableFragment(Record, Added);
		});
		ASSERT_THAT(IsTrue(bRewritten, TEXT("Socket record file must be rewritten and reindexed")));

		const FArcItemDefinitionData* SocketRecord = ArcMassItemRecordTestsImpl::FindRecord(SocketRecordId);
		ASSERT_THAT(IsNotNull(SocketRecord, TEXT("Socket record must still resolve")));
		const FArcScalableFloatItemFragment_Test* After = Owner->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>();
		ASSERT_THAT(IsNotNull(After, TEXT("Owner must still merge the socket item's fragment")));
		ASSERT_THAT(IsTrue(After == SocketRecord->GetScalableFloatFragment<FArcScalableFloatItemFragment_Test>(), TEXT("Owner's merged fragment must point into the reloaded socket record")));
		ASSERT_THAT(IsTrue(After->DataName == TAG_ItemRec_ScalarB.GetTag(), TEXT("Owner's merged fragment must read the rewritten data")));

		const FArcScalableFloatItemFragment_TestB* Added = Owner->FindScalableItemFragment<FArcScalableFloatItemFragment_TestB>();
		ASSERT_THAT(IsNotNull(Added, TEXT("Owner's merged map must gain the fragment type the socket rewrite added")));
		ASSERT_THAT(IsTrue(Added == SocketRecord->GetScalableFloatFragment<FArcScalableFloatItemFragment_TestB>(), TEXT("The added merged fragment must point into the reloaded socket record")));
		ASSERT_THAT(AreEqual(2, Owner->ScalableFloatFragments.Num(), TEXT("Owner must merge both fragment types")));
	}

	TEST_METHOD(RemoveRecord_ItemCachesNulledWithoutCrash)
	{
		const FPrimaryAssetId Id = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(TAG_ItemRec_TagA, TAG_ItemRec_ScalarA));
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));
		const FArcItemId ItemId = ArcMassItemRecordTestsImpl::AddItemOf(*EntityManager, Entity, Id);
		const FArcItemData* Item = ArcMassItemRecordTestsImpl::FindItem(*EntityManager, Entity, ItemId);
		ASSERT_THAT(IsNotNull(Item, TEXT("Item must be in the store")));
		ASSERT_THAT(IsTrue(Item->IsValid(), TEXT("Item must be valid before the record is removed")));
		ASSERT_THAT(IsFalse(Item->GetItemAggregatedTags().IsEmpty(), TEXT("Aggregated tags must be built before the removal")));

		ASSERT_THAT(IsTrue(Records.Remove(Id), TEXT("Record must be removed from the index")));

		ASSERT_THAT(IsNull(Item->GetItemDefinition(), TEXT("Removed record must null the item's cache")));
		ASSERT_THAT(IsNull(Item->ScalableFloatContext.ItemDefinition, TEXT("Removed record must null the scalable float context")));
		ASSERT_THAT(AreEqual(0, Item->ScalableFloatFragments.Num(), TEXT("Removed record must clear the scalable cache")));
		ASSERT_THAT(IsTrue(Item->GetItemAggregatedTags().IsEmpty(), TEXT("Removed record must clear the aggregated tags")));
		ASSERT_THAT(IsFalse(Item->IsValid(), TEXT("Item without a record is invalid")));
		ASSERT_THAT(IsTrue(Item->GetItemDefinitionId() == Id, TEXT("Item keeps its id after the record is gone")));
	}

	TEST_METHOD(RemoveSocketedItemRecord_OwnerMergedFragmentsDropped)
	{
		const FPrimaryAssetId OwnerRecordId = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(TAG_ItemRec_TagA, FGameplayTag()));
		const FPrimaryAssetId SocketRecordId = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(FGameplayTag(), TAG_ItemRec_ScalarA));
		ASSERT_THAT(IsTrue(OwnerRecordId.IsValid() && SocketRecordId.IsValid(), TEXT("Item records must be written and indexed")));

		const FArcItemId OwnerId = ArcMassItemRecordTestsImpl::AddItemOf(*EntityManager, Entity, OwnerRecordId);
		const FArcItemId SocketId = ArcMassItemRecordTestsImpl::AddItemOf(*EntityManager, Entity, SocketRecordId);
		ASSERT_THAT(IsTrue(ArcMassItems::AttachItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), OwnerId, SocketId, TAG_ItemRec_Socket), TEXT("Socket item must attach")));

		const FArcItemData* Owner = ArcMassItemRecordTestsImpl::FindItem(*EntityManager, Entity, OwnerId);
		ASSERT_THAT(IsNotNull(Owner, TEXT("Owner must be in the store")));
		ASSERT_THAT(IsNotNull(Owner->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>(), TEXT("Owner must merge the socket item's fragment")));

		ASSERT_THAT(IsTrue(Records.Remove(SocketRecordId), TEXT("Socket record must be removed from the index")));

		ASSERT_THAT(IsNull(Owner->FindScalableItemFragment<FArcScalableFloatItemFragment_Test>(), TEXT("Owner must drop the fragment of the removed socket record")));
		ASSERT_THAT(IsNotNull(Owner->GetItemDefinition(), TEXT("Owner's own record is untouched")));
	}
};

TEST_CLASS(ArcMassItemRecordPersistence, "ArcMassItems.Records.Persistence")
{
	FArcItemTestRecordFiles Records;

	TEST_METHOD(SpecRoundTrip_KeepsDefinitionIdAndResolvesToRecord)
	{
		const FPrimaryAssetId Id = Records.Write(ArcMassItemRecordTestsImpl::MakeRecord(TAG_ItemRec_TagA, FGameplayTag()));
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));
		const FArcItemSpec Source = FArcItemSpec::NewItem(Id, 3, 7);

		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(FArcItemSpecSerializer::Version);
		FArcItemSpecSerializer::Save(Source, SaveAr);
		const TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data), TEXT("Saved bytes must load")));
		FArcItemSpec Loaded;
		FArcItemSpecSerializer::Load(Loaded, LoadAr);

		ASSERT_THAT(IsTrue(Loaded.GetItemDefinitionId() == Id, TEXT("Definition id must survive the round trip")));
		ASSERT_THAT(IsTrue(Loaded.ItemId == Source.ItemId, TEXT("Item id must survive the round trip")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(7), Loaded.Amount, TEXT("Amount must survive the round trip")));
		ASSERT_THAT(AreEqual(static_cast<uint8>(3), Loaded.Level, TEXT("Level must survive the round trip")));
		ASSERT_THAT(IsTrue(Loaded.GetItemDefinition() == ArcMassItemRecordTestsImpl::FindRecord(Id), TEXT("Loaded spec must resolve to the indexed record")));
	}

	TEST_METHOD(SpecRoundTrip_UnknownId_KeepsIdWithoutResolving)
	{
		FAutomationTestBase::bSuppressLogWarnings = true;

		const FPrimaryAssetId UnknownId = ArcMassItemRecordTestsImpl::MakeUnindexedId();
		const FArcItemSpec Source = FArcItemSpec::NewItem(UnknownId, 1, 1);

		FArcJsonSaveArchive SaveAr;
		SaveAr.SetVersion(FArcItemSpecSerializer::Version);
		FArcItemSpecSerializer::Save(Source, SaveAr);
		const TArray<uint8> Data = SaveAr.Finalize();

		FArcJsonLoadArchive LoadAr;
		ASSERT_THAT(IsTrue(LoadAr.InitializeFromData(Data), TEXT("Saved bytes must load")));
		FArcItemSpec Loaded;
		FArcItemSpecSerializer::Load(Loaded, LoadAr);

		ASSERT_THAT(IsTrue(Loaded.GetItemDefinitionId() == UnknownId, TEXT("The id of a missing record must survive the round trip")));
		ASSERT_THAT(IsNull(Loaded.GetItemDefinition(), TEXT("A missing record must not resolve after load")));
	}
};
