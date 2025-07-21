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
#include "ArcItemData.h"
#include "ArcItemsStoreComponent.h"

class UArcItemsSubsystem;

struct ARCCORE_API ArcItems
{
	template <typename T>
	static const T* GetFragment(class UArcItemDefinition* InItem)
	{
		return InItem->FindFragment<T>();
	}

	template <typename T>
	static const T* GetFragment(const FArcItemData* E)
	{
		return E->GetItemDefinition()->FindFragment<T>();
	}

	/*
	 * Try to find Data Fragment in this instance of item and any attached items.
	 */
	template <typename T>
	static const T* FindFragment(const FArcItemData* E)
	{
		{
			const T* Ptr = E->GetItemDefinition()->FindFragment<T>();
			if (Ptr != nullptr)
			{
				return Ptr;
			}

			const T* SpecFrag = E->FindSpecFragment<T>();
			if (SpecFrag != nullptr)
			{
				return SpecFrag;
			}
		}

		
		if (const FArcItemData* OwnerPtr = E->GetOwnerPtr())
		{
			{
				const T* Ptr = OwnerPtr->GetItemDefinition()->FindFragment<T>();
				if (Ptr != nullptr)
				{
					return Ptr;
				}

				const T* SpecFrag = OwnerPtr->FindSpecFragment<T>();
				if (SpecFrag != nullptr)
				{
					return SpecFrag;
				}
			}
			{
				for (const FArcItemData* SocketItem : OwnerPtr->GetItemsInSockets())
				{
					const T* Ptr = SocketItem->GetItemDefinition()->FindFragment<T>();
					if (Ptr != nullptr)
					{
						return Ptr;
					}

					const T* SpecFrag = SocketItem->FindSpecFragment<T>();
					if (SpecFrag != nullptr)
					{
						return SpecFrag;
					}
				}
			}
		};
		
		for (const FArcItemData* SocketItem : E->GetItemsInSockets())
		{
			const T* Ptr = SocketItem->GetItemDefinition()->FindFragment<T>();
			if (Ptr != nullptr)
			{
				return Ptr;
			}

			const T* SpecFrag = SocketItem->FindSpecFragment<T>();
			if (SpecFrag != nullptr)
			{
				return SpecFrag;
			}
		}

		return nullptr;
	}

	template <typename T>
	static const T* FindScalableItemFragment(const FArcItemData* E)
	{
		return E->FindScalableItemFramgnet<T>();
	}
	
	static const uint8* FindFragment(const FArcItemData* E, UScriptStruct* InType)
	{
		{
			const uint8* Ptr = E->GetItemDefinition()->GetFragment(InType);
			if (Ptr != nullptr)
			{
				return Ptr;
			}

			const uint8* SpecFrag = E->FindSpecFragment(InType);
			if (SpecFrag != nullptr)
			{
				return SpecFrag;
			}
		}

		
		if (const FArcItemData* OwnerPtr = E->GetOwnerPtr())
		{
			{
				const uint8* Ptr = OwnerPtr->GetItemDefinition()->GetFragment(InType);
				if (Ptr != nullptr)
				{
					return Ptr;
				}

				const uint8* SpecFrag = OwnerPtr->FindSpecFragment(InType);
				if (SpecFrag != nullptr)
				{
					return SpecFrag;
				}
			}
			{
				for (const FArcItemData* SocketItem : OwnerPtr->GetItemsInSockets())
				{
					const uint8* Ptr = SocketItem->GetItemDefinition()->GetFragment(InType);
					if (Ptr != nullptr)
					{
						return Ptr;
					}

					const uint8* SpecFrag = SocketItem->FindSpecFragment(InType);
					if (SpecFrag != nullptr)
					{
						return SpecFrag;
					}
				}
			}
		};
		
		for (const FArcItemData* SocketItem : E->GetItemsInSockets())
		{
			const uint8* Ptr = SocketItem->GetItemDefinition()->GetFragment(InType);
			if (Ptr != nullptr)
			{
				return Ptr;
			}

			const uint8* SpecFrag = SocketItem->FindSpecFragment(InType);
			if (SpecFrag != nullptr)
			{
				return SpecFrag;
			}
		}

		return nullptr;
	}

	static void BroadcastItemChanged(const FArcItemData* InItem);

	template<typename T>
	static T* FindMutableInstance(const FArcItemData* InItem)
	{
		if (TSharedPtr<FArcItemInstance>* WeakPtr = const_cast<FArcItemData*>(InItem)->InstancedData.Find(T::StaticStruct()->GetFName()))
		{
			TSharedPtr<T> Casted =  StaticCastSharedPtr<T>(*WeakPtr);
			//const_cast<FArcItemData*>(InItem)->OwnerComponent->MarkItemInstanceDirtyById(InItem->GetItemId(), T::StaticStruct());
			const_cast<FArcItemData*>(InItem)->OwnerComponent->MarkItemDirtyById(InItem->GetItemId());
			return Casted.Get();
		}

		if (const FArcItemData* OwnerPtr = InItem->GetOwnerPtr())
		{
			if (TSharedPtr<FArcItemInstance>* Ptr = const_cast<FArcItemData*>(OwnerPtr)->InstancedData.Find(T::StaticStruct()->GetFName()))
			{
				TSharedPtr<T> Casted =  StaticCastSharedPtr<T>(*Ptr);
				//const_cast<FArcItemData*>(InItem)->OwnerComponent->MarkItemInstanceDirtyById(OwnerPtr->GetItemId(), T::StaticStruct());
				const_cast<FArcItemData*>(InItem)->OwnerComponent->MarkItemDirtyById(OwnerPtr->GetItemId());
				return Casted.Get();
			}
			// Last Resort: Try to find instance in attached items
			for (const FArcItemData* SocketItem : OwnerPtr->GetItemsInSockets())
			{
				if (TSharedPtr<FArcItemInstance>* WeakPtr = const_cast<FArcItemData*>(SocketItem)->InstancedData.Find(T::StaticStruct()->GetFName()))
				{
					TSharedPtr<T> Casted =  StaticCastSharedPtr<T>(*WeakPtr);
					//const_cast<FArcItemData*>(SocketItem)->OwnerComponent->MarkItemInstanceDirtyById(SocketItem->GetItemId(), T::StaticStruct());
					const_cast<FArcItemData*>(SocketItem)->OwnerComponent->MarkItemDirtyById(SocketItem->GetItemId());
					return Casted.Get();
				}
			}
		}

		// Last Resort: Try to find instance in attached items
		for (const FArcItemData* SocketItem : InItem->GetItemsInSockets())
		{
			if (TSharedPtr<FArcItemInstance>* WeakPtr = const_cast<FArcItemData*>(SocketItem)->InstancedData.Find(T::StaticStruct()->GetFName()))
			{
				TSharedPtr<T> Casted =  StaticCastSharedPtr<T>(*WeakPtr);
				//const_cast<FArcItemData*>(SocketItem)->OwnerComponent->MarkItemInstanceDirtyById(SocketItem->GetItemId(), T::StaticStruct());
				const_cast<FArcItemData*>(SocketItem)->OwnerComponent->MarkItemDirtyById(SocketItem->GetItemId());
				return Casted.Get();
			}
		}
		
		return nullptr;
	}

	
	static FArcItemInstance* FindMutableInstance(const FArcItemData* InItem, UScriptStruct* InstanceType)
	{
		if (TSharedPtr<FArcItemInstance>* WeakPtr = const_cast<FArcItemData*>(InItem)->InstancedData.Find(InstanceType->GetFName()))
		{
			//const_cast<FArcItemData*>(InItem)->OwnerComponent->MarkItemInstanceDirtyById(InItem->GetItemId(), T::StaticStruct());
			const_cast<FArcItemData*>(InItem)->OwnerComponent->MarkItemDirtyById(InItem->GetItemId());
			return (*WeakPtr).Get();
		}

		if (const FArcItemData* OwnerPtr = InItem->GetOwnerPtr())
		{
			if (TSharedPtr<FArcItemInstance>* Ptr = const_cast<FArcItemData*>(OwnerPtr)->InstancedData.Find(InstanceType->GetFName()))
			{
				//const_cast<FArcItemData*>(InItem)->OwnerComponent->MarkItemInstanceDirtyById(OwnerPtr->GetItemId(), T::StaticStruct());
				const_cast<FArcItemData*>(InItem)->OwnerComponent->MarkItemDirtyById(OwnerPtr->GetItemId());
				return (*Ptr).Get();
			}
			// Last Resort: Try to find instance in attached items
			for (const FArcItemData* SocketItem : OwnerPtr->GetItemsInSockets())
			{
				if (TSharedPtr<FArcItemInstance>* WeakPtr = const_cast<FArcItemData*>(SocketItem)->InstancedData.Find(InstanceType->GetFName()))
				{
					//const_cast<FArcItemData*>(SocketItem)->OwnerComponent->MarkItemInstanceDirtyById(SocketItem->GetItemId(), T::StaticStruct());
					const_cast<FArcItemData*>(SocketItem)->OwnerComponent->MarkItemDirtyById(SocketItem->GetItemId());
					return (*WeakPtr).Get();
				}
			}
		}

		// Last Resort: Try to find instance in attached items
		for (const FArcItemData* SocketItem : InItem->GetItemsInSockets())
		{
			if (TSharedPtr<FArcItemInstance>* WeakPtr = const_cast<FArcItemData*>(SocketItem)->InstancedData.Find(InstanceType->GetFName()))
			{
				//const_cast<FArcItemData*>(SocketItem)->OwnerComponent->MarkItemInstanceDirtyById(SocketItem->GetItemId(), T::StaticStruct());
				const_cast<FArcItemData*>(SocketItem)->OwnerComponent->MarkItemDirtyById(SocketItem->GetItemId());
				return (*WeakPtr).Get();
			}
		}
		
		return nullptr;
	}
	
	template<typename T>
	static const T* FindInstance(const FArcItemData* InItem)
	{
		if (const TSharedPtr<FArcItemInstance>* WeakPtr = InItem->InstancedData.Find(T::StaticStruct()->GetFName()))
		{
			TSharedPtr<T> Casted =  StaticCastSharedPtr<T>(*WeakPtr);
			
			return Casted.Get();
		}

		if (const FArcItemData* OwnerPtr = InItem->GetOwnerPtr())
		{
			if (const TSharedPtr<FArcItemInstance>* Ptr = OwnerPtr->InstancedData.Find(T::StaticStruct()->GetFName()))
			{
				TSharedPtr<T> Casted =  StaticCastSharedPtr<T>(*Ptr);
			
				return Casted.Get();
			}
			
			for (const FArcItemData* SocketItem : OwnerPtr->GetItemsInSockets())
			{
				if (const TSharedPtr<FArcItemInstance>* Ptr = SocketItem->InstancedData.Find(T::StaticStruct()->GetFName()))
				{
					TSharedPtr<T> Casted =  StaticCastSharedPtr<T>(*Ptr);
			
					return Casted.Get();
				}	
			}
			
		}

		// Last Resort: Try to find instance in attached items
		for (const FArcItemData* SocketItem : InItem->GetItemsInSockets())
		{
			if (const TSharedPtr<FArcItemInstance>* Ptr = SocketItem->InstancedData.Find(T::StaticStruct()->GetFName()))
			{
				TSharedPtr<T> Casted =  StaticCastSharedPtr<T>(*Ptr);
		
				return Casted.Get();
			}	
		}
		
		return nullptr;
	}

	template<typename T>
	static void AddInstance(const FArcItemData* InItem)
	{
		InItem->GetItemsStoreComponent()->ItemsArray.AddItemInstance(InItem, T::StaticStruct());
	}
	
	template<typename T>
	static void ForEachInstance(const FArcItemData* InItem, TFunctionRef<void(const FArcItemData*, T*)> ForEachFunc)
	{
		for (const TPair<FName, TSharedPtr<FArcItemInstance>>& Pair : InItem->InstancedData)
		{
			if (Pair.Value->GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				T* Instance = static_cast<T*>(Pair.Value.Get());
				
				ForEachFunc(InItem, Instance);
			}
		}
	}
	
	template<typename T>
	static void ForEachFragment(const FArcItemData* InItem, TFunctionRef<void(const FArcItemData*, const T*)> ForEachFunc)
	{
		TArray<const T*> Fragments = InItem->GetItemDefinition()->GetFragmentsOfType<T>();

		for (const T* Fragment : Fragments)
		{
			ForEachFunc(InItem, Fragment);
		}

		InItem->ForSpecFragment(ForEachFunc);
	}
};