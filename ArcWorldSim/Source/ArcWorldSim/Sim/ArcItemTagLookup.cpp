// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcItemTagLookup.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"
#include "Items/ArcItemDefinitionData.h"

void ArcWorldSim::CollectItemIdsForAssetTags(
	const FGameplayTagContainer& RequiredTags,
	const FGameplayTagContainer& DenyTags,
	TArray<FPrimaryAssetId>& OutIds)
{
	OutIds.Reset();

	if (RequiredTags.IsEmpty() && DenyTags.IsEmpty())
	{
		return;
	}

	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (AssetRegistry == nullptr)
	{
		return;
	}

	TArray<FAssetData> DefAssets;
	AssetRegistry->GetAssetsByClass(
		FArcItemDefinitionData::StaticStruct()->GetStructPathName(),
		DefAssets,
		false);

	for (const FAssetData& DefAsset : DefAssets)
	{
		const FGameplayTagContainer AssetTags = FArcItemDefinitionData::GetAssetTags(DefAsset);
		if (!AssetTags.HasAll(RequiredTags))
		{
			continue;
		}
		if (!DenyTags.IsEmpty() && AssetTags.HasAny(DenyTags))
		{
			continue;
		}

		const FPrimaryAssetId DefId = DefAsset.GetPrimaryAssetId();
		if (!DefId.IsValid() || !UAssetManager::Get().GetPrimaryAssetPath(DefId).IsValid())
		{
			continue;
		}

		OutIds.Add(DefId);
	}
}
