// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemFragment_InstanceTags.generated.h"

struct FArcItemSpec;

USTRUCT(BlueprintType, meta = (DisplayName = "Instance Tags", Category = "Item", ToolTip = "Tags of this item instance, e.g. an enchantment. Part of the stack key. Absent means no instance tags."))
struct ARCMASSITEMSRUNTIME_API FArcItemFragment_InstanceTags : public FArcItemFragment_ItemInstanceBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly, Category = "Instance Tags")
	FGameplayTagContainer Tags;

	virtual ~FArcItemFragment_InstanceTags() override = default;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemFragment_InstanceTags::StaticStruct();
	}
};

namespace ArcMassItems
{
	ARCMASSITEMSRUNTIME_API const FGameplayTagContainer& GetSpecInstanceTags(const FArcItemSpec& Spec);
}
