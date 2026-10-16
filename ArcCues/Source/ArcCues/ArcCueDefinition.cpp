// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCueDefinition.h"

#include "AssetRegistry/AssetData.h"
#include "GameplayTagsManager.h"

const FName UArcCueDefinition::CueTagName = TEXT("CueTag");
const FName UArcCueDefinition::FilterTagsName = TEXT("FilterTags");

FPrimaryAssetId UArcCueDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ArcCueDefinition"), GetFName());
}

void UArcCueDefinition::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	// CueTag — single tag, written as its plain string.
	Context.AddTag(UObject::FAssetRegistryTag(
		CueTagName,
		CueTag.IsValid() ? CueTag.ToString() : FString(),
		UObject::FAssetRegistryTag::TT_Hidden));

	// FilterTags — comma-separated list, mirroring FArcItemFragment_Tags emission shape.
	{
		FString TagsStr;
		const int32 Num = FilterTags.Num();
		int32 Idx = 0;
		for (const FGameplayTag& Tag : FilterTags)
		{
			TagsStr += Tag.ToString();
			Idx++;
			if (Num > Idx)
			{
				TagsStr += TEXT(",");
			}
		}
		Context.AddTag(UObject::FAssetRegistryTag(
			FilterTagsName,
			TagsStr,
			UObject::FAssetRegistryTag::TT_Hidden));
	}
}

FGameplayTag UArcCueDefinition::GetCueTagFromAssetData(const FAssetData& AssetData)
{
	FString TagStr;
	AssetData.GetTagValue<FString>(CueTagName, TagStr);
	if (TagStr.IsEmpty())
	{
		return FGameplayTag();
	}
	return UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagStr), /*ErrorIfNotFound*/ false);
}

FGameplayTagContainer UArcCueDefinition::GetFilterTagsFromAssetData(const FAssetData& AssetData)
{
	FString TagsStr;
	AssetData.GetTagValue<FString>(FilterTagsName, TagsStr);

	TArray<FString> Split;
	TagsStr.ParseIntoArray(Split, TEXT(","));

	FGameplayTagContainer Out;
	UGameplayTagsManager::Get().RequestGameplayTagContainer(Split, Out);
	return Out;
}
