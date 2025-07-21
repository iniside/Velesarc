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
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
class AActor;

namespace Arcx::Utils
{
	template <typename T>
	T* GetComponent(const AActor* Owner
					, TSubclassOf<T> Type)
	{
		return Cast<T>(Owner->GetComponentByClass(Type));
	}

	template <typename T>
	TArray<T*> GetComponents(AActor* Owner
					, TSubclassOf<T> Type)
	{
		TArray<T*> OutComponents;
		Owner->GetComponents<T>(OutComponents);
		return OutComponents;
	}

	template <typename T>
	TArray<T*> GetComponentsChecked(AActor* Owner
					, TSubclassOf<T> Type)
	{
		TArray<T*> OutComponents;
		if(Owner == nullptr)
		{
			return OutComponents;
		}
		
		Owner->GetComponents<T>(OutComponents);
		
		return OutComponents;
	}

	template<typename T>
	TArray<T*> TObjectArrayToRaw(const TArray<TObjectPtr<T>>& InArray)
	{
		TArray<T*> Out;
		Out.Reserve(InArray.Num());
		for (const TObjectPtr<T>& Elem : InArray)
		{
			Out.Add(Elem.Get());
		}
		return Out;
	}
	ARCCORE_API bool IsPlayableWorld(const UWorld* InWorld);
}