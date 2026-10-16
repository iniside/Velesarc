// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemFragment_Rarity.generated.h"

struct FArcItemSpec;

USTRUCT(BlueprintType, meta = (DisplayName = "Rarity", Category = "Item", ToolTip = "Rarity of this item instance. Absent means the default rarity."))
struct ARCMASSITEMSRUNTIME_API FArcItemFragment_Rarity : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly, Category = "Rarity")
	FGameplayTag Rarity;

	virtual ~FArcItemFragment_Rarity() override = default;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemFragment_Rarity::StaticStruct();
	}
};

namespace ArcMassItems
{
	ARCMASSITEMSRUNTIME_API FGameplayTag GetSpecRarity(const FArcItemSpec& Spec);

	ARCMASSITEMSRUNTIME_API bool IsSameStackKey(const FArcItemSpec& A, const FArcItemSpec& B);
}
