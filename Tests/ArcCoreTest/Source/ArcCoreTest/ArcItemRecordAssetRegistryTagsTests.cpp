// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcItemTestRecordFiles.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Core/ArcCoreAssetManager.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemAr_Item, "Test.ItemAr.Item");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemAr_Asset, "Test.ItemAr.Asset");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemAr_Granted, "Test.ItemAr.Granted");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemAr_Required, "Test.ItemAr.Required");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemAr_Deny, "Test.ItemAr.Deny");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemAr_EditedItem, "Test.ItemAr.EditedItem");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemAr_EditedAsset, "Test.ItemAr.EditedAsset");

namespace ArcItemRecordArTagsTestsImpl
{
	bool FindAssetData(const FPrimaryAssetId& Id, FAssetData& OutAssetData)
	{
		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (AssetRegistry == nullptr)
		{
			return false;
		}

		TArray<FAssetData> Assets;
		AssetRegistry->GetAssetsByClass(FArcItemDefinitionData::StaticStruct()->GetStructPathName(), Assets, false);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.GetPrimaryAssetId() == Id)
			{
				OutAssetData = Asset;
				return true;
			}
		}
		return false;
	}

	FArcItemDefinitionData MakeTaggedRecord()
	{
		FArcItemDefinitionData Record;

		FArcItemFragment_Tags TagsFragment;
		TagsFragment.ItemTags.AddTag(TAG_ItemAr_Item);
		TagsFragment.AssetTags.AddTag(TAG_ItemAr_Asset);
		TagsFragment.GrantedTags.AddTag(TAG_ItemAr_Granted);
		TagsFragment.RequiredTags.AddTag(TAG_ItemAr_Required);
		TagsFragment.DenyTags.AddTag(TAG_ItemAr_Deny);
		ArcItemTestRecords::AddFragment(Record, TagsFragment);
		return Record;
	}
}

TEST_CLASS(ArcItemRecordAssetRegistryTags, "ArcCore.JsonIndex.ItemAssetRegistryTags")
{
	FArcItemTestRecordFiles Records;

	TEST_METHOD(TagsFragment_RecordFoundByStructClassPathWithAllFiveTagSets)
	{
		const FPrimaryAssetId Id = Records.Write(ArcItemRecordArTagsTestsImpl::MakeTaggedRecord());
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));

		FAssetData AssetData;
		ASSERT_THAT(IsTrue(ArcItemRecordArTagsTestsImpl::FindAssetData(Id, AssetData), TEXT("Asset Registry query by the record struct path must find the record")));
		ASSERT_THAT(IsTrue(UArcCoreAssetManager::Get().GetPrimaryAssetPath(Id).IsValid(), TEXT("The asset manager must confirm the id")));

		const FGameplayTagContainer ItemTags = FArcItemDefinitionData::GetItemTags(AssetData);
		const FGameplayTagContainer AssetTags = FArcItemDefinitionData::GetAssetTags(AssetData);
		const FGameplayTagContainer GrantedTags = FArcItemDefinitionData::GetGrantedTags(AssetData);
		const FGameplayTagContainer RequiredTags = FArcItemDefinitionData::GetRequiredTags(AssetData);
		const FGameplayTagContainer DenyTags = FArcItemDefinitionData::GetDenyTags(AssetData);

		ASSERT_THAT(AreEqual(1, ItemTags.Num(), TEXT("ItemTags must carry exactly the record's tag")));
		ASSERT_THAT(IsTrue(ItemTags.HasTagExact(TAG_ItemAr_Item), TEXT("ItemTags must hold the item tag")));
		ASSERT_THAT(AreEqual(1, AssetTags.Num(), TEXT("AssetTags must carry exactly the record's tag")));
		ASSERT_THAT(IsTrue(AssetTags.HasTagExact(TAG_ItemAr_Asset), TEXT("AssetTags must hold the asset tag")));
		ASSERT_THAT(IsTrue(GrantedTags.HasTagExact(TAG_ItemAr_Granted), TEXT("GrantedTags must hold the granted tag")));
		ASSERT_THAT(IsTrue(RequiredTags.HasTagExact(TAG_ItemAr_Required), TEXT("RequiredTags must hold the required tag")));
		ASSERT_THAT(IsTrue(DenyTags.HasTagExact(TAG_ItemAr_Deny), TEXT("DenyTags must hold the deny tag")));
	}

	TEST_METHOD(RecordWithoutTagsFragment_FoundWithoutTagKeys)
	{
		const FPrimaryAssetId Id = Records.Write(FArcItemDefinitionData());
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));

		FAssetData AssetData;
		ASSERT_THAT(IsTrue(ArcItemRecordArTagsTestsImpl::FindAssetData(Id, AssetData), TEXT("Asset Registry query by the record struct path must find the record")));

		FString AssetTagsValue;
		ASSERT_THAT(IsFalse(AssetData.GetTagValue<FString>(TEXT("AssetTags"), AssetTagsValue), TEXT("A record without a tags fragment must not carry the AssetTags key")));
		ASSERT_THAT(IsTrue(FArcItemDefinitionData::GetAssetTags(AssetData).IsEmpty(), TEXT("AssetTags reader must return empty")));
	}

	TEST_METHOD(ReindexAfterTagEdit_ReplacesAssetRegistryTags)
	{
		const FPrimaryAssetId Id = Records.Write(ArcItemRecordArTagsTestsImpl::MakeTaggedRecord());
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));

		FAssetData Before;
		ASSERT_THAT(IsTrue(ArcItemRecordArTagsTestsImpl::FindAssetData(Id, Before), TEXT("Record must be found before the edit")));
		ASSERT_THAT(IsTrue(FArcItemDefinitionData::GetAssetTags(Before).HasTagExact(TAG_ItemAr_Asset), TEXT("Initial AssetTags must hold the original tag")));

		const bool bRewritten = Records.Rewrite(Id, [](FArcItemDefinitionData& Record)
		{
			FArcItemFragment_Tags TagsFragment;
			TagsFragment.ItemTags.AddTag(TAG_ItemAr_EditedItem);
			TagsFragment.AssetTags.AddTag(TAG_ItemAr_EditedAsset);

			Record.FragmentSet.Reset();
			ArcItemTestRecords::AddFragment(Record, TagsFragment);
		});
		ASSERT_THAT(IsTrue(bRewritten, TEXT("Record file must be rewritten and reindexed")));

		FAssetData After;
		ASSERT_THAT(IsTrue(ArcItemRecordArTagsTestsImpl::FindAssetData(Id, After), TEXT("Record must still be found after the reindex")));

		const FGameplayTagContainer AssetTags = FArcItemDefinitionData::GetAssetTags(After);
		ASSERT_THAT(IsTrue(AssetTags.HasTagExact(TAG_ItemAr_EditedAsset), TEXT("AssetTags must hold the edited tag")));
		ASSERT_THAT(IsFalse(AssetTags.HasTagExact(TAG_ItemAr_Asset), TEXT("AssetTags must not keep the replaced tag")));
		ASSERT_THAT(IsTrue(FArcItemDefinitionData::GetItemTags(After).HasTagExact(TAG_ItemAr_EditedItem), TEXT("ItemTags must hold the edited tag")));
		ASSERT_THAT(IsTrue(FArcItemDefinitionData::GetGrantedTags(After).IsEmpty(), TEXT("GrantedTags removed from the file must be gone from the registry")));
		FString GrantedValue;
		ASSERT_THAT(IsFalse(After.GetTagValue<FString>(TEXT("GrantedTags"), GrantedValue), TEXT("An emptied tag set must leave no registry key")));
	}

	TEST_METHOD(RemovedRecord_IsNotConfirmedByAssetManager)
	{
		const FPrimaryAssetId Id = Records.Write(ArcItemRecordArTagsTestsImpl::MakeTaggedRecord());
		ASSERT_THAT(IsTrue(Id.IsValid(), TEXT("Item record must be written and indexed")));
		ASSERT_THAT(IsTrue(UArcCoreAssetManager::Get().GetPrimaryAssetPath(Id).IsValid(), TEXT("The asset manager must confirm the id before removal")));

		ASSERT_THAT(IsTrue(Records.Remove(Id), TEXT("Record file must be removed from the index")));

		ASSERT_THAT(IsFalse(UArcCoreAssetManager::Get().GetPrimaryAssetPath(Id).IsValid(), TEXT("A removed record must not be confirmed by the asset manager")));
		ASSERT_THAT(IsNull(UArcCoreAssetManager::Get().FindJsonRecord<FArcItemDefinitionData>(Id), TEXT("A removed record must not resolve")));
	}
};
