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

#include "ArcNamedPrimaryAssetId.h"
#include "Items/ArcItemTypes.h"
#include "Items/ArcItemInstance.h"

#include "Items/Fragments/ArcItemFragment.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "StructUtils/InstancedStruct.h"

#include "ArcItemSpec.generated.h"

class UArcItemDefinition;

DECLARE_LOG_CATEGORY_EXTERN(LogArcItemSpec, Log, All);

USTRUCT()
struct ARCCORE_API FArcItemSpecFragmentInstances
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TArray<FInstancedStruct> Data;

public:
	FArcItemSpecFragmentInstances() { }

	void Clear() { Data.Reset(); }
	int32 Num() const { return Data.Num(); }

	bool IsValid(int32 Index) const
	{
		return (Index < Data.Num() && Data[Index].IsValid());
	}

	const FArcItemFragment_ItemInstanceBase* Get(int32 Index) const
	{
		return IsValid(Index) ? Data[Index].GetPtr<FArcItemFragment_ItemInstanceBase>() : nullptr;
	}

	FArcItemFragment_ItemInstanceBase* Get(int32 Index)
	{
		return IsValid(Index) ? Data[Index].GetMutablePtr<FArcItemFragment_ItemInstanceBase>() : nullptr;
	}

	const FArcItemFragment_ItemInstanceBase* Get(UScriptStruct* InType) const
	{
		for (const FInstancedStruct& Instance : Data)
		{
			if (Instance.IsValid() && Instance.GetScriptStruct()->IsChildOf(InType))
			{
				return Instance.GetPtr<FArcItemFragment_ItemInstanceBase>();
			}
		}
		return nullptr;
	}

	template<typename T>
	const FArcItemFragment_ItemInstanceBase* Get() const
	{
		for (const FInstancedStruct& Instance : Data)
		{
			if (Instance.IsValid() && Instance.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				return Instance.GetPtr<FArcItemFragment_ItemInstanceBase>();
			}
		}
		return nullptr;
	}

	void RemoveAt(int32 Index)
	{
		if (Index >= 0 && Index < Num())
		{
			Data.RemoveAt(Index);
		}
	}

	void Add(FArcItemFragment_ItemInstanceBase* DataPtr)
	{
		UScriptStruct* StructType = DataPtr->GetScriptStruct();
		FInstancedStruct& Added = Data.AddDefaulted_GetRef();
		Added.InitializeAs(StructType, reinterpret_cast<const uint8*>(DataPtr));
	}

	void Add(const FInstancedStruct& InStruct)
	{
		Data.Add(InStruct);
	}

	void Append(const FArcItemSpecFragmentInstances& OtherHandle)
	{
		Data.Append(OtherHandle.Data);
	}

	bool operator==(const FArcItemSpecFragmentInstances& Other) const
	{
		return Data == Other.Data;
	}

	bool operator!=(const FArcItemSpecFragmentInstances& Other) const
	{
		return !(operator==(Other));
	}

	TArray<const FArcItemFragment_ItemInstanceBase*> GetDataArray() const
	{
		TArray<const FArcItemFragment_ItemInstanceBase*> Out;
		for (const FInstancedStruct& Instance : Data)
		{
			if (Instance.IsValid())
			{
				const FArcItemFragment_ItemInstanceBase* Ptr = Instance.GetPtr<FArcItemFragment_ItemInstanceBase>();
				if (Ptr)
				{
					Out.Add(Ptr);
				}
			}
		}
		return Out;
	}
};

/*
 * Spec which contain definition of item from which FArcItemData can be created.
 * It more or less mirrors what ItemEntry have, but after creation it immutable.
 * While ItemEntry can be treated as instance. Might store it later as SharedPtr and share
 * between entries (if possible).
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcItemSpec
	: public FFastArraySerializerItem
{
	GENERATED_BODY()

	friend struct FArcItemData;

public:
	UPROPERTY(EditAnywhere, Category = "Data")
	FArcItemId ItemId;

	UPROPERTY(EditAnywhere, Category = "Data")
	uint16 Amount = 1;

	UPROPERTY(EditAnywhere, Category = "Data")
	uint8 Level = 1;

	UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	mutable FArcNamedPrimaryAssetId ItemDefinitionId;

	/**
	 * List of @link FArcItemFragment_ItemInstanceBase, which can be added to spec during creation.
	 * Remember that SAME TYPE of fragment if added here SHOULD NOT BE added to ArcItemDefinition and vice verse.
	 * 
	 * Once item is added to store, instances from these fragments are added as well.
	 * Can be used to dynamically generate list of stats/abilities/effects and so on, which are then added on item.
	 */
	UPROPERTY()
	FArcItemSpecFragmentInstances InstanceData;

	TArray<FInstancedStruct> InitialInstanceData;
	
	UPROPERTY()
	mutable TObjectPtr<const UArcItemDefinition> ItemDefinition;
	
public:
	UScriptStruct* GetScriptStruct() const
	{
		return FArcItemSpec::StaticStruct();
	}

	const FPrimaryAssetId GetItemDefinitionId() const
	{
		return ItemDefinitionId;
	}
	const UArcItemDefinition* GetItemDefinition() const;

	static FArcItemSpec NewItem(const UArcItemDefinition* NewItem
								, uint8 Level
								, int32 Amount);

	static FArcItemSpec NewItem(const FPrimaryAssetId& NewItem
								, uint8 Level
								, int32 Amount);
	
	/**
	 * @brief Adds instance of of fragment T. Must be derived from  FArcItemFragment_ItemInstanceBase.
	 * Each type of fragment added must be unique.
	 */
	template<typename T>
	FArcItemSpec& AddInstanceData(T* InInstanceData)
	{
		static_assert(TIsDerivedFrom<T, FArcItemFragment_ItemInstanceBase>::Value, "Only acceepts fragments derived from FArcItemFragment_ItemInstanceBase");
		
		TArray<const FArcItemFragment_ItemInstanceBase*> Array = InstanceData.GetDataArray();
		for (const FArcItemFragment_ItemInstanceBase* Item : Array)
		{
			if (Item->GetScriptStruct() == T::StaticStruct())
			{
				return *this;
			}
		}
		if (T::StaticStruct()->IsChildOf(FArcItemFragment_ItemInstanceBase::StaticStruct()))
		{
			InstanceData.Add(static_cast<FArcItemFragment_ItemInstanceBase*>(InInstanceData));
		}
		return *this;
	}

	FArcItemSpec& SetItemId(const FArcItemId& InId)
	{
		ItemId = InId;
		return *this;
	}

	FArcItemSpec& SetAmount(int32 InAmount)
	{
		Amount = InAmount;
		return *this;
	}

	FArcItemSpec& SetItemLevel(uint8 InLevel)
	{
		Level = InLevel;
		return *this;
	}

	FArcItemSpec& SetItemDefinition(const FPrimaryAssetId& InItemDefinitionId)
	{
		ItemDefinitionId = InItemDefinitionId;
		return *this;
	}
	
	FArcItemSpec& SetItemDefinitionAsset(const UArcItemDefinition* InItemDefinition)
	{
		ItemDefinition = InItemDefinition;
		return *this;
	}
	
	FArcItemSpec()
		: ItemId()
		, Amount(1)
		, Level(1)
		, ItemDefinitionId()
	{
	}
	
	FArcItemSpec(const FArcItemSpec& Other)
	{
		ItemId = Other.ItemId;
		Amount = Other.Amount;
		Level = Other.Level;
		ItemDefinitionId = Other.ItemDefinitionId;
		InstanceData = Other.InstanceData;
		ItemDefinition = Other.ItemDefinition;
		InitialInstanceData = Other.InitialInstanceData;
	}
	FArcItemSpec(FArcItemSpec&& Other)
	{
		ItemId = Other.ItemId;
		Amount = Other.Amount;
		Level = Other.Level;
		ItemDefinitionId = Other.ItemDefinitionId;
		InstanceData = MoveTemp(Other.InstanceData);
		ItemDefinition = MoveTemp(Other.ItemDefinition);
		InitialInstanceData = MoveTemp(Other.InitialInstanceData);
		Other.ItemDefinition = nullptr;
	}

	~FArcItemSpec()
	{
		InstanceData.Clear();
	}

	bool operator==(const FArcItemSpec& Other) const
	{
		//We need to do Deep Comparison here..
		//Shouldn't be any slower than "normal" USTRUCT, as it is the same code.
		for( TFieldIterator<FProperty> It(FArcItemSpec::StaticStruct()); It; ++It )
		{
			for( int32 i=0; i < It->ArrayDim; i++ )
			{
				if( !It->Identical_InContainer(this,&Other, i,0) )
				{
					return false;
				}
			}
		}
		return true;
	}

	bool operator==(const FArcItemId& Other) const
	{
		return ItemId == Other;
	}

	bool operator==(const FPrimaryAssetId& Other) const
	{
		return ItemDefinitionId == Other;
	}
	
	FArcItemSpec& operator=(const FArcItemSpec& Other)
	{
		ItemId = Other.ItemId;
		Amount = Other.Amount;
		Level = Other.Level;
		ItemDefinitionId = Other.ItemDefinitionId;
		InstanceData = Other.InstanceData;
		ItemDefinition = Other.ItemDefinition;
		InitialInstanceData = Other.InitialInstanceData;

		return *this;
	}

	FArcItemSpec& operator=(FArcItemSpec&& Other)
	{
		ItemId = Other.ItemId;
		Amount = Other.Amount;
		Level = Other.Level;
		ItemDefinitionId = Other.ItemDefinitionId;
		InstanceData = MoveTemp(Other.InstanceData);
		ItemDefinition = MoveTemp(Other.ItemDefinition);
		InitialInstanceData = MoveTemp(Other.InitialInstanceData);
		Other.ItemDefinition = nullptr;

		return *this;
	}

	friend uint64 GetTypeHash(const FArcItemSpec& In)
	{
		return GetTypeHash(In.ItemId);
	}

	const FArcItemSpecFragmentInstances& GetInstanceData() const
	{
		return InstanceData;
	}
	
	void SetInstanceData(const FArcItemSpecFragmentInstances& Other)
	{
		InstanceData = Other;
	}

	template<typename T>
	TArray<const T*> TGetSpecFragments() const
	{
		TArray<const T*> Out;
		if (T::StaticStruct()->IsChildOf(FArcItemFragment_ItemInstanceBase::StaticStruct()))
		{
			TArray<const FArcItemFragment_ItemInstanceBase*> Array = InstanceData.GetDataArray();
			
			for (const FArcItemFragment_ItemInstanceBase* Item : Array)
			{
				Out.Add(Item);
			}
		}

		return Out;
	}

	TArray<const FArcItemFragment_ItemInstanceBase*> GetSpecFragments() const
	{
		return InstanceData.GetDataArray();
	}

	template<typename T>
	const T* FindFragment() const
	{
		if (T::StaticStruct()->IsChildOf(FArcItemFragment_ItemInstanceBase::StaticStruct()) == false)
		{
			return nullptr;
		}
		return reinterpret_cast<const T*>(InstanceData.Get<T>());
		
		//TArray<const FArcItemFragment_ItemInstanceBase*> Array = InstanceData.GetDataArray();
		//for (const FArcItemFragment_ItemInstanceBase* Item : Array)
		//{
		//	if (Item->GetScriptStruct()->IsChildOf(T::StaticStruct()))
		//	{
		//		return reinterpret_cast<const T*>(Item);
		//	}
		//}
		//
		//return nullptr;
	}

	
	const uint8* FindFragment(UScriptStruct* InType) const
	{
		if (InType->IsChildOf(FArcItemFragment_ItemInstanceBase::StaticStruct()) == false)
		{
			return nullptr;
		}

		TArray<const FArcItemFragment_ItemInstanceBase*> Array = InstanceData.GetDataArray();
		for (const FArcItemFragment_ItemInstanceBase* Item : Array)
		{
			if (Item->GetScriptStruct()->IsChildOf(InType))
			{
				return reinterpret_cast<const uint8*>(Item);
			}
		}

		return nullptr;
	}

	
	template<typename T>
    T* FindFragmentMutable() const
    {
    	if (T::StaticStruct()->IsChildOf(FArcItemFragment_ItemInstanceBase::StaticStruct()) == false)
    	{
    		return nullptr;
    	}
    
    	TArray<const FArcItemFragment_ItemInstanceBase*> Array = InstanceData.GetDataArray();
    	for (const FArcItemFragment_ItemInstanceBase* Item : Array)
    	{
    		if (Item->GetScriptStruct()->IsChildOf(T::StaticStruct()))
    		{
    			return const_cast<T*>(static_cast<const T*>(Item));
    		}
    	}
    
    	return nullptr;
    }
};
