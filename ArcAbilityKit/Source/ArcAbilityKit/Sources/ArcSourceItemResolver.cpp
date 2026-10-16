// Copyright Lukasz Baran. All Rights Reserved.

#include "Sources/ArcSourceItemResolver.h"

#include "Sources/ArcAbilitySource_Item.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Effects/ArcEffectContext.h"
#include "Items/ArcItemData.h"
#include "Mass/EntityManager.h"

namespace ArcAbilityKit
{
	const FArcItemData* FindSourceItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle)
	{
		const FArcActiveAbility* ActiveAbility = ArcAbilities::FindAbility(EntityManager, Entity, Handle);
		if (ActiveAbility == nullptr)
		{
			return nullptr;
		}

		const FArcAbilitySource_Item* ItemSource = ActiveAbility->CustomData.Find<FArcAbilitySource_Item>();
		if (ItemSource == nullptr)
		{
			return nullptr;
		}

		return ItemSource->ItemDataHandle.Get();
	}

	const FArcItemData* FindSourceItem(const FArcEffectContext& Context)
	{
		const FArcAbilitySource_Item* ItemSource = Context.SourceData.GetPtr<FArcAbilitySource_Item>();
		if (ItemSource == nullptr)
		{
			return nullptr;
		}

		return ItemSource->ItemDataHandle.Get();
	}

	FArcAbilitySource_Item MakeItemSource(const FArcItemData* Item, const FGameplayTag& SlotId)
	{
		FArcAbilitySource_Item ItemSource;
		ItemSource.ItemDefinition = Item->GetItemDefinitionId();
		ItemSource.ItemId = Item->GetItemId();
		ItemSource.ItemDataHandle = FArcItemDataHandle(const_cast<FArcItemData*>(Item));
		ItemSource.SlotId = SlotId;
		return ItemSource;
	}
}
