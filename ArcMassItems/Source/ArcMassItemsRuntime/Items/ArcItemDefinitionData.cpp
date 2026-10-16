// Copyright Lukasz Baran. All Rights Reserved.

#include "Items/ArcItemDefinitionData.h"

#include "AssetRegistry/AssetData.h"
#include "Core/ArcCoreAssetManager.h"
#include "GameplayTagsManager.h"
#include "Items/ArcItemScalableFloat.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "StructUtils/StructView.h"

namespace ArcItemDefinitionDataPrivate
{
	DEFINE_LOG_CATEGORY_STATIC(LogArcItemDefinitionData, Log, All);

	TSet<FPrimaryAssetId> WarnedIds;

	FArcJsonAssetTypeInfo MakeJsonTypeInfo()
	{
		FArcJsonAssetTypeInfo Info;
		Info.Extension = FArcItemDefinitionData::JsonExtension();
		Info.Struct = FArcItemDefinitionData::StaticStruct();
		Info.PrimaryAssetType = FArcItemDefinitionData::JsonPrimaryAssetType();
		Info.TypeProperty = GET_MEMBER_NAME_CHECKED(FArcItemDefinitionData, ItemType);
		Info.IdProperty = GET_MEMBER_NAME_CHECKED(FArcItemDefinitionData, ItemId);
		Info.DisplayName = NSLOCTEXT("ArcItemDefinitionData", "ItemJsonDisplayName", "Item (JSON)");
		Info.Color = FLinearColor(FColor(255, 157, 0));
		Info.AddAssetRegistryTags = [](const FInstancedStruct& Record, FAssetDataTagMap& Tags)
			{
				for (const FArcInstancedStruct& Element : Record.Get<FArcItemDefinitionData>().GetFragmentSet())
				{
					if (const FArcItemFragment* Fragment = Element.GetPtr<FArcItemFragment>())
					{
						Fragment->GetAssetRegistryTags(Tags);
					}
				}
			};
		return Info;
	}

	FGameplayTagContainer GetTagsFromAssetData(const FAssetData& AssetData, FName KeyName)
	{
		FString Tags;
		AssetData.GetTagValue<FString>(KeyName, Tags);

		TArray<FString> SplitTags;
		Tags.ParseIntoArray(SplitTags, TEXT(","));

		FGameplayTagContainer OutTags;
		UGameplayTagsManager::Get().RequestGameplayTagContainer(SplitTags, OutTags);
		return OutTags;
	}
}

const FArcJsonAssetTypeInfo& FArcItemDefinitionData::GetJsonTypeInfo()
{
	static const FArcJsonAssetTypeInfo Info = ArcItemDefinitionDataPrivate::MakeJsonTypeInfo();
	return Info;
}

const FArcItemDefinitionData* FArcItemDefinitionData::Find(const FPrimaryAssetId& Id)
{
	if (!Id.IsValid())
	{
		return nullptr;
	}

	const FArcItemDefinitionData* Record = UArcCoreAssetManager::Get().FindJsonRecord<FArcItemDefinitionData>(Id);
	if (Record == nullptr)
	{
		bool bAlreadyWarned = false;
		ArcItemDefinitionDataPrivate::WarnedIds.Add(Id, &bAlreadyWarned);
		if (!bAlreadyWarned)
		{
			UE_LOGF(ArcItemDefinitionDataPrivate::LogArcItemDefinitionData, Warning, "ArcMassItems: item record %ls not found", *Id.ToString());
		}
	}
	return Record;
}

FPrimaryAssetId FArcItemDefinitionData::GetPrimaryAssetId() const
{
	return ArcJsonAssets::MakeRecordId(GetJsonTypeInfo(), FConstStructView::Make(*this));
}

const uint8* FArcItemDefinitionData::GetFragment(const UScriptStruct* InStructType) const
{
	if (const FArcInstancedStruct* IS = FragmentSet.FindByHash(GetTypeHash(InStructType->GetFName()), InStructType->GetFName()))
	{
		return IS->GetMemory();
	}
	return nullptr;
}

const FArcScalableFloatItemFragment* FArcItemDefinitionData::GetScalableFloatFragment(const UScriptStruct* InStructType) const
{
	if (const FArcInstancedStruct* IS = ScalableFloatFragmentSet.FindByHash(GetTypeHash(InStructType->GetFName()), InStructType->GetFName()))
	{
		return IS->GetPtr<FArcScalableFloatItemFragment>();
	}
	return nullptr;
}

float FArcItemDefinitionData::GetValue(const FArcScalableCurveFloat& InScalableFloat) const
{
	if (const FArcScalableFloatItemFragment* Found = GetScalableFloatFragment(InScalableFloat.GetOwnerType()))
	{
		return InScalableFloat.GetValue(Found, 1.f);
	}
	return 0.f;
}

FGameplayTagContainer FArcItemDefinitionData::GetItemTags(const FAssetData& AssetData)
{
	return ArcItemDefinitionDataPrivate::GetTagsFromAssetData(AssetData, TEXT("ItemTags"));
}

FGameplayTagContainer FArcItemDefinitionData::GetAssetTags(const FAssetData& AssetData)
{
	return ArcItemDefinitionDataPrivate::GetTagsFromAssetData(AssetData, TEXT("AssetTags"));
}

FGameplayTagContainer FArcItemDefinitionData::GetGrantedTags(const FAssetData& AssetData)
{
	return ArcItemDefinitionDataPrivate::GetTagsFromAssetData(AssetData, TEXT("GrantedTags"));
}

FGameplayTagContainer FArcItemDefinitionData::GetRequiredTags(const FAssetData& AssetData)
{
	return ArcItemDefinitionDataPrivate::GetTagsFromAssetData(AssetData, TEXT("RequiredTags"));
}

FGameplayTagContainer FArcItemDefinitionData::GetDenyTags(const FAssetData& AssetData)
{
	return ArcItemDefinitionDataPrivate::GetTagsFromAssetData(AssetData, TEXT("DenyTags"));
}
