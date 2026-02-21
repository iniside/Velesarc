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
#include "ArcItemsSubsystem.h"

class UArcItemsSubsystem;

struct ARCCORE_API ArcItemsHelper
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
	 * Traverse item hierarchy: self → owner → owner's sockets → own sockets.
	 * Visitor returns a pointer type; first non-null result short-circuits.
	 */
	template<typename Func>
	static auto TraverseHierarchy(const FArcItemData* InItem, Func&& Visitor)
		-> decltype(Visitor(InItem))
	{
		using R = decltype(Visitor(InItem));
		if (R Result = Visitor(InItem)) return Result;

		if (const FArcItemData* OwnerPtr = InItem->GetOwnerPtr())
		{
			if (R Result = Visitor(OwnerPtr)) return Result;
			for (const FArcItemData* SocketItem : OwnerPtr->GetItemsInSockets())
			{
				if (R Result = Visitor(SocketItem)) return Result;
			}
		}

		for (const FArcItemData* SocketItem : InItem->GetItemsInSockets())
		{
			if (R Result = Visitor(SocketItem)) return Result;
		}

		return R{};
	}

	/*
	 * Try to find Data Fragment in this instance of item and any attached items.
	 */
	template <typename T>
	static const T* FindFragment(const FArcItemData* E)
	{
		return TraverseHierarchy(E, [](const FArcItemData* Item) -> const T*
		{
			if (const T* Ptr = Item->GetItemDefinition()->FindFragment<T>())
				return Ptr;
			return Item->FindSpecFragment<T>();
		});
	}

	template <typename T>
	static const T* FindScalableItemFragment(const FArcItemData* E)
	{
		return E->FindScalableItemFragment<T>();
	}

	static const uint8* FindFragment(const FArcItemData* E, UScriptStruct* InType)
	{
		return TraverseHierarchy(E, [InType](const FArcItemData* Item) -> const uint8*
		{
			if (const uint8* Ptr = Item->GetItemDefinition()->GetFragment(InType))
				return Ptr;
			return Item->FindSpecFragment(InType);
		});
	}

	static void BroadcastItemChanged(const FArcItemData* InItem);

	template<typename T>
	static T* FindMutableInstance(const FArcItemData* InItem)
	{
		UArcItemsSubsystem* ItemsSubsystem = UArcItemsSubsystem::Get(InItem->GetItemsStoreComponent());

		return TraverseHierarchy(InItem, [InItem, ItemsSubsystem](const FArcItemData* Item) -> T*
		{
			if (TSharedPtr<FArcItemInstance>* Ptr = const_cast<FArcItemData*>(Item)->InstancedData.Find(T::StaticStruct()->GetFName()))
			{
				const_cast<FArcItemData*>(Item)->OwnerComponent->MarkItemDirtyById(Item->GetItemId());
				ItemsSubsystem->OnItemChangedDynamic.Broadcast(InItem->GetItemsStoreComponent(), InItem->GetItemId());
				return StaticCastSharedPtr<T>(*Ptr).Get();
			}
			return nullptr;
		});
	}

	static FArcItemInstance* FindMutableInstance(const FArcItemData* InItem, UScriptStruct* InstanceType)
	{
		return TraverseHierarchy(InItem, [InstanceType](const FArcItemData* Item) -> FArcItemInstance*
		{
			if (TSharedPtr<FArcItemInstance>* Ptr = const_cast<FArcItemData*>(Item)->InstancedData.Find(InstanceType->GetFName()))
			{
				const_cast<FArcItemData*>(Item)->OwnerComponent->MarkItemDirtyById(Item->GetItemId());
				return Ptr->Get();
			}
			return nullptr;
		});
	}

	template<typename T>
	static const T* FindInstance(const FArcItemData* InItem)
	{
		return TraverseHierarchy(InItem, [](const FArcItemData* Item) -> const T*
		{
			if (const TSharedPtr<FArcItemInstance>* Ptr = Item->InstancedData.Find(T::StaticStruct()->GetFName()))
			{
				return StaticCastSharedPtr<T>(*Ptr).Get();
			}
			return nullptr;
		});
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