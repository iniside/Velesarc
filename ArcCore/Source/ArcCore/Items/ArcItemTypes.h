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

#include "AttributeSet.h"

#include "Engine/NetSerialization.h"
#include "ArcItemId.h"
#include "ScalableFloat.h"

#include "ArcItemTypes.generated.h"

class UArcItemsComponent;
struct FArcItemData;
struct FArcItemInstance;

DECLARE_MULTICAST_DELEGATE_OneParam(FArcGenericItemIdDelegate
	, FArcItemId);

DECLARE_MULTICAST_DELEGATE_TwoParams(FArcItemInstanceDelegate
	, const FArcItemData*
	, FArcItemInstance*);

UENUM(BlueprintType)
enum class EArcModType : uint8
{
	/* Set Mod to Additive and just pass value. */
	Additive = 0
	/* Set Mod to Additive and just pass value. */
	, Multiply = 1
	, Division = 2
	, MAX = 3
};

// TODO Need to trim the properties to what's absolutely needed.
// TODO Add individual level ?
USTRUCT(BlueprintType)
struct ARCCORE_API FArcItemAttributeStat
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, SaveGame, Category = "Data")
	EArcModType Type = EArcModType::Additive;
	
	UPROPERTY(EditAnywhere, SaveGame, Category = "Data")
	FScalableFloat Value;

	UPROPERTY(EditAnywhere, SaveGame, Category = "Data")
	FGameplayAttribute Attribute;
		
	FArcItemAttributeStat& SetValue(float InValue)
	{
		Value = InValue;
		return *this;
	}

	FArcItemAttributeStat& SetAttribute(const FGameplayAttribute& InAttribute)
	{
		Attribute = InAttribute;
		return *this;
	}

	FArcItemAttributeStat()
		: Value(0.0f)
		, Attribute(FGameplayAttribute())
	{
	};

	FArcItemAttributeStat(const FGameplayAttribute& InAttribute
						  , float InValue)
		: Value(InValue)
		, Attribute(InAttribute)
	{
	}

	FArcItemAttributeStat(const FArcItemAttributeStat& Other)
		: Value(Other.Value)
		, Attribute(Other.Attribute)
	{
	};

	bool operator==(const FGameplayAttribute& Other) const
	{
		return Attribute == Other;
	}

	bool operator==(const FArcItemAttributeStat& Other) const
	{
		return Attribute == Other.Attribute;
	}

	bool IsValid() const
	{
		return Attribute.IsValid();
	}
};



struct FArcStat
{
	float Value = 0;
	EArcModType Type = EArcModType::Additive;
	FArcItemId ItemId;

	bool operator==(const FArcItemId& Handle) const
	{
		return ItemId == Handle;
	}

	bool operator==(const FArcStat& Handle) const
	{
		return ItemId == Handle.ItemId;
	}
};
