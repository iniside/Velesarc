// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemTypes.h"

#include "ArcItemListEntry.generated.h"

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcItemListEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FArcItemId ItemId;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FText Description;

	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag CurrentItemStoreSlot;

	UPROPERTY(BlueprintReadOnly)
	int32 Stacks = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTagContainer Tags;

	UPROPERTY(BlueprintReadOnly)
	bool bIsEquipped = false;

	UPROPERTY(BlueprintReadOnly)
	FText RarityLabel;

	UPROPERTY(BlueprintReadOnly)
	FLinearColor RarityColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly)
	FText CategoryLabel;
};
