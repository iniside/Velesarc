// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcTagCountContainer.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcTagCountContainer
{
	GENERATED_BODY()

	int32 AddTag(FGameplayTag Tag);
	int32 RemoveTag(FGameplayTag Tag);
	void AddTags(const FGameplayTagContainer& InTags);
	void RemoveTags(const FGameplayTagContainer& InTags);

	int32 GetCount(FGameplayTag Tag) const;

	bool HasTag(FGameplayTag Tag) const;
	bool HasTagExact(FGameplayTag Tag) const;
	bool HasAll(const FGameplayTagContainer& Other) const;
	bool HasAllExact(const FGameplayTagContainer& Other) const;
	bool HasAny(const FGameplayTagContainer& Other) const;
	bool HasAnyExact(const FGameplayTagContainer& Other) const;
	FGameplayTagContainer Filter(const FGameplayTagContainer& Other) const;
	FGameplayTagContainer FilterExact(const FGameplayTagContainer& Other) const;
	bool IsEmpty() const;
	int32 Num() const;

	const FGameplayTagContainer& GetTagContainer() const;
	const TMap<FGameplayTag, int32>& GetTagCounts() const { return TagCounts; }

private:
	UPROPERTY()
	FGameplayTagContainer Tags;

	UPROPERTY()
	TMap<FGameplayTag, int32> TagCounts;
};
