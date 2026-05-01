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
#include "StructUtils/InstancedStruct.h"
#include "ArcItemId.h"

#include "ArcItemInstance.generated.h"

struct FGameplayTag;
struct FArcItemData;
class UScriptStruct;
class UArcItemsStoreComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogItemInstance
	, Log
	, Log);

USTRUCT()
struct ARCCORE_API FArcItemInstance
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UArcItemsStoreComponent> IC;
	FArcItemData* OwningItem = nullptr;

public:
	virtual ~FArcItemInstance();

	virtual bool Equals(const FArcItemInstance& Other) const PURE_VIRTUAL(FArcItemInstance::Equals, return false; );

	bool operator==(const FArcItemInstance& Other) const
	{
		return Equals(Other);
	}

	bool operator!=(const FArcItemInstance& Other) const
	{
		return !(*this == Other);
	}

	virtual FString ToString() const
	{
		return StaticStruct()->GetName();
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
struct FArcItemInstanceArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FInstancedStruct> Data;

	bool operator==(const FArcItemInstanceArray& Other) const
	{
		return Data == Other.Data;
	}

	bool operator!=(const FArcItemInstanceArray& Other) const
	{
		return Data != Other.Data;
	}
};