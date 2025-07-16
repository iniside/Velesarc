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

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcInteractionStateChangePreset.generated.h"

USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionCustomData
{
	GENERATED_BODY()
};

class UArcItemsStoreComponent;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionData_TargetItemsStore : public FArcInteractionCustomData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UArcItemsStoreComponent> TargetItemStoreClass;
};
/**
 * 
 */
UCLASS()
class ARCCORE_API UArcInteractionStateChangePreset : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, meta=(BaseStruct = "/Script/ArcCore.ArcInteractionStateChange", ExcludeBaseStruct))
	TArray<FInstancedStruct> StateChangeActions;
	
	UPROPERTY(EditAnywhere, meta=(BaseStruct = "/Script/ArcCore.ArcInteractionCustomData", ExcludeBaseStruct))
	TArray<FInstancedStruct> CustomData;

	template<typename T>
	const T* FindCustomData() const
	{
		for (const FInstancedStruct& InstancedStruct : CustomData)
		{
			if (InstancedStruct.GetScriptStruct()->IsChildOf(T::StaticStruct()))
			{
				return InstancedStruct.GetPtr<T>();
			}
		}

		return nullptr;
	}
};
