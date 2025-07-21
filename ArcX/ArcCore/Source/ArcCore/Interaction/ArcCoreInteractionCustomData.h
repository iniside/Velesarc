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


#include "UObject/Object.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemsArray.h"

#include "ArcCoreInteractionCustomData.generated.h"


struct FArcCoreInteractionItemsArray;

struct FArcCoreInteractionItem;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcCoreInteractionCustomData
{
	GENERATED_BODY()

	virtual UScriptStruct* GetScriptStruct() const
	{
		return FArcCoreInteractionCustomData::StaticStruct();
	}

	virtual void PreReplicatedRemove(const FArcCoreInteractionItemsArray& InArraySerializer, const FArcCoreInteractionItem* InItem) {}

	virtual void PostReplicatedAdd(const FArcCoreInteractionItemsArray& InArraySerializer, const FArcCoreInteractionItem* InItem) {}

	virtual void PostReplicatedChange(const FArcCoreInteractionItemsArray& InArraySerializer, const FArcCoreInteractionItem* InItem) {}

	virtual ~FArcCoreInteractionCustomData() = default;
};

class UArcItemListTable;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionData_LootTable : public FArcCoreInteractionCustomData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UArcItemListTable> LootTable;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcInteractionData_LootTable::StaticStruct();
	}

	virtual ~FArcInteractionData_LootTable() override = default;
};

USTRUCT(BlueprintType)
struct FArcLocalEntryId
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	uint32 Id = 0;

	FArcLocalEntryId()
		: Id(0)
	{}

	FArcLocalEntryId(uint32 InId)
		: Id(InId)
	{}
	
	bool operator==(const FArcLocalEntryId& Other) const
	{
		return Id == Other.Id;
	}
};

USTRUCT(BlueprintType)
struct FArcItemSpecEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FArcLocalEntryId Id;

	UPROPERTY(EditAnywhere)
	FArcItemSpec ItemSpec;

	FArcItemSpecEntry()
		: Id()
		, ItemSpec()
	{}

	FArcItemSpecEntry(uint32 InId, FArcItemSpec&& InItemSpec)
		: Id(InId)
		, ItemSpec(MoveTemp(InItemSpec))
	{}
	
	bool operator==(const FArcItemSpecEntry& Other) const
	{
		return Id == Other.Id;
	}
	
	bool operator==(const FArcLocalEntryId& Other) const
	{
		return Id == Other;
	}
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionData_ItemSpecs : public FArcCoreInteractionCustomData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<FArcItemSpecEntry> ItemSpec;

	TArray<FArcItemSpec> GetItemSpecs() const
	{
		TArray<FArcItemSpec> Specs;
		for (const FArcItemSpecEntry& E : ItemSpec)
		{
			Specs.Add(E.ItemSpec);
		}

		return Specs;
	}
	
	const FArcItemSpecEntry* FindEntry(const FArcLocalEntryId& InId) const
	{
		return ItemSpec.FindByKey(InId);
	}
	
	void RemoveItem(const FArcLocalEntryId& InId)
	{
		int32 Idx = ItemSpec.IndexOfByKey(InId);
		if (Idx != INDEX_NONE)
		{
			ItemSpec.RemoveAt(Idx);
		}
	}

	void AddItem(FArcItemSpec&& InItemSpec)
	{
		uint32 Num = (uint32)ItemSpec.Num();
		ItemSpec.Add( { Num+1, MoveTemp(InItemSpec) } );
	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcInteractionData_ItemSpecs::StaticStruct();
	}

	virtual ~FArcInteractionData_ItemSpecs() override = default;
	
#if WITH_EDITORONLY_DATA
	void PostSerialize(const FArchive& Ar);
#endif
};

template<>
struct TStructOpsTypeTraits< FArcInteractionData_ItemSpecs > : public TStructOpsTypeTraitsBase2< FArcInteractionData_ItemSpecs >
{
#if WITH_EDITORONLY_DATA
	enum
	{
		WithPostSerialize = true,
	};
#endif
};

USTRUCT(BlueprintType)
struct FArcItemDataEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FArcLocalEntryId Id;

	UPROPERTY(VisibleAnywhere)
	FArcItemCopyContainerHelper ItemData;

	bool operator==(const FArcItemSpecEntry& Other) const
	{
		return Id == Other.Id;
	}
	
	bool operator==(const FArcLocalEntryId& Other) const
	{
		return Id == Other;
	}
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionData_ItemData : public FArcCoreInteractionCustomData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<FArcItemDataEntry> Items;

	void AddItemContainer(const FArcItemCopyContainerHelper& Item)
	{
		uint32 Num = (uint32)Items.Num();
		Items.Add({Num+1, Item});
	}
	
	const FArcItemDataEntry* FindEntry(const FArcLocalEntryId& InId) const
	{
		return Items.FindByKey(InId);
	}
	
	void RemoveItem(const FArcLocalEntryId& InId)
	{
		int32 Idx = Items.IndexOfByKey(InId);
		if (Idx != INDEX_NONE)
		{
			Items.RemoveAt(Idx);
		}
	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcInteractionData_ItemData::StaticStruct();
	}

	virtual ~FArcInteractionData_ItemData() override = default;
};


USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionData_LevelActor : public FArcCoreInteractionCustomData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<AActor> LevelActor = nullptr;

	UPROPERTY(VisibleAnywhere, NotReplicated)
	TObjectPtr<AActor> LevelActorLocal = nullptr;

	UPROPERTY(EditAnywhere)
	bool bRemoveLevelActorOnRemove = false;

	virtual void PostReplicatedAdd(const FArcCoreInteractionItemsArray& InArraySerializer, const FArcCoreInteractionItem* InItem) override;
	virtual void PreReplicatedRemove(const FArcCoreInteractionItemsArray& InArraySerializer, const FArcCoreInteractionItem* InItem) override;
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcInteractionData_LevelActor::StaticStruct();
	}

	virtual ~FArcInteractionData_LevelActor() override = default;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionData_SpawnedActor : public FArcCoreInteractionCustomData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSoftClassPtr<AActor> ActorClass = nullptr;

	UPROPERTY(VisibleAnywhere, NotReplicated)
	TObjectPtr<AActor> SpawnedActor = nullptr;
	
public:
	UPROPERTY()
	FVector Location = FVector::ZeroVector;
	
	virtual void PostReplicatedAdd(const FArcCoreInteractionItemsArray& InArraySerializer, const FArcCoreInteractionItem* InItem) override;
	virtual void PreReplicatedRemove(const FArcCoreInteractionItemsArray& InArraySerializer, const FArcCoreInteractionItem* InItem) override;
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcInteractionData_SpawnedActor::StaticStruct();
	}

	virtual ~FArcInteractionData_SpawnedActor() override = default;
};

class AArcInteractionInstances;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionData_InteractionInstance : public FArcCoreInteractionCustomData
{
	GENERATED_BODY()

	/* Interaction actor, to which add additonal interaction instance. */
	UPROPERTY(EditAnywhere)
	TSoftClassPtr<AArcInteractionInstances> InteractionInstancesClass = nullptr;

	UPROPERTY(VisibleAnywhere, NotReplicated)
	TObjectPtr<AArcInteractionInstances> InteractionInstances = nullptr;

	UPROPERTY(EditAnywhere)
	bool bRemoveLevelActorOnRemove = false;

	//virtual void PostReplicatedAdd(const FArcCoreInteractionItem* InItem) override;
	//virtual void PreReplicatedRemove(const FArcCoreInteractionItem* InItem) override;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcInteractionData_InteractionInstance::StaticStruct();
	}

	virtual ~FArcInteractionData_InteractionInstance() override = default;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionData_BoolState : public FArcCoreInteractionCustomData
{
	GENERATED_BODY()

	/* Interaction actor, to which add additonal interaction instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bBoolState = false;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcInteractionData_BoolState::StaticStruct();
	}

	virtual ~FArcInteractionData_BoolState() override = default;
};

USTRUCT()
struct FArcCoreInteractionCustomDataInternal
{
	GENERATED_BODY()

	TSharedPtr<FArcCoreInteractionCustomData> CustomData;

	FArcCoreInteractionCustomDataInternal()
	: CustomData(nullptr)
	{
	}

	FArcCoreInteractionCustomDataInternal(TSharedPtr<FArcCoreInteractionCustomData> InInstance)
	{
		CustomData = InInstance;
	}
	
	FArcCoreInteractionCustomDataInternal(const FArcCoreInteractionCustomDataInternal& Other)
	{
		CustomData = Other.CustomData;
	}
	
	FArcCoreInteractionCustomDataInternal(FArcCoreInteractionCustomDataInternal&& Other)
	{
		if (CustomData.IsValid() && Other.CustomData.IsValid())
		{
			CustomData = MoveTemp(Other.CustomData);
		}
	}

	bool IsValid() const
	{
		return CustomData.IsValid();
	}
	
	bool operator==(const FArcCoreInteractionCustomDataInternal& Other) const
	{
		// When used as replicated property and we want to detect changes of the intance
		// data we need to compare the data stored in the inner structs.
		
		bool bItemEqual = false;
		const FArcCoreInteractionCustomData* PolymorphicStruct = CustomData.Get();
		const FArcCoreInteractionCustomData* OtherPolymorphicStruct = Other.CustomData.Get();
		
		const UScriptStruct* PolymorphicStructScriptStruct = PolymorphicStruct ? PolymorphicStruct->GetScriptStruct() : nullptr;
		const UScriptStruct* OtherPolymorphicStructScriptStruct = OtherPolymorphicStruct ? OtherPolymorphicStruct->GetScriptStruct() : nullptr;

		if (PolymorphicStructScriptStruct != OtherPolymorphicStructScriptStruct)
		{
			return false;
		}

		if (PolymorphicStructScriptStruct)
		{
			// Compare actual struct data	
			bItemEqual = PolymorphicStructScriptStruct->CompareScriptStruct(PolymorphicStruct, OtherPolymorphicStruct, 0);
		}
		else
		{
			return false;
		}
	
		//UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal operator=="))
		return bItemEqual;
	}

	FArcCoreInteractionCustomDataInternal& operator=(const FArcCoreInteractionCustomDataInternal& Other)
	{
		if (!(*this == Other))
		{
			const UScriptStruct* DstScriptStruct = CustomData.IsValid() ? CustomData->GetScriptStruct() : nullptr;
			const UScriptStruct* SrcScriptStruct = Other.CustomData.IsValid() ? Other.CustomData->GetScriptStruct() : nullptr;

			const FArcCoreInteractionCustomData* SrcPolymorphicStruct = Other.CustomData.IsValid() ? Other.CustomData.Get() : nullptr;
			if (SrcPolymorphicStruct)
			{
				CA_ASSUME(SrcScriptStruct != nullptr);
				FArcCoreInteractionCustomData* const DstPolymorphicStruct = CustomData.IsValid() ? CustomData.Get() : nullptr;
				if (DstPolymorphicStruct == SrcPolymorphicStruct)
				{
					//UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal operator= Deep Copy ItemEntry"))
					SrcScriptStruct->CopyScriptStruct(DstPolymorphicStruct, SrcPolymorphicStruct);
				}
				else
				{
					if (DstPolymorphicStruct && SrcPolymorphicStruct)
					{
						SrcScriptStruct->CopyScriptStruct(DstPolymorphicStruct, SrcPolymorphicStruct);
						//UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal operator= Copy Src to Dest %s"), *GetNameSafe(DstPolymorphicStruct->GetItemDefinition()))
					}
					else
					{
						//UE_LOG(LogItemArray, Log, TEXT("FArcItemDataInternal operator= Memory Allocation ItemEntry Source %s"), *GetNameSafe(SrcPolymorphicStruct->GetItemDefinition()))
						FArcCoreInteractionCustomData* NewPolymorphicStruct = static_cast<FArcCoreInteractionCustomData*>(FMemory::Malloc(SrcScriptStruct->GetStructureSize(), SrcScriptStruct->GetMinAlignment()));
						SrcScriptStruct->InitializeStruct(NewPolymorphicStruct);
						SrcScriptStruct->CopyScriptStruct(NewPolymorphicStruct, SrcPolymorphicStruct);
						CustomData = MakeShareable(NewPolymorphicStruct);	
					}
				}
			}
			else
			{
				CustomData.Reset();
			}
		}
		return *this;
	}
	FArcCoreInteractionCustomDataInternal& operator=(FArcCoreInteractionCustomDataInternal&& Other)
	{
		CustomData = MoveTemp(Other.CustomData);
		return *this;
	}
};

template <>
struct TStructOpsTypeTraits<FArcCoreInteractionCustomDataInternal> : public TStructOpsTypeTraitsBase2<FArcCoreInteractionCustomDataInternal>
{
	enum
	{
		WithCopy = true
		, WithIdenticalViaEquality = true
	};
};

USTRUCT()
struct FArcCoreInteractionCustomDataInternalArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcCoreInteractionCustomDataInternal> Data;

	FArcCoreInteractionCustomDataInternalArray() { Data.Reset(); }

	FArcCoreInteractionCustomDataInternalArray(FArcCoreInteractionCustomData* DataPtr)
	{
		Data.Add(TSharedPtr<FArcCoreInteractionCustomData>(DataPtr));
	}

	FArcCoreInteractionCustomDataInternalArray(FArcCoreInteractionCustomDataInternalArray&& Other) : Data(MoveTemp(Other.Data))	{ }
	FArcCoreInteractionCustomDataInternalArray(const FArcCoreInteractionCustomDataInternalArray& Other)
	{
		if (Data.Num() == Other.Data.Num())
		{
			// TODO: This should work as we always the same number of items.
			for (int32 Idx =0; Idx < Data.Num(); Idx++)
			{
				if (Data[Idx] != Other.Data[Idx])
				{
					Data[Idx] = Other.Data[Idx];
				}
			}	
		}
		else
		{
			Data = Other.Data;	
		}
	}

	FArcCoreInteractionCustomDataInternalArray& operator=(FArcCoreInteractionCustomDataInternalArray&& Other) { Data = MoveTemp(Other.Data); return *this; }
	FArcCoreInteractionCustomDataInternalArray& operator=(const FArcCoreInteractionCustomDataInternalArray& Other)
	{
		if (Data.Num() == Other.Data.Num())
		{
			// TODO: This should work as we always the same number of items.
			for (int32 Idx =0; Idx < Data.Num(); Idx++)
			{
				if (Data[Idx] != Other.Data[Idx])
				{
					Data[Idx] = Other.Data[Idx];
				}
			}	
		}
		else
		{
			Data = Other.Data;	
		}
		
		return *this;
	}
	
	bool operator==(const FArcCoreInteractionCustomDataInternalArray& Other) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("FArcItemInstanceArray::operator==");
		
		// This comparison operator only compares actual instance pointers
		if (Data.Num() != Other.Data.Num())
		{
			return false;
		}

		for (int32 It = 0, EndIt = Data.Num(); It < EndIt; ++It)
		{
			if (Data[It].IsValid() != Other.Data[It].IsValid())
			{
				return false;
			}

			// Deep Comparison.
			if (Data[It] != Other.Data[It])
			{
				return false;
			}
		}
		return true;
	}

	/** Comparison operator */
	bool operator!=(const FArcCoreInteractionCustomDataInternalArray& Other) const
	{
		return !(FArcCoreInteractionCustomDataInternalArray::operator==(Other));
	}
};

template<>
struct TStructOpsTypeTraits<FArcCoreInteractionCustomDataInternalArray> : public TStructOpsTypeTraitsBase2<FArcCoreInteractionCustomDataInternalArray>
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FExamplePolymorphicArrayItem> Data is copied around
		WithIdenticalViaEquality = true,
	};
};

USTRUCT(BlueprintType)
struct FArcCoreInteractionItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Transient)
	FGuid Id;
	
	UPROPERTY(Transient)
	FArcCoreInteractionCustomDataInternalArray CustomData;

	// TODO:: Can change it to bitfield.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UScriptStruct>> ChangedCustomData;

	UPROPERTY(Transient, NotReplicated)
	TArray<TObjectPtr<UScriptStruct>> ChangedCustomDataNotReplicated;
	
	UPROPERTY(Transient)
	bool bRemoveLevelActor = false;

	UPROPERTY(Transient)
	FVector Location = FVector::ZeroVector;

	// Needs UPROPERTY() or be stomped.
	UPROPERTY(NotReplicated, Transient)
	bool bSpawnOnServer = false;
	
	static FArcCoreInteractionItem Make()
	{
		FArcCoreInteractionItem Item;
		return Item;
	}
	
public:
	bool operator==(const FArcCoreInteractionItem& Other) const
	{
		return  Id == Other.Id
				&& CustomData == Other.CustomData;
	}
	FArcCoreInteractionItem()
	{
	}
	
	FArcCoreInteractionItem(const FArcCoreInteractionItem& Other)
	{
		Id = Other.Id;
		CustomData = Other.CustomData;
		ChangedCustomData = Other.ChangedCustomData;
		ChangedCustomDataNotReplicated = Other.ChangedCustomDataNotReplicated; 
	}

	FArcCoreInteractionItem(FArcCoreInteractionItem&& Other)
	{
		Id = Other.Id;
		CustomData = MoveTemp(Other.CustomData);
		ChangedCustomData = MoveTemp(Other.ChangedCustomData);
		ChangedCustomDataNotReplicated = MoveTemp(Other.ChangedCustomDataNotReplicated);
	}
	
	FArcCoreInteractionItem& operator=(const FArcCoreInteractionItem& Other)
	{
		Id = Other.Id;
		//if (CustomData.CustomData.IsValid() && Other.CustomData.CustomData.IsValid())
		{
			CustomData = Other.CustomData;	
		}
		ChangedCustomData = Other.ChangedCustomData;
		ChangedCustomDataNotReplicated = Other.ChangedCustomDataNotReplicated;
		return *this;
	}

	FArcCoreInteractionItem& operator=(FArcCoreInteractionItem&& Other)
	{
		Id = Other.Id;
		CustomData = MoveTemp(Other.CustomData);
		ChangedCustomData = MoveTemp(Other.ChangedCustomData);
		ChangedCustomDataNotReplicated = MoveTemp(Other.ChangedCustomDataNotReplicated);
		return *this;
	}

	void PreRemove(const FArcCoreInteractionItemsArray& InArraySerializer);
	void OnAdded(const FArcCoreInteractionItemsArray& InArraySerializer);
	void OnChanged(const FArcCoreInteractionItemsArray& InArraySerializer);
	
	void PreReplicatedRemove(const FArcCoreInteractionItemsArray& InArraySerializer);

	void PostReplicatedAdd(const FArcCoreInteractionItemsArray& InArraySerializer);

	void PostReplicatedChange(const FArcCoreInteractionItemsArray& InArraySerializer);
};

template <>
struct TStructOpsTypeTraits<FArcCoreInteractionItem>
		: public TStructOpsTypeTraitsBase2<FArcCoreInteractionItem>
{
	enum
	{
		WithCopy = true
		, WithIdenticalViaEquality = true
	};
};

USTRUCT(BlueprintType)
struct FArcCoreInteractionItemsArray : public FIrisFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TArray<FArcCoreInteractionItem> Items;

	// Valid only on server.
	TMap<FGuid, int32> IdToItemIdx;
	
	AActor* Owner = nullptr;

	using ItemArrayType = TArray<FArcCoreInteractionItem>;
	
	const ItemArrayType& GetItemArray() const
	{
		return Items;
	}
	
	ItemArrayType& GetItemArray()
	{
		return Items;
	}
	
	typedef UE::Net::TIrisFastArrayEditor<FArcCoreInteractionItemsArray> FFastArrayEditor;
	
	FFastArrayEditor Edit()
	{
		return FFastArrayEditor(*this);
	}

	// TODO: Might be nice to cache of indexes, as they shouldn't change that often once everything is initialized.
	FArcCoreInteractionItem* AddInteraction(const FGuid& InId, TArray<FArcCoreInteractionCustomData*>&& InCustomData)
	{
		FArcCoreInteractionItem Item = FArcCoreInteractionItem::Make();
		Item.Id = InId;
		
		int32 Idx = Items.Add(MoveTemp(Item));
		for (FArcCoreInteractionCustomData* CustomData : InCustomData)
		{
			int32 CustomIdx = Items[Idx].CustomData.Data.AddDefaulted();
			Items[Idx].CustomData.Data[CustomIdx].CustomData = TSharedPtr<FArcCoreInteractionCustomData>(CustomData);
		}
		
		for (int32 ItemIdx = 0; ItemIdx < Items.Num(); ItemIdx++)
		{
			IdToItemIdx.FindOrAdd(Items[ItemIdx].Id) = ItemIdx;
		}
		
		Items[Idx].OnAdded(*this);
		MarkItemDirty(Items[Idx]);

		return &Items[Idx]; 
	}
	void AddInteractionLocal(const FGuid& InId, const TArray<FArcCoreInteractionCustomData*>& InCustomData)
	{
		FArcCoreInteractionItem Item = FArcCoreInteractionItem::Make();
		Item.Id = InId;

		int32 Idx = Items.Add(MoveTemp(Item));
		for (FArcCoreInteractionCustomData* CustomData : InCustomData)
		{
			int32 CustomIdx = Items[Idx].CustomData.Data.AddDefaulted();
			Items[Idx].CustomData.Data[CustomIdx].CustomData = TSharedPtr<FArcCoreInteractionCustomData>(CustomData);
		}

		for (int32 ItemIdx = 0; ItemIdx < Items.Num(); ItemIdx++)
		{
			IdToItemIdx.FindOrAdd(Items[ItemIdx].Id) = ItemIdx;
		}
		Items[Idx].OnAdded(*this);
		//MarkItemDirty(Items[Idx]);
	}
	void Remove(const FGuid& InteractionId, bool bRemoveLevelActor)
	{
		for (int32 Idx = 0; Idx < Items.Num(); Idx++)
		{
			if (Items[Idx].Id == InteractionId)
			{
				Items[Idx].bRemoveLevelActor = true;
				MarkItemDirty(Items[Idx]);
				Items[Idx].PreRemove(*this);
				Edit().Remove(Idx);
				for (int32 ItemIdx = 0; ItemIdx < Items.Num(); ItemIdx++)
				{
					IdToItemIdx.FindOrAdd(Items[ItemIdx].Id) = ItemIdx;
				}
				return;
			}
		}
	}

	template<typename T>
	T* FindInteractionDataMutable(const FGuid& InId)
	{
		for (int32 Idx = 0; Idx < Items.Num(); Idx++)
		{
			if (Items[Idx].Id == InId)
			{
				for (FArcCoreInteractionCustomDataInternal& Data : Items[Idx].CustomData.Data)
				{
					if (Data.CustomData->GetScriptStruct() == T::StaticStruct())
					{
						Items[Idx].ChangedCustomDataNotReplicated.Add(T::StaticStruct());
						return static_cast<T*>(Data.CustomData.Get());		
					}
				}
			}
		}

		return nullptr;
	}
	
	void MarkDirtyById(const FGuid& InteractionId)
    {
    	for (int32 Idx = 0; Idx < Items.Num(); Idx++)
    	{
    		if (Items[Idx].Id == InteractionId)
    		{
    			Items[Idx].ChangedCustomData.Empty();
    			Items[Idx].ChangedCustomData = Items[Idx].ChangedCustomDataNotReplicated;
    			Items[Idx].ChangedCustomDataNotReplicated.Empty();
    			
    			Items[Idx].OnChanged(*this);
    			MarkItemDirty(Items[Idx]);
    			return;
    		}
    	}
    }
};

template <>
struct TStructOpsTypeTraits<FArcCoreInteractionItemsArray> : public TStructOpsTypeTraitsBase2<FArcCoreInteractionItemsArray>
{
	enum
	{
		WithCopy = false
	};
};
