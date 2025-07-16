/**
 * This file is part of ArcX.
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
#include "ArcMapAbilityEffectSpecContainer.h"
#include "GameplayTagContainer.h"
#include "UObject/NoExportTypes.h"

#include "Items/ArcItemTypes.h"
#include "Items/ArcItemSpec.h"

#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Iris/Serialization/NetSerializer.h"

#include "ArcItemData.generated.h"

class UScriptStruct;
class UGameplayEffect;
class UArcItemsStoreComponent;
class UArcItemDefinition;
struct FArcItemInstance;
struct FArcItemSpec;
struct FArcItemsArray;
struct FArcScalableCurveFloat;
struct FArcScalableFloatItemFragment;
struct FArcItemInstanceArray;

DECLARE_LOG_CATEGORY_EXTERN(LogItemEntry, Log, Log);

/*
 * This is instance of item. It's stored as SharedPtr<> only to make it easier to pass the
 * reference around, since array gets resized, indexes change, there is no guarantee of
 * memory unless it's allocated as shared.
 *
 * The non mutable properties cloud be stored as TSharedPtr<FArcItemSpec> either here or
 * ArcItemEntryInternal. Since spec once generated is non mutable.
 */
USTRUCT()
struct ARCCORE_API FArcItemData
{
	GENERATED_BODY()
	
	friend struct FArcItemDataInternalWrapper;
	friend struct FArcItemDataInternal;
	friend struct FArcItemsArray;
	friend struct ArcItems;

public:
	/** Merged Scalable Fragments from this item, and items attached to it. */
	TMap<const UScriptStruct*, const FArcScalableFloatItemFragment*> ScalableFloatFragments;

	/** Spec from this items has been created. Immutable and never changes once created. */
	//mutable TSharedPtr<FArcItemSpec> SpecPtr = nullptr;

	UPROPERTY(SaveGame)
	FArcItemSpec Spec;
	
	/** Cached pointer to items which are attached to this item. */
	mutable TArray<const FArcItemData*> ItemsInSockets;

	UPROPERTY(SaveGame)
	TArray<FArcItemId> AttachedItems;
	
	TMap<FName, TSharedPtr<FArcItemInstance>> InstancedData;
	/**
	 * Items Store component, which currently owns this item.
	 * Can change over time as Item is moved between components.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UArcItemsStoreComponent> OwnerComponent = nullptr;

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

	UPROPERTY(NotReplicated)
	bool bAddedToSlot = false;

	UPROPERTY(NotReplicated)
	bool bRemoveFromSlot = false;
	
	// TODO: Remove
	// TODO:: Maybe not remove. I don't know.
	UPROPERTY(SaveGame)
	FGameplayTag AttachedToSlot;

	UPROPERTY(SaveGame)
	FGameplayTag OldAttachedToSlot;

	/** Current Level of item. Currently used for reading from Scalable Floats. Can for example mean item rarity. */
	UPROPERTY(SaveGame)
	uint8 Level = 1;
	
	/** properties which all items have and we want to replicate them anyway. */
	UPROPERTY()
	mutable TObjectPtr<const UArcItemDefinition> ItemDefinition = nullptr;

	/** ItemTags aggregated from item and items linked to this (like Parts). */
	UPROPERTY(NotReplicated)
	FGameplayTagContainer ItemAggregatedTags;

	UPROPERTY()
	FGameplayTagContainer DynamicTags;

	// Fix oe workaround for item nor replicating correctly, when it's properties changes to fast from ie, RPC which edits item.
	UPROPERTY()
	uint8 ForceRep = 0;

	UPROPERTY()
	FArcItemInstanceArray ItemInstances;
	
	/** Handle is valid we item is replicated but it's owner is not. */
	FDelegateHandle WaitOwnerItemAdded;

	mutable TWeakPtr<FArcItemData> OwnerItemWeakPtr;

public:
	FArcItemData& operator=(const FArcItemData& Other)
	{
		ItemId = Other.ItemId;
		Spec = Other.Spec;
		AttachedItems = Other.AttachedItems;
		OwnerComponent = Other.OwnerComponent;
		OwnerId = Other.OwnerId;
		OldOwnerId = Other.OldOwnerId;
		Slot = Other.Slot;
		OldSlot = Other.OldSlot;
		AttachedToSlot = Other.AttachedToSlot;
		OldAttachedToSlot = Other.OldAttachedToSlot;
		Level = Other.Level;
		ItemDefinition = Other.ItemDefinition;
		ItemAggregatedTags = Other.ItemAggregatedTags;
		ForceRep = Other.ForceRep;
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
		OwnerComponent = MoveTemp(Other.OwnerComponent);
		OwnerId = MoveTemp(Other.OwnerId);
		OldOwnerId = MoveTemp(Other.OldOwnerId);
		Slot = MoveTemp(Other.Slot);
		OldSlot = MoveTemp(Other.OldSlot);
		AttachedToSlot = MoveTemp(Other.AttachedToSlot);
		OldAttachedToSlot = MoveTemp(Other.OldAttachedToSlot);
		Level = MoveTemp(Other.Level);
		ItemDefinition = MoveTemp(Other.ItemDefinition);
		ItemAggregatedTags = MoveTemp(Other.ItemAggregatedTags);
		ItemInstances = MoveTemp(Other.ItemInstances);
	}
	
	virtual ~FArcItemData();

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

	UArcItemsStoreComponent* GetItemsStoreComponent() const
	{
		return OwnerComponent;
	}
	
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
	
	static TSharedPtr<FArcItemData> NewFromSpec(const FArcItemSpec& InSpec);
	
	static TSharedPtr<FArcItemData> Duplicate(UArcItemsStoreComponent* InItemsStore
									   , const FArcItemData* From
									   , const bool bPreserveItemId);
	
	bool operator!=(const FArcItemData& Other) const
	{
		//return Other.ItemId != ItemId;
		return ForceRep != Other.ForceRep
				|| ItemId != Other.ItemId
				|| Level != Other.Level
				|| OwnerId != Other.OwnerId
				|| OldOwnerId != Other.OldOwnerId
				|| Slot != Other.Slot
				|| OldSlot != Other.OldSlot
				|| AttachedToSlot != Other.AttachedToSlot
				|| OldAttachedToSlot != Other.OldAttachedToSlot
				|| OwnerComponent != Other.OwnerComponent
				|| ItemDefinition != Other.ItemDefinition
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
				&& OwnerComponent == Other.OwnerComponent
				&& ItemDefinition == Other.ItemDefinition
				&& AttachedItems == Other.AttachedItems;
	}

	bool operator==(const TSharedPtr<FArcItemData>& Other) const
	{
		return *this == *Other.Get();
	}
	
	bool operator!=(const FArcItemData* Other) const
	{
		return *this != *Other;
	}
	
	bool operator==(const FArcItemData* Other) const
	{
		return *this == *Other;
	}

	bool operator!=(const TSharedPtr<FArcItemData>& Other) const
	{
		return *this != *Other.Get();
	}

	bool IsValid() const
	{
		return ItemDefinition != nullptr && ItemId.IsValid();
	}

	
	const uint8* FindFragment(UScriptStruct* InStructType) const;

	template<typename T>
	const T* FindScalableItemFramgnet() const
	{
		if (ScalableFloatFragments.Contains(T::StaticStruct()))
		{
			return ScalableFloatFragments[T::StaticStruct()];
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

	void AddToSlot(const FGameplayTag& InSlotId);
	void RemoveFromSlot(const FGameplayTag& InSlotId);
	void ChangeSlot(const FGameplayTag& InNewSlot);
	/** Attach to provided Item Id. */
	void AttachToItem(const FArcItemId& InOwnerItem, const FGameplayTag& InAttachSlot);

	/** Detach from item, which is current owner of this item. */
	void DetachFromItem();

	void PreReplicatedRemove(const FArcItemsArray& InArraySerializer);

	void PostReplicatedAdd(const FArcItemsArray& InArraySerialize);

	void PostReplicatedChange(const FArcItemsArray& InArraySerializer);

	void Initialize(UArcItemsStoreComponent* ItemsStoreComponent);

	void OnItemAdded();
	void OnItemChanged();
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
		OnItemChanged();
	}

	void RemoveDynamicTag(const FGameplayTag& InTag)
	{
		DynamicTags.AddTag(InTag);
		OnItemChanged();
	}
	
public:
	uint8 GetLevel() const
	{
		return Level;
	}

	const FPrimaryAssetId& GetItemDefinitionId() const;

	const UArcItemDefinition* GetItemDefinition() const;

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
		//WithCopy = true,
		WithIdenticalViaEquality = true
	};
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcItemDataHandle
{
	GENERATED_BODY()

private:
	TWeakPtr<FArcItemData> ItemData;

public:
	FArcItemDataHandle() = default;
	FArcItemDataHandle(TSharedPtr<FArcItemData>& InItemData)
	{
		ItemData = InItemData;
	}

	FArcItemDataHandle(TWeakPtr<FArcItemData>& InItemData)
	{
		ItemData = InItemData;
	}
	
	const FArcItemData* operator()() const
	{
		return ItemData.IsValid() ? ItemData.Pin().Get() : nullptr;
	}

	const FArcItemData* Get() const
	{
		return ItemData.IsValid() ? ItemData.Pin().Get() : nullptr;
	}

	FArcItemData* operator()()
	{
		return ItemData.IsValid() ? ItemData.Pin().Get() : nullptr;
	}

	FArcItemData* Get()
	{
		return ItemData.IsValid() ? ItemData.Pin().Get() : nullptr;
	}

	FArcItemDataHandle& operator=(const FArcItemDataHandle& Other)
	{
		ItemData = Other.ItemData;
		return *this;
	}

	bool IsValiD() const
	{
		return ItemData.IsValid();
	}

	FArcItemData* operator->()
	{
		return ItemData.IsValid() ? ItemData.Pin().Get() : nullptr;
	}

	const FArcItemData* operator->() const
	{
		return ItemData.IsValid() ? ItemData.Pin().Get() : nullptr;
	}
	
	FArcItemData* operator*()
	{
		return ItemData.IsValid() ? ItemData.Pin().Get() : nullptr;
	}
	
	const FArcItemData* operator*() const
	{
		return ItemData.IsValid() ? ItemData.Pin().Get() : nullptr;
	}
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


/*
 * TODO Remove this when using Iris ?
 **/
namespace Arcx::Net
{
	//struct FArcItemDataInternalNetSerializer;

	struct FArcItemCache
	{
		static TMap<FArcItemId, TSharedPtr<FArcItemData>> Items;
	};
}

namespace Arcx::Net
{
	struct FArcItemDataInternalNetSerializer;
}