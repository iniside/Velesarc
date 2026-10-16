// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Sources/ArcAbilitySource_Item.h"

struct FMassEntityManager;
struct FArcEffectContext;
struct FArcItemData;

namespace ArcAbilityKit
{
	ARCABILITYKIT_API const FArcItemData* FindSourceItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle);

	ARCABILITYKIT_API const FArcItemData* FindSourceItem(const FArcEffectContext& Context);

	ARCABILITYKIT_API FArcAbilitySource_Item MakeItemSource(const FArcItemData* Item, const FGameplayTag& SlotId);
}
