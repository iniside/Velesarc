// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class FArcSaveArchive;
class FArcLoadArchive;
struct FArcItemSpec;

/**
 * Item-spec persistence helpers. The full TArray<FArcItemCopyContainerHelper>
 * surface (Save/Load + attachment serialization) retired with
 * UArcItemsStoreComponent (plan step 8b-persistence). Only single-spec helpers
 * remain, used by FArcItemSpecSerializer.
 */
struct ARCMASSITEMSRUNTIME_API FArcItemStoreSerializer
{
	static constexpr uint32 Version = 2;

	static void SaveItemSpec(const FArcItemSpec& Spec, const FGameplayTag& SlotId, FArcSaveArchive& Ar);
	static void LoadItemSpec(FArcItemSpec& OutSpec, FGameplayTag& OutSlotId, FArcLoadArchive& Ar);

private:
	static void SavePersistentInstances(const FArcItemSpec& Spec, FArcSaveArchive& Ar);
	static void LoadPersistentInstances(FArcItemSpec& Spec, FArcLoadArchive& Ar);

	static void SaveInstanceData(const FArcItemSpec& Spec, FArcSaveArchive& Ar);
	static void LoadInstanceData(FArcItemSpec& Spec, FArcLoadArchive& Ar);
};
