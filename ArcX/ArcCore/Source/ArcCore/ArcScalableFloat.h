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

#include "AttributeSet.h"
#include "ScalableFloat.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcScalableFloat.generated.h"

class AActor;
class UArcCoreAbilitySystemComponent;
class UArcItemsStoreComponent;
class UArcItemDefinition;
struct FArcItemData;

struct FArcScalableFloatContext
{
	UArcCoreAbilitySystemComponent* AbilitySystemComponent = nullptr;
	AActor* Owner = nullptr;
	AActor* Avatar = nullptr;
	const FArcItemData* ItemData = nullptr;
	UArcItemsStoreComponent* ItemsStore = nullptr;
	const UArcItemDefinition* ItemDefinition = nullptr;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcScalableFloatValueScaling
{
	GENERATED_BODY()

public:
	virtual ~FArcScalableFloatValueScaling() = default;

	virtual float CalculateValue(const FScalableFloat& InValue, int32 Level, const FArcScalableFloatContext& Context) const
	{
		return InValue.GetValueAtLevel(Level);
	}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Gameplay Attribute Scaling"))
struct ARCCORE_API FArcGameplayAttributeScaling : public FArcScalableFloatValueScaling
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FGameplayAttribute Attribute;
	
	virtual ~FArcGameplayAttributeScaling() override = default;

	virtual float CalculateValue(const FScalableFloat& InValue, int32 Level, const FArcScalableFloatContext& Context) const override
	{
		return InValue.GetValueAtLevel(Level);
	}
};

/**
 * It exists only to make different customization for Scalable float which does not take two rows.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcScalableFloat : public FScalableFloat
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Config", meta = (BaseStruct = "/Script/ArcCore.ArcScalableFloatValueScaling", ExcludeBaseStruct))
	FInstancedStruct CustomScaling;

	float GetScaledValue(int32 Level, const FArcScalableFloatContext& Context = FArcScalableFloatContext()) const
	{
		if (const FArcScalableFloatValueScaling* Scaling = CustomScaling.GetPtr<FArcScalableFloatValueScaling>())
		{
			return Scaling->CalculateValue(*this, Level, Context);
		}

		return GetValueAtLevel(Level);
	}
	
	FArcScalableFloat()
		: FScalableFloat()
	{}

	FArcScalableFloat(float InInitialValue)
		: FScalableFloat(InInitialValue)
	{}
};