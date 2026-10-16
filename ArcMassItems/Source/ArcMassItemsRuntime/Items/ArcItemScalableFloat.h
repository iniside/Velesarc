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

#include "ArcScalableFloat.h"
#include "ArcItemScalableFloat.generated.h"

struct FArcScalableFloatItemFragment;

/**
 * Lightweight wrapper that points at a single `FArcScalableFloat` property on a
 * specific `FArcScalableFloatItemFragment` subtype. Carries the owner struct so
 * `FArcItemData::GetValue` can route to the right per-item fragment instance.
 *
 * Earlier versions dispatched between an `FScalableFloat` property and a
 * `UCurveFloat*` property via an `EArcScalableType` enum; that two-source
 * pattern was folded into `FArcScalableFloat` itself (inline `FRichCurve`
 * source). Only the `FArcScalableFloat` branch remains.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcScalableCurveFloat
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TFieldPath<FStructProperty> ScalableFloat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data", meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcScalableFloatItemFragment"))
	TObjectPtr<UScriptStruct> OwnerType;

	FArcScalableCurveFloat() = default;

	FArcScalableCurveFloat(FStructProperty* InStructProperty, UScriptStruct* InOwnerType)
		: ScalableFloat(InStructProperty)
		, OwnerType(InOwnerType)
	{
	}

	const UScriptStruct* GetOwnerType() const
	{
		return OwnerType;
	}

	float GetValue(const FArcScalableFloatItemFragment* InItem
				   , float Level
				   , const FArcScalableFloatContext& Context = FArcScalableFloatContext()) const
	{
		if (!ScalableFloat.Get())
		{
			return 0.f;
		}
		const FArcScalableFloat* DataPtr = ScalableFloat->ContainerPtrToValuePtr<FArcScalableFloat>(InItem);
		return DataPtr->GetScaledValue(Level, Context);
	}

	float GetValue(const FArcScalableFloatItemFragment* InItem
				   , uint8 Level
				   , const FArcScalableFloatContext& Context = FArcScalableFloatContext()) const
	{
		return GetValue(InItem, static_cast<float>(Level), Context);
	}
};

#define ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(ClassName, PropertyName)                                                \
                                                                                                                    \
public:                                                                                                             \
	static FArcScalableCurveFloat Get##PropertyName##Data()                                                         \
	{                                                                                                               \
		static FStructProperty* Prop =                                                                              \
			FindFProperty<FStructProperty>(ClassName::StaticStruct(), #PropertyName);                               \
		static FArcScalableCurveFloat Data(Prop, ClassName::StaticStruct());                                        \
		return Data;                                                                                                \
	}                                                                                                               \
                                                                                                                    \
	static float Get##PropertyName##Value(const FArcItemData* InItem)                                               \
	{                                                                                                               \
		return InItem->GetValue(Get##PropertyName##Data());                                                         \
	}                                                                                                               \
	static float Get##PropertyName##Value(const FArcScalableFloatItemFragment* InItem, float Level, const FArcScalableFloatContext& Context = FArcScalableFloatContext()) \
	{                                                                                                               \
		return Get##PropertyName##Data().GetValue(InItem, Level, Context);                                          \
	}                                                                                                               \
	float Get##PropertyName##Direct(const float Level, const FArcScalableFloatContext& Context = FArcScalableFloatContext()) const \
	{                                                                                                               \
		return PropertyName.GetScaledValue(Level, Context);                                                         \
	}                                                                                                               \
private:
