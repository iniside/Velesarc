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
#include "ArcItemDefinitionData.h"

struct ARCMASSITEMSRUNTIME_API ArcItemsHelper
{
	template <typename T>
	static const T* GetFragment(const FArcItemDefinitionData* InItem)
	{
		return InItem != nullptr ? InItem->FindFragment<T>() : nullptr;
	}

	template <typename T>
	static const T* GetFragment(const FArcItemData* E)
	{
		return GetFragment<T>(E->GetItemDefinition());
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
			if (const T* Ptr = GetFragment<T>(Item->GetItemDefinition()))
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
			const FArcItemDefinitionData* Definition = Item->GetItemDefinition();
			if (const uint8* Ptr = Definition != nullptr ? Definition->GetFragment(InType) : nullptr)
				return Ptr;
			return Item->FindSpecFragment(InType);
		});
	}

	template<typename T>
	static T* FindMutableInstance(const FArcItemData* InItem)
	{
		// Mass-side dirty marking is the responsibility of the Mass operation that
		// called us — actor-side MarkItemDirtyById retired with UArcItemsStoreComponent.
		return TraverseHierarchy(InItem, [](const FArcItemData* Item) -> T*
		{
			FStructView* View = const_cast<FArcItemData*>(Item)->InstancedData.Find(T::StaticStruct());
			if (View)
			{
				return View->GetPtr<T>();
			}
			return nullptr;
		});
	}

	static FArcItemInstance* FindMutableInstance(const FArcItemData* InItem, UScriptStruct* InstanceType)
	{
		return TraverseHierarchy(InItem, [InstanceType](const FArcItemData* Item) -> FArcItemInstance*
		{
			FStructView* View = const_cast<FArcItemData*>(Item)->InstancedData.Find(InstanceType);
			if (View)
			{
				return View->GetPtr<FArcItemInstance>();
			}
			return nullptr;
		});
	}

	template<typename T>
	static const T* FindInstance(const FArcItemData* InItem)
	{
		return TraverseHierarchy(InItem, [](const FArcItemData* Item) -> const T*
		{
			const FStructView* View = Item->InstancedData.Find(T::StaticStruct());
			if (View)
			{
				return View->GetPtr<T>();
			}
			return nullptr;
		});
	}

	/*
	 * AddInstance retired with UArcItemsStoreComponent::ItemsArray. Mass-side
	 * instance lifecycle is owned by the entity's store fragment.
	 */

	template<typename T>
	static void ForEachInstance(const FArcItemData* InItem, TFunctionRef<void(const FArcItemData*, T*)> ForEachFunc)
	{
		for (TPair<const UScriptStruct*, FStructView>& Pair : const_cast<FArcItemData*>(InItem)->InstancedData)
		{
			if (Pair.Key->IsChildOf(T::StaticStruct()))
			{
				T* Instance = Pair.Value.GetPtr<T>();
				ForEachFunc(InItem, Instance);
			}
		}
	}

	template<typename T>
	static void ForEachFragment(const FArcItemData* InItem, TFunctionRef<void(const FArcItemData*, const T*)> ForEachFunc)
	{
		if (const FArcItemDefinitionData* Definition = InItem->GetItemDefinition())
		{
			TArray<const T*> Fragments = Definition->GetFragmentsOfType<T>();
			for (const T* Fragment : Fragments)
			{
				ForEachFunc(InItem, Fragment);
			}
		}

		InItem->ForSpecFragment(ForEachFunc);
	}
};
