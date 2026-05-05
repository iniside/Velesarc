// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityHandle.h"

struct FArcItemData;
struct FMassEntityManager;
class UScriptStruct;

namespace ArcMassItems::QuickBar::Internal
{
	bool ItemMatchesRequiredTags(const FArcItemData* ItemData, const FGameplayTagContainer& Required);

	const FArcItemData* GetItemFromStore(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FArcItemId ItemId);
}
