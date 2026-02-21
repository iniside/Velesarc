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

#include "Net/Serialization/FastArraySerializer.h"

#include "ArcItemId.h"
#include "HAL/UnrealMemory.h"

#include "ArcItemInstance.generated.h"

struct FGameplayTag;
struct FArcItemData;
class UScriptStruct;
class UArcItemsStoreComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogItemInstance
	, Log
	, Log);

struct FArcItemInstance;

namespace ArcItems
{
	/** Allocate and construct a polymorphic FArcItemInstance with a custom deleter that correctly calls Destruct + FMemory::Free. */
	template<typename T>
	TSharedPtr<T> AllocateInstance()
	{
		UScriptStruct* StructType = T::StaticStruct();
		void* Mem = FMemory::Malloc(StructType->GetCppStructOps()->GetSize(), StructType->GetCppStructOps()->GetAlignment());
		StructType->GetCppStructOps()->Construct(Mem);
		return TSharedPtr<T>(static_cast<T*>(Mem), [StructType](T* Ptr)
		{
			StructType->GetCppStructOps()->Destruct(Ptr);
			FMemory::Free(Ptr);
		});
	}

	/** Non-template version for runtime UScriptStruct*. */
	inline TSharedPtr<FArcItemInstance> AllocateInstance(UScriptStruct* InstanceType)
	{
		void* Mem = FMemory::Malloc(InstanceType->GetCppStructOps()->GetSize(), InstanceType->GetCppStructOps()->GetAlignment());
		InstanceType->GetCppStructOps()->Construct(Mem);
		return TSharedPtr<FArcItemInstance>(static_cast<FArcItemInstance*>(Mem), [InstanceType](FArcItemInstance* Ptr)
		{
			InstanceType->GetCppStructOps()->Destruct(Ptr);
			FMemory::Free(Ptr);
		});
	}
}
/**
 * Base struct for mutable Item/Slot data.
 *
 * Always override Equals function to make sure item can be correctly compared for replication !.
 */
USTRUCT()
struct ARCCORE_API FArcItemInstance
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UArcItemsStoreComponent> IC;
	TWeakPtr<FArcItemData> OwningItem;
	
public:
	virtual UScriptStruct* GetScriptStruct() const
	{
		return FArcItemInstance::StaticStruct();
	}
	
	virtual ~FArcItemInstance();
	virtual bool Equals(const FArcItemInstance& Other) const PURE_VIRTUAL(FArcItemInstance::Equals, return false; );
	
	bool operator==(const FArcItemInstance& Other) const
	{
		return Equals(Other);
	}

	bool operator==(const FArcItemInstance* Other) const
	{
		return Equals(*Other);
	}

	bool operator==(TSharedPtr<FArcItemInstance>& Other) const
	{
		return Equals(*Other.Get());
	}
	
	bool operator!=(const FArcItemInstance& Other) const
	{
		return !(*this == Other);
	}
	
	bool operator==(const UScriptStruct* Other) const
	{
		return GetScriptStruct() == Other;
	}
	
	virtual TSharedPtr<FArcItemInstance> Duplicate() const
	{
		TSharedPtr<FArcItemInstance> SharedPtr = ArcItems::AllocateInstance(GetScriptStruct());
		*SharedPtr = *this;
		return SharedPtr;
	}

	virtual FString ToString() const
	{
		return GetScriptStruct()->GetName();
	}

	virtual bool ShouldPersist() const { return false; }
};

template <>
struct TStructOpsTypeTraits<FArcItemInstance> : public TStructOpsTypeTraitsBase2<FArcItemInstance>
{
	enum
	{
		WithPureVirtual = true
	};
};

USTRUCT()
struct ARCCORE_API FArcItemInstance_ItemData : public FArcItemInstance
{
	GENERATED_BODY()
};

USTRUCT()
struct FArcItemInstanceInternal
{
	GENERATED_BODY()

public:
	TSharedPtr<FArcItemInstance> Data;

	FArcItemInstanceInternal(TSharedPtr<FArcItemInstance> InInstance)
	{
		Data = InInstance;
	}
	FArcItemInstanceInternal() { }

	FArcItemInstanceInternal(const FArcItemInstanceInternal& Other)
	{
		*this = Other;
	}

	FArcItemInstanceInternal& operator=(FArcItemInstanceInternal&& Other)
	{
		Data = MoveTemp(Other.Data); return *this;
	}

	FArcItemInstanceInternal& operator=(const FArcItemInstanceInternal& Other) 
	{
		// TODO: I'm not entirely sure if we need deep copy, or shallow would be enough.
		//Data = Other.Data;
		//return *this;
		// When used as a replicated property we need to do deep copy as we store copies of the state in order to detect changes
		if (this != &Other)
		{
			const UScriptStruct* DstScriptStruct = Data.IsValid() ? Data->GetScriptStruct() : nullptr;
			const UScriptStruct* SrcScriptStruct = Other.Data.IsValid() ? Other.Data->GetScriptStruct() : nullptr;

			const FArcItemInstance* SrcPolymorphicStruct = Other.Data.IsValid() ? Other.Data.Get() : nullptr;
			if (SrcPolymorphicStruct)
			{
				CA_ASSUME(SrcScriptStruct != nullptr);
				FArcItemInstance* const DstPolymorphicStruct = Data.IsValid() ? Data.Get() : nullptr;
				if (DstPolymorphicStruct == SrcPolymorphicStruct)
				{
					SrcScriptStruct->CopyScriptStruct(DstPolymorphicStruct, SrcPolymorphicStruct);
				}
				else
				{
					if (DstPolymorphicStruct && SrcPolymorphicStruct)
					{
						SrcScriptStruct->CopyScriptStruct(DstPolymorphicStruct, SrcPolymorphicStruct);
						UE_LOG(LogItemInstance, Log, TEXT("FArcItemInstanceInternal operator= Copy Src to Dest %s"), *GetNameSafe(DstPolymorphicStruct->GetScriptStruct()))
					}
					else
					{
						UE_LOG(LogItemInstance, Log, TEXT("FArcItemInstanceInternal operator= Allocation %s"), *GetNameSafe(SrcPolymorphicStruct->GetScriptStruct()))
						Data = ArcItems::AllocateInstance(const_cast<UScriptStruct*>(SrcScriptStruct));
						SrcScriptStruct->CopyScriptStruct(Data.Get(), SrcPolymorphicStruct);	
					}
													
				}				
			}
			else
			{
				Data.Reset();
			}
		}

		return *this;
	}

	void Reset()
	{
		Data.Reset();
	}

	bool IsValid() const
	{
		return Data.IsValid();
	}

	/** Initalize Data to a struct of wanted type */
	template<typename T>
	void Raise()
	{
		Data = TSharedPtr<T>(new T);
	}

	/** Get data as expected type */
	template<typename T>
	T& GetAs()
	{
		check(Data.Get()->GetScriptStruct() == T::StaticStruct());
		return *static_cast<T*>(Data.Get());
	}

	/** Comparison operator that compares instance data */
	bool operator==(const FArcItemInstanceInternal& Other) const
	{
		if (Data.IsValid() != Other.Data.IsValid())
		{
			return false;
		}
		
		return Data->Equals(*Other.Data.Get());
		// When used as replicated property and we want to detect changes of the intance data we need to compare the data stored in the inner structs.
		//const FArcItemInstance* PolymorphicStruct = Data.Get();
		//const FArcItemInstance* OtherPolymorphicStruct = Other.Data.Get();
		//
		//const UScriptStruct* PolymorphicStructScriptStruct = PolymorphicStruct ? PolymorphicStruct->GetScriptStruct() : nullptr;
		//const UScriptStruct* OtherPolymorphicStructScriptStruct = OtherPolymorphicStruct ? OtherPolymorphicStruct->GetScriptStruct() : nullptr;
		//
		//if (PolymorphicStructScriptStruct != OtherPolymorphicStructScriptStruct)
		//{
		//	UE_LOG(LogItemInstance, Log, TEXT("FArcItemInstanceInternal operator== Deep Compare  Pointers mismatch"))
		//	return false;
		//}
		//
		//if (PolymorphicStructScriptStruct)
		//{
		//	UE_LOG(LogItemInstance, Log, TEXT("FArcItemInstanceInternal operator== Deep Compare %s"), *GetNameSafe(PolymorphicStructScriptStruct))
		//	// Compare actual struct data	
		//	return PolymorphicStructScriptStruct->CompareScriptStruct(PolymorphicStruct, OtherPolymorphicStruct, 0);
		//}
		//else
		//{
		//	return false;
		//}		
	}

	bool operator!=(const FArcItemInstanceInternal& Other) const
	{
		if (Data.IsValid() != Other.Data.IsValid())
		{
			return true;
		}
		const bool bValue = !Data->Equals(*Other.Data.Get());
		return bValue;
	}
};

template<>
struct TStructOpsTypeTraits<FArcItemInstanceInternal> : public TStructOpsTypeTraitsBase2<FArcItemInstanceInternal>
{
	enum
	{
		WithCopy = true,					// Necessary to do a deep copy
		WithIdenticalViaEquality = true,	// We have a custom compare operator
	};
};

// Wrapper To Allow different polymorphic types.
USTRUCT()
struct FArcItemInstanceArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcItemInstanceInternal> Data;

	FArcItemInstanceArray() { Data.Reset(); }

	FArcItemInstanceArray(FArcItemInstance* DataPtr)
	{
		Data.Add(TSharedPtr<FArcItemInstance>(DataPtr));
	}

	FArcItemInstanceArray(FArcItemInstanceArray&& Other) : Data(MoveTemp(Other.Data))	{ }
	FArcItemInstanceArray(const FArcItemInstanceArray& Other)
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

	FArcItemInstanceArray& operator=(FArcItemInstanceArray&& Other) { Data = MoveTemp(Other.Data); return *this; }
	FArcItemInstanceArray& operator=(const FArcItemInstanceArray& Other)
	{
		if (Data.Num() == Other.Data.Num())
		{
			// TODO: This should work as we always the same number of items.
			for (int32 Idx = 0; Idx < Data.Num(); Idx++)
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
	
	bool operator==(const FArcItemInstanceArray& Other) const
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

			const bool bValue = (Data[It] != Other.Data[It]);
			// Deep Comparison.
			if ( bValue )
			{
				return false;
			}
		}
		return true;
	}

	/** Comparison operator */
	bool operator!=(const FArcItemInstanceArray& Other) const
	{
		return !(FArcItemInstanceArray::operator==(Other));
	}
};

template<>
struct TStructOpsTypeTraits<FArcItemInstanceArray> : public TStructOpsTypeTraitsBase2<FArcItemInstanceArray>
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FExamplePolymorphicArrayItem> Data is copied around
		WithIdenticalViaEquality = true,
	};
};