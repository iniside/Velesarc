// Copyright Lukasz Baran. All Rights Reserved.

#include "Sources/ArcEffectTagSource_SourceItem.h"

#include "Sources/ArcSourceItemResolver.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

void FArcEffectTagSource_SourceItem::AppendTags(const FArcEffectContext& Context, FGameplayTagContainer& OutSourceTags,
                                                FGameplayTagContainer& OutTargetTags) const
{
	const FArcItemData* Item = ArcAbilityKit::FindSourceItem(Context);
	if (Item == nullptr || Item->GetItemDefinition() == nullptr)
	{
		return;
	}

	const FArcItemFragment_Tags* TagsFragment = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(Item);
	if (TagsFragment != nullptr)
	{
		OutSourceTags.AppendTags(TagsFragment->ItemTags);
	}
}
