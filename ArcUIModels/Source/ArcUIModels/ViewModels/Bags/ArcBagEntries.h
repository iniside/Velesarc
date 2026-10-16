// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Bags/ArcBagId.h"
#include "Items/ArcItemTypes.h"

#include "ArcBagEntries.generated.h"

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcBagTabEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FArcBagId BagId;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	int32 SlotCount = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bActive = false;
};

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcBagSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bHasItem = false;

	UPROPERTY(BlueprintReadOnly)
	FArcItemId ItemId;

	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(BlueprintReadOnly)
	FText ItemName;

	UPROPERTY(BlueprintReadOnly)
	FText ItemDescription;

	UPROPERTY(BlueprintReadOnly)
	int32 Stacks = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bSelected = false;
};
