// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemTypes.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "Fragments/ArcMassItemEventFragment.h"

struct FMassEntityManager;
struct FMassExecutionContext;
struct FArcItemSpec;
struct FArcItemData;
struct FArcItemInstanceArray;
struct FArcScalableCurveFloat;
class UScriptStruct;
class UArcItemDefinition;

namespace ArcMassItems
{
	// Resolve the qualified signal name for a given event type and store struct.
	// Public so the transaction layer (and tests) can look up signal names without
	// duplicating the switch.
	ARCMASSITEMSRUNTIME_API FName GetSignalForEventType(
		EArcMassItemEventType Type, const UScriptStruct* StoreType);

	// StoreType must be FArcMassItemStoreFragment or a derived type.
	// Pass FArcMassItemStoreFragment::StaticStruct() when only the default store is in use.

	ARCMASSITEMSRUNTIME_API FArcItemId AddItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& Spec);

	/** Full item initialization after creation. Performs ItemInstance creation, fragment lifecycle callbacks, and asset loading. Called automatically from AddItem. */
	ARCMASSITEMSRUNTIME_API void InitializeItemData(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	ARCMASSITEMSRUNTIME_API bool RemoveItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, int32 StacksToRemove = 0);

	ARCMASSITEMSRUNTIME_API bool DestroyItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	ARCMASSITEMSRUNTIME_API bool AddItemToSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag SlotTag);

	ARCMASSITEMSRUNTIME_API bool RemoveItemFromSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	ARCMASSITEMSRUNTIME_API bool ChangeItemSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag NewSlot);

	ARCMASSITEMSRUNTIME_API bool ModifyStacks(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, int32 Delta);

	// --- Attachment operations ---

	ARCMASSITEMSRUNTIME_API bool AttachItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId OwnerId, FArcItemId AttachmentId, FGameplayTag AttachSlot);

	ARCMASSITEMSRUNTIME_API bool DetachItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId AttachmentId);

	ARCMASSITEMSRUNTIME_API TArray<const FArcItemData*> GetAttachedItems(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId OwnerId);

	ARCMASSITEMSRUNTIME_API const FArcItemData* FindAttachedItemOnSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId OwnerId, FGameplayTag AttachSlot);

	ARCMASSITEMSRUNTIME_API const FArcItemData* GetItemAttachedTo(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId OwnerId, FGameplayTag AttachSlot);

	// Updates the item's visual attachment record. Sets the FArcItemInstance_ItemVisualAttachment
	// on the item, sets the matching FArcMassItemAttachment.OldVisualItemDefinition / VisualItemDefinition
	// pair, and emits an AttachmentChanged event so the attachment processor can re-realize the visual.
	ARCMASSITEMSRUNTIME_API bool SetVisualItemAttachment(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UScriptStruct* StoreType,
		FArcItemId ItemId,
		UArcItemDefinition* NewVisualDef);

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

	ARCMASSITEMSRUNTIME_API const FArcItemData* FindItemByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const UArcItemDefinition* Definition);

	ARCMASSITEMSRUNTIME_API TArray<const FArcItemData*> FindItemsByTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FGameplayTagContainer& Tags);

	ARCMASSITEMSRUNTIME_API const FArcItemData* FindItemByDefinitionTags(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FGameplayTagContainer& Tags);

	ARCMASSITEMSRUNTIME_API TArray<const FArcItemData*> FindItemsByDefinitionTags(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FGameplayTagContainer& Tags);

	ARCMASSITEMSRUNTIME_API int32 CountItemsByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FPrimaryAssetId& DefinitionId);

	ARCMASSITEMSRUNTIME_API bool Contains(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const UArcItemDefinition* Definition);

	ARCMASSITEMSRUNTIME_API bool IsOnAnySlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	// --- Transfer operations ---
	// FromStoreType and ToStoreType may differ when moving between specialised stores.

	ARCMASSITEMSRUNTIME_API bool MoveItem(FMassEntityManager& EntityManager, FMassEntityHandle FromEntity, FMassEntityHandle ToEntity, FArcItemId ItemId, const UScriptStruct* FromStoreType, const UScriptStruct* ToStoreType);
}
