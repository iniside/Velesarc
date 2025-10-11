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
#include "ArcCore/Items/ArcItemId.h"

#include "GameplayEffectTypes.h"

#include "MassEntitySubsystem.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "AbilitySystem/Targeting/ArcTargetDataId.h"

#include "ArcGameplayEffectContext.generated.h"

struct FArcItemData;
class UArcItemDefinition;
class UArcItemsStoreComponent;

USTRUCT()
struct ARCCORE_API FArcGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()
	friend struct FArcGameplayEffectContextNetSerializer;

public:
	virtual ~FArcGameplayEffectContext() override
	{
	}

protected:
	/*
	 * Item (as from NxItems) from which this Context originated. In case
	 * of ability it will be handle to the item which gave ability.
	 * Might be empty.
	 **/

public:
	UPROPERTY()
	FArcItemId SourceItemId;

	UPROPERTY()
	TObjectPtr<UArcItemsStoreComponent> ItemsStoreComponent = nullptr;
	
	UPROPERTY()
	mutable TObjectPtr<const UArcItemDefinition> SourceItem = nullptr;
	
	mutable FArcItemData* SourceItemPtr = nullptr;

	FArcTargetDataId TargetDataHandle;

	// it's only going to be valid on Authority.
	TArray<FMassEntityHandle> TargetEntities;

public:
	void SetSourceItemHandle(FArcItemId InSourceItemHandle);

	FArcItemId GetSourceItemHandle() const
	{
		return SourceItemId;
	}

	const FArcItemData* GetSourceItemPtr() const;

	const TWeakPtr<FArcItemData> GetSourceItemWeakPtr() const;

	void SetSourceItemId(const FArcItemId& InSourceItemId)
	{
		SourceItemId = InSourceItemId;
	}
	
	void SetItemStoreComponent(UArcItemsStoreComponent* InItemsStoreComponent)
	{
		ItemsStoreComponent = InItemsStoreComponent;
	}

	void SetSourceItemPtr(FArcItemData* InSourceItemPtr)
	{
		SourceItemPtr = InSourceItemPtr;
	}
	
	const UArcItemDefinition* GetSourceItem() const;

	void SetSourceItemDef(const UArcItemDefinition* InSourceItem)
	{
		SourceItem = InSourceItem;
	}
	
	FArcTargetDataId GetTargetDataHandle() const
	{
		return TargetDataHandle;
	}

	void SetTargetDataHandle(FArcTargetDataId InTargetDataHandle)
	{
		TargetDataHandle = InTargetDataHandle;
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcGameplayEffectContext::StaticStruct();
	}

	virtual FGameplayEffectContext* Duplicate() const override
	{
		FArcGameplayEffectContext* NewContext = new FArcGameplayEffectContext();
		*NewContext = *this;
		NewContext->AddActors(Actors);
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult()
				, true);
		}
		return NewContext;
	}

	virtual bool NetSerialize(FArchive& Ar
							  , class UPackageMap* Map
							  , bool& bOutSuccess) override;
};

template <>
struct TStructOpsTypeTraits<FArcGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FArcGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true
		, WithCopy = true // Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};

USTRUCT()
struct FArcHealthFragment : public FMassFragment
{
	GENERATED_BODY()

public:
	float Health = 0;
	float MaxHealth = 0;
};
