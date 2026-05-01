// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemTypes.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"

struct FMassEntityManager;
struct FMassExecutionContext;
struct FArcItemSpec;
struct FArcItemData;
struct FArcItemInstanceArray;
struct FArcScalableCurveFloat;
class UScriptStruct;

namespace ArcMassItems
{
	// StoreType must be FArcMassItemStoreFragment or a derived type.
	// Pass FArcMassItemStoreFragment::StaticStruct() when only the default store is in use.

	ARCMASSITEMSRUNTIME_API FArcItemId AddItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& Spec);

	ARCMASSITEMSRUNTIME_API bool RemoveItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, int32 StacksToRemove = 0);

	ARCMASSITEMSRUNTIME_API bool AddItemToSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag SlotTag);

	ARCMASSITEMSRUNTIME_API bool RemoveItemFromSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	ARCMASSITEMSRUNTIME_API bool ModifyStacks(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, int32 Delta);

	// --- Attachment operations ---

	ARCMASSITEMSRUNTIME_API bool AttachItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId OwnerId, FArcItemId AttachmentId, FGameplayTag AttachSlot);

	ARCMASSITEMSRUNTIME_API bool DetachItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId AttachmentId);

	// --- Instance data operations ---

	ARCMASSITEMSRUNTIME_API bool SetItemInstances(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, const FArcItemInstanceArray& Instances);

	ARCMASSITEMSRUNTIME_API const FArcItemInstanceArray* GetItemInstances(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	// --- Scalable value ---

	ARCMASSITEMSRUNTIME_API float GetScalableValue(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, const FArcScalableCurveFloat& ScalableFloat);

	// --- Level and tags ---

	ARCMASSITEMSRUNTIME_API bool SetLevel(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, uint8 NewLevel);

	ARCMASSITEMSRUNTIME_API bool AddDynamicTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag Tag);

	ARCMASSITEMSRUNTIME_API bool RemoveDynamicTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag Tag);

	// --- Query operations ---

	ARCMASSITEMSRUNTIME_API const FArcItemData* FindItemByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FPrimaryAssetId& DefinitionId);

	ARCMASSITEMSRUNTIME_API TArray<const FArcItemData*> FindItemsByTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FGameplayTagContainer& Tags);

	// --- Transfer operations ---
	// FromStoreType and ToStoreType may differ when moving between specialised stores.

	ARCMASSITEMSRUNTIME_API bool MoveItem(FMassEntityManager& EntityManager, FMassEntityHandle FromEntity, FMassEntityHandle ToEntity, FArcItemId ItemId, const UScriptStruct* FromStoreType, const UScriptStruct* ToStoreType);
}
