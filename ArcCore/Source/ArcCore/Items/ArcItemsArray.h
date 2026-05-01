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
#include "Items/ArcItemDefinition.h"
#include "ArcItemData.h"
#include "ArcItemId.h"
#include "ArcItemInstance.h"
#include "ArcItemSpec.h"

#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Serialization/JsonSerializerMacros.h"
#include "Net/Core/NetBitArray.h"
#include "StructUtils/PropertyBag.h"

#if UE_WITH_IRIS
#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"
#endif

#include "ArcItemsArray.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogItemArray
	, Log
	, Log);

class FReferenceCollector;


USTRUCT()
struct FArcAttachedItemHelper
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FArcItemId ItemId;
	
	UPROPERTY()
	FArcItemSpec Item;

	UPROPERTY()
	FGameplayTag SlotId;
};

USTRUCT()
struct ARCCORE_API FArcItemCopyContainerHelper
{
	GENERATED_BODY()

	UPROPERTY()
	FArcItemId ItemId;
	
	UPROPERTY()
	FArcItemSpec Item;

	UPROPERTY()
	FGameplayTag SlotId;
	
	// Attachments of Item.
	UPROPERTY()
	TArray<FArcAttachedItemHelper> ItemAttachments;

	static FArcItemCopyContainerHelper New(UArcItemsStoreComponent* InItemsStore, const FArcItemData* InData);
	static FArcItemCopyContainerHelper New(const FArcItemData* InData);

	static FArcItemCopyContainerHelper FromSpec(const FArcItemSpec& Item);

	TArray<FArcItemId> AddItems(UArcItemsStoreComponent* InItemsStore);

	/** Converts an FArcItemData back to an FArcItemSpec, preserving persistent instance data. */
	static FArcItemSpec ToSpec(const FArcItemData* InData);

private:
	/** Copies all persistent FArcItemInstance data from ItemData into the spec's InitialInstanceData. */
	static void CopyPersistentInstances(FArcItemSpec& OutSpec, const FArcItemData* InData);

public:

	friend uint32 GetTypeHash(const FArcItemCopyContainerHelper& In)
	{
		return GetTypeHash(In.ItemId);
	}

	bool operator==(const FArcItemCopyContainerHelper& Other) const
	{
		return ItemId == Other.ItemId;
	}

	bool operator==(const FArcItemId& Other) const
	{
		return ItemId == Other;
	}
};


USTRUCT()
struct FArcItemDataInternalWrapper
	: public FFastArraySerializerItem
{
	GENERATED_BODY()

	friend struct FArcItemsArray;

	UPROPERTY()
	FInstancedStruct ItemData;

	// Called when item is being removed. Only bind to it if you need just Id. Item
	// Pointer might not be valid.
	static FArcGenericItemIdDelegate OnItemRemovedDelegate;

public:
	FArcItemDataInternalWrapper() = default;

	explicit FArcItemDataInternalWrapper(const FArcItemSpec& InSpec)
	{
		ItemData = FArcItemData::NewFromSpec(InSpec);
		FArcItemData* Data = ItemData.GetMutablePtr<FArcItemData>();
		Data->Spec = InSpec;
	}

	FArcItemDataInternalWrapper(const FArcItemDataInternalWrapper& Other) = default;
	FArcItemDataInternalWrapper(FArcItemDataInternalWrapper&& Other) = default;
	FArcItemDataInternalWrapper& operator=(const FArcItemDataInternalWrapper& Other) = default;
	FArcItemDataInternalWrapper& operator=(FArcItemDataInternalWrapper&& Other) = default;

	void PreReplicatedRemove(const struct FArcItemsArray& InArraySerializer);
	void PostReplicatedAdd(const struct FArcItemsArray& InArraySerializer);
	void PostReplicatedChange(const struct FArcItemsArray& InArraySerializer);

	bool operator==(const FArcItemDataInternalWrapper& Other) const
	{
		const FArcItemData* A = ItemData.GetPtr<FArcItemData>();
		const FArcItemData* B = Other.ItemData.GetPtr<FArcItemData>();
		if (A && B) return *A == *B;
		return A == B;
	}

	bool operator==(const FArcItemId& InItemId) const
	{
		const FArcItemData* Data = ItemData.GetPtr<FArcItemData>();
		return Data && Data->GetItemId() == InItemId;
	}

	bool operator==(const FPrimaryAssetId& AssetId) const
	{
		const FArcItemData* Data = ItemData.GetPtr<FArcItemData>();
		return Data && Data->GetItemDefinitionId() == AssetId;
	}

	bool operator==(const UArcItemDefinition* ItemDefinition) const
	{
		const FArcItemData* Data = ItemData.GetPtr<FArcItemData>();
		return Data && Data->GetItemDefinition() == ItemDefinition;
	}

	FArcItemData* ToItem()
	{
		return ItemData.GetMutablePtr<FArcItemData>();
	}

	const FArcItemData* ToItem() const
	{
		return ItemData.GetPtr<FArcItemData>();
	}

	bool IsValid() const
	{
		return ItemData.IsValid();
	}
};

template <>
struct TStructOpsTypeTraits<FArcItemDataInternalWrapper>
		: public TStructOpsTypeTraitsBase2<FArcItemDataInternalWrapper>
{
	enum
	{
		WithCopy = true
		, WithIdenticalViaEquality = true
	};
};


USTRUCT(BlueprintType)
struct FArcItemsArray
	: public FIrisFastArraySerializer
{
	GENERATED_BODY()

	friend class UArcItemsStoreComponent;
	friend struct FArcMoveItemBetweenStoresCommand;
	friend struct ArcItemsHelper;
	friend struct FArcItemCopyContainerHelper;
	
protected:
	UPROPERTY()
	TArray<FArcItemDataInternalWrapper> Items;

	mutable TMap<FArcItemId, FArcItemData*> ItemsMap;
	
public:
	const TArray<FArcItemDataInternalWrapper>& GetItems() const
	{
		return Items;
	}
	
	UPROPERTY(NotReplicated)
	TObjectPtr<class UArcItemsStoreComponent> Owner;

	/**
	 * Called before removing elements and after the elements themselves are notified. The
	 * indices are valid for this function call only!
	 *
	 * NOTE: intentionally not virtual; invoked via templated code, @see FExampleItemEntry
	 */
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices
							 , int32 FinalSize);

	/**
	 * Called after adding all new elements and after the elements themselves are
	 * notified.  The indices are valid for this function call only!
	 *
	 * NOTE: intentionally not virtual; invoked via templated code, @see FExampleItemEntry
	 */
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices
						   , int32 FinalSize);

	/**
	 * Called after updating all existing elements with new data and after the elements
	 * themselves are notified. The indices are valid for this function call only!
	 *
	 * NOTE: intentionally not virtual; invoked via templated code, @see FExampleItemEntry
	 */
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices
							  , int32 FinalSize);
	~FArcItemsArray()
	{
		ItemsMap.Reset();
	}
	
public:
	void AddCachedItem(const FArcItemId& InId, FArcItemData* InItem) const
	{
		ItemsMap.FindOrAdd(InId) = InItem;
	}

	void RemoveCachedItem(const FArcItemId& InId) const
	{
		ItemsMap[InId] = nullptr;
		ItemsMap.Remove(InId);
	}


	using ItemArrayType = TArray<FArcItemDataInternalWrapper>;
	
	const ItemArrayType& GetItemArray() const
	{
		return Items;
	}
	
	ItemArrayType& GetItemArray()
	{
		return Items;
	}
	
	typedef UE::Net::TIrisFastArrayEditor<FArcItemsArray> FFastArrayEditor;
	
	FFastArrayEditor Edit()
	{
		return FFastArrayEditor(*this);
	}

	FArcItemsArray()
		: FIrisFastArraySerializer()
		, Owner(nullptr)
	{
	}

private:
	TArray<FArcItemId> AddItemCopyInternal(UArcItemsStoreComponent* NewItemStoreComponent
		, FArcItemCopyContainerHelper& InContainer);
	
public:
	int32 AddItem(const FArcItemSpec& NewItemSpec
				  , bool& bAlreadyExists);
	
	void RemoveItem(const FArcItemId& InItemId);

	void RemoveItem(int32 Idx);

	void AddItemInstance(const FArcItemData* ToItem, UScriptStruct* InInstanceType);
	
	void SetOwner(class UArcItemsStoreComponent* NewOwner)
	{
		Owner = NewOwner;
	}

	void AddInternalItem(UArcItemsStoreComponent* NewItemStoreComponent, FInstancedStruct&& InItem, TArray<FInstancedStruct>& AttachedItems);

	TArray<FArcItemCopyContainerHelper> GetAllInternalItems() const;
	FArcItemCopyContainerHelper GetItemCopyHelper(const FArcItemId& InItemId) const;
	
	FArcItemData* operator[](int32 Idx)
	{
		return Items[Idx].ToItem();
	}

	const FArcItemData* operator[](int32 Idx) const
	{
		return Items[Idx].ToItem();
	}

	const FArcItemData* operator[](FArcItemId InItemId) const
	{
		if (const FArcItemData* const* P = ItemsMap.Find(InItemId))
		{
			return *P;
		}
		return nullptr;
	}

	FArcItemData* operator[](FArcItemId InItemId)
	{
		if (FArcItemData** P = ItemsMap.Find(InItemId))
		{
			return *P;
		}
		return nullptr;
	}
	
	TArray<const FArcItemData*> GetItemsPtr() const
	{
		TArray<const FArcItemData*> OutItems;
		for (const FArcItemDataInternalWrapper& W : Items)
		{
			OutItems.Add(W.ToItem());
		}
		return OutItems;
	}

	const FArcItemData* GetItemFromSlot(const FGameplayTag& InSlotId) const
	{
		for (const FArcItemDataInternalWrapper& W : Items)
		{
			if(W.ToItem()->GetSlotId() == InSlotId)
			{
				return W.ToItem();
			}
		}

		return nullptr;
	}

	TArray<const FArcItemData*> GetAllItemsOnSlots() const
	{
		TArray<const FArcItemData*> Out;
		for (const FArcItemDataInternalWrapper& W : Items)
		{
			if(W.ToItem()->GetSlotId().IsValid())
			{
				Out.Add(W.ToItem());
			}
		}

		return Out;
	}

	TArray<const FArcItemData*> GetItemsAttachedTo(const FArcItemId& InItemId) const
	{
		TArray<const FArcItemData*> Out;

		if (InItemId.IsValid() == false)
		{
			return Out;
		}
		
		for (const FArcItemDataInternalWrapper& W : Items)
		{
			if(W.ToItem()->GetOwnerId() == InItemId)
			{
				Out.Add(W.ToItem());
			}
		}

		return Out;
	}

	
	void MarkItemDirtyIdx(int32 Idx);

	void MarkItemDirtyHandle(const FArcItemId& Handle);

	void MarkItemInstanceDirtyHandle(const FArcItemId& Handle, UScriptStruct* InScriptStruct);
	
	int32 Num() const
	{
		return Items.Num();
	}

	template<typename T>
	const FArcItemData* operator()(const T& AssetId) const
	{
		int32 Idx = Items.IndexOfByKey(AssetId);
		if (Idx != INDEX_NONE)
		{
			return Items[Idx].ToItem();
		}
		
		return nullptr;
	}

	template<typename T>
	FArcItemData* operator()(const T& AssetId)
	{
		int32 Idx = Items.IndexOfByKey(AssetId);
		if (Idx != INDEX_NONE)
		{
			return Items[Idx].ToItem();
		}
		
		return nullptr;
	}

	
	template<typename T>
	bool Contains(const T& AssetId) const
	{
		int32 Idx = Items.IndexOfByKey(AssetId);
	
		if (Idx != INDEX_NONE)
		{
			return true;
		}
		
		return false;
	}
		
	template<typename T>
	int32 IndexOf(const T& Type)
	{
		return Items.IndexOfByKey(Type);
	}
};

// WithNoDestructor
template <>
struct TStructOpsTypeTraits<FArcItemsArray> : public TStructOpsTypeTraitsBase2<FArcItemsArray>
{
	enum
	{
		WithCopy = false
	};
};
