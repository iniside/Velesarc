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

#include "HAL/Platform.h"
#include "DataRegistryId.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/AssetRegistryTagsContext.h"

#include "ArcItemFragment.generated.h"


class UArcItemDefinition;
struct FAssetRegistryTag;

USTRUCT(meta = (Hidden, LoadBehavior = "LazyOnDemand"))
struct ARCCORE_API FArcItemFragment
{
	GENERATED_BODY()

public:
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
	{
	};

	virtual EDataValidationResult IsDataValid(const UArcItemDefinition* ItemDefinition, class FDataValidationContext& Context) const
	{
		return EDataValidationResult::NotValidated;
	}
	
	virtual ~FArcItemFragment()
	{
	}
};

struct FArcItemData;

UENUM()
enum class EArcItemInstanceCreation : uint8
{
	Always, // Always create item instance,
	OnOwnerWhenAttached, // Only create item instance when attached to owner, and add it to owner.  
	WhenAttached // when attached, create item own instance.
};

USTRUCT()
struct ARCCORE_API FArcItemFragment_ItemInstanceBase : public FArcItemFragment
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	bool bCreateInstance = true;
public:
	virtual ~FArcItemFragment_ItemInstanceBase() override = default;

	virtual void OnItemInitialize(const FArcItemData* InItem) const {};
	
	virtual void OnItemAdded(const FArcItemData* InItem) const {};
	virtual void OnItemChanged(const FArcItemData* InItem) const {};
	virtual void OnPreRemoveItem(const FArcItemData* InItem) const {};

	virtual void OnItemAddedToSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const {};
	virtual void OnItemRemovedFromSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const {};
	virtual void OnItemChangedSlot(const FArcItemData* InItem, const FGameplayTag& InNewSlotId, const FGameplayTag& InOldSlotId) const {};
	
	virtual void OnItemAttachedTo(const FArcItemData* InItem, const FArcItemData* InOwnerItem) const {};
	virtual void OnItemDetachedFrom(const FArcItemData* InItem, const FArcItemData* InOwnerItem) const {};

	bool GetCreateItemInstance() const
	{
		return bCreateInstance;
	}
	virtual UScriptStruct* GetItemInstanceType() const
	{
		return nullptr;
	}

	virtual UScriptStruct* GetScriptStruct() const
	{
		return FArcItemFragment_ItemInstanceBase::StaticStruct();
	}
};

USTRUCT()
struct ARCCORE_API FArcInstancedStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FInstancedStruct InstancedStruct;

	UPROPERTY()
	FName StructName;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bFromTemplate = false;
#endif
	
	FArcInstancedStruct()
	{
		StructName = NAME_None;
	}

	FArcInstancedStruct(const FInstancedStruct& Other)
		: InstancedStruct(Other)
	{
		if (Other.IsValid() == true)
		{
			StructName = Other.GetScriptStruct()->GetFName();	
		}
		
	}

	void PreSave()
	{
		if (InstancedStruct.IsValid() == true)
		{
			StructName = InstancedStruct.GetScriptStruct()->GetFName();	
		}
	}

	void InitializeAs(UScriptStruct* ScriptStruct, const uint8* Memory)
	{
		InstancedStruct.Reset();
		InstancedStruct.InitializeAs(ScriptStruct, Memory);
	}
	
	FArcInstancedStruct& operator=(const FInstancedStruct& InOther)
	{
		if (InstancedStruct.Identical(&InOther
				, PPF_None) == false)
		{
			InstancedStruct.InitializeAs(InOther.GetScriptStruct()
				, InOther.GetMemory());
			
			StructName = InOther.GetScriptStruct()->GetFName();
		}
		return *this;
	}

	bool operator==(const FArcInstancedStruct& In) const
	{
		return StructName == In.StructName;
	}

	bool operator==(const FName& In) const
	{
		return StructName == In;
	}
	
	bool operator==(UScriptStruct* In) const
	{
		return InstancedStruct.GetScriptStruct() == In;
	}

	friend uint32 GetTypeHash(const FArcInstancedStruct& In)
	{
		return GetTypeHash(In.StructName);
	}

	const UScriptStruct* GetScriptStruct() const
	{
		return InstancedStruct.GetScriptStruct();
	}

	const uint8* GetMemory() const
	{
		return InstancedStruct.GetMemory();
	}

	template <typename T>
	const T& Get() const
	{
		const uint8* Memory = InstancedStruct.GetMemory();
		const UScriptStruct* Struct = GetScriptStruct();
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(TBaseStructure<T>::Get()));
		return *((T*)Memory);
	}

	/** Returns const pointer to the struct, or nullptr if cast is not valid. */
	template <typename T>
	const T* GetPtr() const
	{
		const uint8* Memory = InstancedStruct.GetMemory();
		const UScriptStruct* Struct = GetScriptStruct();
		if (Memory != nullptr && Struct && Struct->IsChildOf(TBaseStructure<T>::Get()))
		{
			return ((T*)Memory);
		}
		return nullptr;
	}

	template <typename T>
	T& GetMutable()
	{
		uint8* Memory = InstancedStruct.GetMutableMemory();
		const UScriptStruct* Struct = InstancedStruct.GetScriptStruct();
		check(Memory != nullptr);
		check(Struct != nullptr);
		check(Struct->IsChildOf(TBaseStructure<T>::Get()));
		return *((T*)Memory);
	}

	/** Returns mutable pointer to the struct, or nullptr if cast is not valid. */
	template <typename T>
	T* GetMutablePtr()
	{
		uint8* Memory = InstancedStruct.GetMutableMemory();
		const UScriptStruct* Struct = InstancedStruct.GetScriptStruct();
		if (Memory != nullptr && Struct && Struct->IsChildOf(TBaseStructure<T>::Get()))
		{
			return ((T*)Memory);
		}
		return nullptr;
	}

	bool IsValid() const
	{
		return InstancedStruct.IsValid();
	}
};

USTRUCT(BlueprintType, meta = (Hidden, LoadBehavior = "LazyOnDemand"))
struct ARCCORE_API FArcScalableFloatItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Registry", meta = (AllowCategories))
	FDataRegistryType RegistryType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data Registry", meta = (Categories = "DataRegistry"))
	FGameplayTag DataName;

	FArcScalableFloatItemFragment()
		: RegistryType()
		, DataName()
	{
	};

	void Initialize(const UScriptStruct* InStruct);
};