#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemData.h"
#include "GameplayTagContainer.h"
#include "ArcAbilitySourceData_Item.generated.h"

class UArcItemDefinition;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAbilitySourceData_Item
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Source")
	TObjectPtr<const UArcItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Source")
	FArcItemId ItemId;

	UPROPERTY(VisibleAnywhere, Category = "Source")
	FArcItemDataHandle ItemDataHandle;

	UPROPERTY(VisibleAnywhere, Category = "Source")
	FGameplayTag SlotId;
};
