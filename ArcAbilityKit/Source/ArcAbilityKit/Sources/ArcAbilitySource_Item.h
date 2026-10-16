// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fragments/ArcAbilityCustomDataBase.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemData.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "ArcAbilitySource_Item.generated.h"

/**
 * Item-flavored custom data payload for FArcAbilityCustomData.
 *
 * Carries the originating item's definition, runtime id, data handle and slot
 * into an active ability. Inherits FArcAbilityCustomDataBase so it surfaces
 * in the FArcAbilityCustomData editor dropdown.
 */
USTRUCT(BlueprintType)
struct ARCABILITYKIT_API FArcAbilitySource_Item : public FArcAbilityCustomDataBase
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Source")
	FPrimaryAssetId ItemDefinition;

	UPROPERTY(VisibleAnywhere, Category = "Source")
	FArcItemId ItemId;

	UPROPERTY(VisibleAnywhere, Category = "Source")
	FArcItemDataHandle ItemDataHandle;

	UPROPERTY(VisibleAnywhere, Category = "Source")
	FGameplayTag SlotId;
};
