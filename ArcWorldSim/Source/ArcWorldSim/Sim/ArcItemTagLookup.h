// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"

namespace ArcWorldSim
{
struct FArcAssetTagFilterKey
{
	FGameplayTagContainer RequiredTags;
	FGameplayTagContainer DenyTags;

	bool operator==(const FArcAssetTagFilterKey& Other) const
	{
		return RequiredTags == Other.RequiredTags && DenyTags == Other.DenyTags;
	}

	friend uint32 GetTypeHash(const FArcAssetTagFilterKey& Key)
	{
		uint32 RequiredHash = 0x9e3779b9u;
		for (const FGameplayTag& Tag : Key.RequiredTags.GetGameplayTagArray())
		{
			RequiredHash ^= GetTypeHash(Tag);
		}
		uint32 DenyHash = 0x85ebca6bu;
		for (const FGameplayTag& Tag : Key.DenyTags.GetGameplayTagArray())
		{
			DenyHash ^= GetTypeHash(Tag);
		}
		return HashCombine(RequiredHash, DenyHash);
	}
};

ARCWORLDSIM_API void CollectItemIdsForAssetTags(
	const FGameplayTagContainer& RequiredTags,
	const FGameplayTagContainer& DenyTags,
	TArray<FPrimaryAssetId>& OutIds);
}
