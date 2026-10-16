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
struct FArcMassItemStoreFragment;
class UScriptStruct;

namespace ArcMassItems
{
	// Resolve the (generic, un-qualified) signal name for a given event type.
	// Public so the transaction layer (and tests) can look up signal names without
	// duplicating the switch. Subscribers receive signals across all store types;
	// filter by FArcMassItemEvent::StoreType in the handler.
	ARCMASSITEMSRUNTIME_API FName GetSignalForEventType(EArcMassItemEventType Type);

	// StoreType must be FArcMassItemStoreFragment or a derived type.
	// Pass FArcMassItemStoreFragment::StaticStruct() when only the default store is in use.

	// Does not check that Spec.ItemDefinitionId resolves to an indexed record: an unknown id still adds
	// an item without a definition (IsValid() false). Gameplay code adds items through the AddNewItem*
	// commands, which reject unknown ids; call this directly only from server-internal systems and tests.
	ARCMASSITEMSRUNTIME_API FArcItemId AddItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FArcItemSpec& Spec);

	/** Full item initialization after creation. Performs ItemInstance creation, fragment lifecycle callbacks, and asset loading. Called automatically from AddItem. */
	ARCMASSITEMSRUNTIME_API void InitializeItemData(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	ARCMASSITEMSRUNTIME_API bool RemoveItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, int32 StacksToRemove = 0);

	ARCMASSITEMSRUNTIME_API bool DestroyItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	ARCMASSITEMSRUNTIME_API bool AddItemToSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag SlotTag);

	// AddItemToSlot without the occupancy check: the slot can hold any number of items. For slots whose items are
	// tracked by another system, e.g. Quick Bar items sharing SlotId.Active.
	ARCMASSITEMSRUNTIME_API bool AddItemToSharedSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag SlotTag);

	ARCMASSITEMSRUNTIME_API bool RemoveItemFromSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	ARCMASSITEMSRUNTIME_API bool ChangeItemSlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId, FGameplayTag NewSlot);

	ARCMASSITEMSRUNTIME_API bool ActivateItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	ARCMASSITEMSRUNTIME_API bool DeactivateItem(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

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
		const FPrimaryAssetId& NewVisualDefId);

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

	ARCMASSITEMSRUNTIME_API const FArcItemData* FindItemById(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	ARCMASSITEMSRUNTIME_API const FArcItemData* FindItemByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FPrimaryAssetId& DefinitionId);

	ARCMASSITEMSRUNTIME_API TArray<const FArcItemData*> FindItemsByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FPrimaryAssetId& DefinitionId);

	ARCMASSITEMSRUNTIME_API TArray<const FArcItemData*> FindItemsByTag(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FGameplayTagContainer& Tags);

	ARCMASSITEMSRUNTIME_API const FArcItemData* FindItemByDefinitionTags(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FGameplayTagContainer& Tags);

	ARCMASSITEMSRUNTIME_API TArray<const FArcItemData*> FindItemsByDefinitionTags(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FGameplayTagContainer& Tags);

	ARCMASSITEMSRUNTIME_API int32 CountItemsByDefinition(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FPrimaryAssetId& DefinitionId);

	// Fragment-direct overloads — for callers that already hold the store fragment (e.g. a ViewModel
	// handed an inventory snapshot) and have no entity manager/handle to resolve it from.

	ARCMASSITEMSRUNTIME_API int32 CountItemsByDefinition(const FArcMassItemStoreFragment& Store, const FPrimaryAssetId& DefinitionId);

	// Sums stacks of items whose ItemAggregatedTags satisfy RequiredTags (HasAll) and avoid DenyTags
	// (none of HasAny). Empty RequiredTags matches all items; empty DenyTags excludes none.
	ARCMASSITEMSRUNTIME_API int32 CountItemsByTags(const FArcMassItemStoreFragment& Store, const FGameplayTagContainer& RequiredTags, const FGameplayTagContainer& DenyTags);

	ARCMASSITEMSRUNTIME_API bool Contains(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, const FPrimaryAssetId& DefinitionId);

	ARCMASSITEMSRUNTIME_API bool IsOnAnySlot(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UScriptStruct* StoreType, FArcItemId ItemId);

	// --- Transfer operations ---
	// FromStoreType and ToStoreType may differ when moving between specialised stores.

	ARCMASSITEMSRUNTIME_API bool MoveItem(FMassEntityManager& EntityManager, FMassEntityHandle FromEntity, FMassEntityHandle ToEntity, FArcItemId ItemId, const UScriptStruct* FromStoreType, const UScriptStruct* ToStoreType);
}
