/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once
#include "ArcItemInstance.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"
#include "GameplayTagContainer.h"
#include "UObject/NoExportTypes.h"
#include "Mass/EntityHandle.h"

#include "ArcScalableFloat.h"
#include "Items/ArcItemTypes.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcItemData.generated.h"

class UScriptStruct;
struct FArcItemDefinitionData;
class UWorld;
struct FArcItemInstance;
struct FArcItemSpec;
struct FArcScalableCurveFloat;
struct FArcScalableFloatItemFragment;
struct FArcItemInstanceArray;

DECLARE_LOG_CATEGORY_EXTERN(LogItemEntry, Log, Log);

/*
 * Mutable runtime item state. Lives inside Mass entities (ArcMassItems plugin) —
 * the actor-side UArcItemsStoreComponent has been retired. Identity / data fields
 * are preserved; the actor-side replication, slot, and attach surface (the
 * PreReplicatedRemove / PostReplicatedAdd / PostReplicatedChange callbacks,
 * AddToSlot / RemoveFromSlot / ChangeSlot, AttachToItem / DetachFromItem,
 * NotifyFragmentsSlotAdded / Removed, CleanupOwnerOnDetach, OnItemAdded /
 * OnItemChanged / OnPreRemove broadcast helpers) was deleted with the store.
 * Mass-side equivalents live in ArcMassItems (Operations / handlers).
 */
USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcItemData
{
	GENERATED_BODY()

public:
	/**
	 * Merged Scalable Fragments from this item, and items attached to it.
	 * Rebuilt by RebuildScalableFloatFragments(); type-keyed, one winner per type:
	 * own definition wins, then first attached item in ItemsInSockets order fills gaps.
	 */
	TMap<const UScriptStruct*, const FArcScalableFloatItemFragment*> ScalableFloatFragments;

	FArcScalableFloatContext ScalableFloatContext;

	UPROPERTY(SaveGame)
	FArcItemSpec Spec;

	/** Cached pointer to items which are attached to this item. */
	mutable TArray<const FArcItemData*> ItemsInSockets;

	UPROPERTY(SaveGame)
	TArray<FArcItemId> AttachedItems;

	TMap<const UScriptStruct*, FStructView> InstancedData;

	/** World context. Set by InitializeMass. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> World;

	/** Mass entity that owns this item. Do NOT add to comparison operators — transient context, not identity. */
	FMassEntityHandle MassEntityHandle;

	/*
	 * ItemId is always generated on server and is consistent between client and server.
	 * ItemId might also be read from backend service/save.
	 */
	UPROPERTY(SaveGame)
	FArcItemId ItemId;

	/** Id of owner, if this item is attached to other item. */
	UPROPERTY(SaveGame)
	FArcItemId OwnerId;

	UPROPERTY(SaveGame)
	FArcItemId OldOwnerId;

	UPROPERTY(SaveGame)
    FGameplayTag Slot;

	UPROPERTY(SaveGame)
	FGameplayTag OldSlot;

	UPROPERTY(SaveGame)
	FGameplayTag AttachedToSlot;

	UPROPERTY(SaveGame)
	FGameplayTag OldAttachedToSlot;

	/** Current Level of item. Currently used for reading from Scalable Floats. Can for example mean item rarity. */
	UPROPERTY(SaveGame)
	uint8 Level = 1;

	UPROPERTY(SaveGame)
	uint16 Stacks = 1;

	UPROPERTY()
	bool bIsActive = false;

	const FArcItemDefinitionData* ItemDefinition = nullptr;

	/** ItemTags aggregated from item and items linked to this (like Parts). */
	UPROPERTY(NotReplicated)
	FGameplayTagContainer ItemAggregatedTags;

	UPROPERTY()
	FGameplayTagContainer DynamicTags;

	UPROPERTY()
	uint8 ForceRep = 0;

	/** Incremented on server each time this item is mutated. Used for client-side version validation. */
	UPROPERTY()
	uint32 Version = 0;

	UPROPERTY()
	FArcItemInstanceArray ItemInstances;

	mutable FArcItemData* OwnerItemCachedPtr = nullptr;

public:
	FArcItemData& operator=(const FArcItemData& Other)
	{
		ItemId = Other.ItemId;
		Spec = Other.Spec;
		AttachedItems = Other.AttachedItems;
		OwnerId = Other.OwnerId;
		OldOwnerId = Other.OldOwnerId;
		Slot = Other.Slot;
		OldSlot = Other.OldSlot;
		AttachedToSlot = Other.AttachedToSlot;
		OldAttachedToSlot = Other.OldAttachedToSlot;
		Level = Other.Level;
		Stacks = Other.Stacks;
		bIsActive = Other.bIsActive;
		ItemDefinition = Other.ItemDefinition;
		ItemAggregatedTags = Other.ItemAggregatedTags;
		DynamicTags = Other.DynamicTags;
		ForceRep = Other.ForceRep;
		Version = Other.Version;
		ItemInstances = Other.ItemInstances;

		return *this;
	}

	FArcItemData();
	FArcItemData(const FArcItemData& Other) = default;

	FArcItemData(FArcItemData&& Other)
	{
		ItemId = MoveTemp(Other.ItemId);
		Spec = MoveTemp(Other.Spec);
		AttachedItems = MoveTemp(Other.AttachedItems);
		OwnerId = MoveTemp(Other.OwnerId);
		OldOwnerId = MoveTemp(Other.OldOwnerId);
		Slot = MoveTemp(Other.Slot);
		OldSlot = MoveTemp(Other.OldSlot);
		AttachedToSlot = MoveTemp(Other.AttachedToSlot);
		OldAttachedToSlot = MoveTemp(Other.OldAttachedToSlot);
		Level = MoveTemp(Other.Level);
		Stacks = Other.Stacks;
		bIsActive = Other.bIsActive;
		ItemDefinition = MoveTemp(Other.ItemDefinition);
		ItemAggregatedTags = MoveTemp(Other.ItemAggregatedTags);
		DynamicTags = MoveTemp(Other.DynamicTags);
		ForceRep = Other.ForceRep;
		Version = Other.Version;
		ItemInstances = MoveTemp(Other.ItemInstances);
	}

	~FArcItemData();

	UScriptStruct* GetScriptStruct() const;

	/**
	 * Get value at level of this item from @link FArcScalableFloatItemFragment.
	 */
	float GetValue(const FArcScalableCurveFloat& InScalableFloat) const;

	/**
	 * Get value at @param InLevel from @link FArcScalableFloatItemFragment.
	 */
	float GetValueWithLevel(const FArcScalableCurveFloat& InScalableFloat
							, float InLevel) const;

	const TArray<const FArcItemData*>& GetItemsInSockets() const;

	void SetOwnerId(const FArcItemId& InOwnerId)
	{
		OwnerId = InOwnerId;
	}

	const FArcItemSpec* GetSpecPtr() const;

	const FArcItemData* GetOwnerPtr() const;

public:
	void SetItemSpec(const FArcItemSpec& InSpec);

	FGameplayTag GetSlotId() const
	{
		return Slot;
	}
	FGameplayTag GetAttachSlot() const
	{
		return AttachedToSlot;
	}

	const TArray<FArcItemId>& GetAttachedItems() const
	{
		return AttachedItems;
	}

	static FInstancedStruct NewFromSpec(const FArcItemSpec& InSpec);

	bool operator!=(const FArcItemData& Other) const
	{
		return ForceRep != Other.ForceRep
				|| ItemId != Other.ItemId
				|| Level != Other.Level
				|| OwnerId != Other.OwnerId
				|| OldOwnerId != Other.OldOwnerId
				|| Slot != Other.Slot
				|| OldSlot != Other.OldSlot
				|| AttachedToSlot != Other.AttachedToSlot
				|| OldAttachedToSlot != Other.OldAttachedToSlot
				|| bIsActive != Other.bIsActive
				|| GetItemDefinitionId() != Other.GetItemDefinitionId()
				|| AttachedItems != Other.AttachedItems;

	}

	bool operator==(const FArcItemData& Other) const
	{
		return ForceRep == Other.ForceRep
				&& ItemId == Other.ItemId
				&& Level == Other.Level
				&& OwnerId == Other.OwnerId
				&& OldOwnerId == Other.OldOwnerId
				&& Slot == Other.Slot
				&& OldSlot == Other.OldSlot
				&& AttachedToSlot == Other.AttachedToSlot
				&& OldAttachedToSlot == Other.OldAttachedToSlot
				&& bIsActive == Other.bIsActive
				&& GetItemDefinitionId() == Other.GetItemDefinitionId()
				&& AttachedItems == Other.AttachedItems;
	}

	bool IsValid() const
	{
		return GetItemDefinitionId().IsValid() && ItemDefinition != nullptr && ItemId.IsValid();
	}


	const uint8* FindFragment(UScriptStruct* InStructType) const;

	template<typename T>
	const T* FindScalableItemFragment() const
	{
		if (ScalableFloatFragments.Contains(T::StaticStruct()))
		{
			return static_cast<const T*>(ScalableFloatFragments[T::StaticStruct()]);
		}

		return nullptr;
	}

	template<typename T>
	const T* FindSpecFragment() const
	{
		return Spec.FindFragment<T>();
	}

	const uint8* FindSpecFragment(UScriptStruct* InStructType) const
	{
		return Spec.FindFragment(InStructType);
	}

	void SetItemInstances(const FArcItemInstanceArray& InInstances);

	/**
	 * Rebuild ScalableFloatFragments from this item's definition plus all items in
	 * ItemsInSockets. Called from InitializeMass, RefreshItemDefinition and from
	 * AttachItem/DetachItem after the socket set changes. Own definition wins per type;
	 * first socket fills gaps.
	 */
	void RebuildScalableFloatFragments();

	/**
	 * Rebuild ItemAggregatedTags from the definition's FArcItemFragment_Tags.
	 * ItemAggregatedTags is derived, NotReplicated state — servers build it in
	 * InitializeMass; clients must rebuild it after fast-array receive
	 * (FArcMassReplicatedItem::PostReplicatedAdd/Change) or tag-based queries
	 * (CountItemsByTags, DoesItemSatisfy) silently match nothing.
	 */
	void RebuildAggregatedTags();

	void RefreshItemDefinition();

	/** Mass-native initialization. Builds instance array, scalable floats, aggregated tags. */
	void InitializeMass(UWorld* InWorld);

	/** Fires OnItemAdded on definition + spec fragments. Mass operations call this after entity placement. */
	void OnItemAdded();

	/** Fires OnPreRemoveItem on definition + spec fragments. Mass operations call this before entity teardown. */
	void OnPreRemove();

	void Deinitialize();

	const FArcItemId& GetItemId() const
	{
		return ItemId;
	}

	const FArcItemId& GetOwnerId() const
	{
		return OwnerId;
	}

	void AddDynamicTag(const FGameplayTag& InTag)
	{
		DynamicTags.AddTag(InTag);
	}

	void RemoveDynamicTag(const FGameplayTag& InTag)
	{
		DynamicTags.RemoveTag(InTag);
	}

public:
	uint8 GetLevel() const
	{
		return Level;
	}

	uint16 GetStacks() const
	{
		return Stacks;
	}

	bool IsActive() const
	{
		return bIsActive;
	}

	uint32 GetVersion() const
	{
		return Version;
	}

	void IncrementVersion()
	{
		++Version;
	}

	void SetStacks(uint16 InStacks)
	{
		Stacks = InStacks;
	}

	const FPrimaryAssetId& GetItemDefinitionId() const;

	const FArcItemDefinitionData* GetItemDefinition() const;

	const FGameplayTagContainer& GetItemAggregatedTags() const;

	template<typename T>
	void ForSpecFragment(TFunctionRef<void(const FArcItemData*, const T*)> ForEachFunc) const
	{
		TArray<const T*> FragmentsCopy = GetSpecPtr()->TGetSpecFragments<T>();
		for (const T* Fragment : FragmentsCopy)
		{
			ForEachFunc(this, Fragment);
		}
	}
};

template <>
struct TStructOpsTypeTraits<FArcItemData> : public TStructOpsTypeTraitsBase2<FArcItemData>
{
	enum
	{
		WithIdenticalViaEquality = true
	};
};

USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcItemDataHandle
{
	GENERATED_BODY()

private:
	FArcItemData* ItemData = nullptr;

public:
	FArcItemDataHandle() = default;
	explicit FArcItemDataHandle(FArcItemData* InItemData)
		: ItemData(InItemData)
	{
	}

	const FArcItemData* Get() const { return ItemData; }
	FArcItemData* Get() { return ItemData; }

	bool IsValid() const { return ItemData != nullptr; }

	const FArcItemData* operator->() const { return ItemData; }
	FArcItemData* operator->() { return ItemData; }
};

template <>
struct TStructOpsTypeTraits<FArcItemDataHandle>
		: public TStructOpsTypeTraitsBase2<FArcItemDataHandle>
{
	enum
	{
		WithCopy = true
	};
};
