// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuildIngredient.h"

#include "Items/ArcItemData.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Items/ArcItemsHelpers.h"
#include "Fragments/ArcMassItemStoreFragment.h"

bool FArcBuildIngredient::DoesItemSatisfy(const FArcItemData* InItemData) const
{
	return InItemData != nullptr;
}

int32 FArcBuildIngredient::CountAvailable(const FArcMassItemStoreFragment& Store) const
{
	int32 Total = 0;
	for (const FArcMassReplicatedItem& ReplicatedItem : Store.ReplicatedItems.Items)
	{
		const FArcItemData* Item = ReplicatedItem.ToItem();
		if (Item != nullptr && Item->GetItemId().IsValid() && Item->Stacks > 0 && DoesItemSatisfy(Item))
		{
			Total += static_cast<int32>(Item->Stacks);
		}
	}
	return Total;
}

bool FArcBuildIngredient_ItemDef::DoesItemSatisfy(const FArcItemData* InItemData) const
{
	if (!InItemData)
	{
		return false;
	}

	if (!ItemDefinitionId.IsValid())
	{
		return false;
	}

	return InItemData->GetItemDefinitionId() == ItemDefinitionId;
}

bool FArcBuildIngredient_Tags::DoesItemSatisfy(const FArcItemData* InItemData) const
{
	if (!InItemData)
	{
		return false;
	}

	const FArcItemFragment_Tags* TagsFragment = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(InItemData);
	if (!TagsFragment)
	{
		return false;
	}

	const FGameplayTagContainer& ItemTags = TagsFragment->AssetTags;

	if (RequiredTags.Num() > 0 && !ItemTags.HasAll(RequiredTags))
	{
		return false;
	}

	if (DenyTags.Num() > 0 && ItemTags.HasAny(DenyTags))
	{
		return false;
	}

	return true;
}
