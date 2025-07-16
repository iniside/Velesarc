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

#include "ArcScalableFloat.h"
#include "Curves/CurveFloat.h"
#include "ScalableFloat.h"
#include "ArcItemScalableFloat.generated.h"

struct FArcScalableFloatItemFragment;

UENUM(BlueprintType)
enum class EArcScalableType : uint8
{
	Scalable
	, Curve
	, None
};

/**
 * Wrapper around FScalableFloat and UCurveFloat, which allow for consistent access to values of both
 * by using reflection.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcScalableCurveFloat
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TFieldPath<FStructProperty> ScalableFloat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TFieldPath<FObjectProperty> CurveProp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data", meta = (MetaStruct = "/Script/ArcCore.ArcScalableFloatItemFragment"))
	TObjectPtr<UScriptStruct> OwnerType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	EArcScalableType Type;

public:
	FArcScalableCurveFloat()
		: ScalableFloat(nullptr)
		, OwnerType(nullptr)
		, Type(EArcScalableType::None)
	{
	};

	explicit FArcScalableCurveFloat(FStructProperty* InStructProperty
							   , UScriptStruct* InOwnerType)
		: ScalableFloat(InStructProperty)
		, OwnerType(InOwnerType)
		, Type(EArcScalableType::Scalable)
	{
	};

	explicit FArcScalableCurveFloat(FObjectProperty* InStructProperty
							   , UScriptStruct* InOwnerType)
		: CurveProp(InStructProperty)
		, OwnerType(InOwnerType)
		, Type(EArcScalableType::Curve)
	{
	};

	FArcScalableCurveFloat(const FArcScalableCurveFloat& Other)
		: ScalableFloat(Other.ScalableFloat)
		, CurveProp(Other.CurveProp)
		, OwnerType(Other.OwnerType)
		, Type(Other.Type)
	{
	};

	// should never be called, but we still need this because generated code is breaking
	// without it.
	FArcScalableCurveFloat operator=(const FArcScalableCurveFloat& Other)
	{
		unimplemented();
		return FArcScalableCurveFloat();
	}

	const UScriptStruct* GetOwnerType() const
	{
		return OwnerType;
	}

	float GetValue(const FArcScalableFloatItemFragment* InItem
				   , float Level) const
	{
		switch (Type)
		{
			case EArcScalableType::Scalable:
			{
				const FArcScalableFloat* DataPtr = ScalableFloat->ContainerPtrToValuePtr<FArcScalableFloat>(InItem);
				return DataPtr->GetScaledValue(Level);
			}
			case EArcScalableType::Curve:
			{
				const UCurveFloat* DataPtr = static_cast<const UCurveFloat*>(CurveProp->GetPropertyValue(
					CurveProp->ContainerPtrToValuePtr<UObject>(InItem)));
				return DataPtr->GetFloatValue(Level);
			}
			default:
				return 0.f;
		}
	}

	float GetValue(const FArcScalableFloatItemFragment* InItem
				   , uint8 Level) const
	{
		switch (Type)
		{
			case EArcScalableType::Scalable:
			{
				const FArcScalableFloat* DataPtr = ScalableFloat->ContainerPtrToValuePtr<FArcScalableFloat>(InItem);
				return DataPtr->GetScaledValue(static_cast<float>(Level));
			}
			case EArcScalableType::Curve:
			{
				const UCurveFloat* DataPtr = static_cast<const UCurveFloat*>(CurveProp->GetPropertyValue(CurveProp->ContainerPtrToValuePtr<UObject>(InItem)));
				return DataPtr->GetFloatValue(static_cast<float>(Level));
			}
			default:
				return 0.f;
		}
	}
};

#define ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(ClassName, PropertyName)                  							\
                                                                                      							\
public:                                                                               							\
	static FArcScalableCurveFloat Get##PropertyName##Data()                           							\
	{                                                                                 							\
		static FStructProperty* Prop =                                                							\
			FindFProperty<FStructProperty>(ClassName::StaticStruct(), #PropertyName); 							\
		static FArcScalableCurveFloat Data(Prop, ClassName::StaticStruct());          							\
		return Data;                                                                  							\
	}                                                                                 							\
                                                                                      							\
                                                                                      							\
	static float Get##PropertyName##Value(const FArcItemData* InItem)		 		  							\
	{                                                                                 							\
		return InItem->GetValue(Get##PropertyName##Data());					  		  							\
	}																				  							\
	static float Get##PropertyName##Value(const FArcScalableFloatItemFragment* InItem, float Level)		 		\
	{                                                                                 							\
		return Get##PropertyName##Data().GetValue(InItem, Level);					  							\
	}																											\
	float Get##PropertyName##Direct(const float Level) const							  						\
	{                                                                                 							\
		return PropertyName.GetScaledValue(Level);									  							\
	}																				  							\
private:

#define ARC_CURVE_GETTER(ClassName, PropertyName)                                     							\
                                                                                      							\
public:                                                                               							\
	static FArcScalableCurveFloat Get##PropertyName##Data()                           							\
	{                                                                                 							\
		static FObjectProperty* Prop =                                                							\
			FindFProperty<FObjectProperty>(ClassName::StaticStruct(), #PropertyName); 							\
		static FArcScalableCurveFloat Data(Prop, ClassName::StaticStruct());          							\
		return Data;                                                                  							\
	}                                                                                 							\
                                                                                      							\
    float Get##PropertyName##Value(const float Level) const							  							\
    {                                                                                 							\
        return PropertyName->GetFloatValue(Level);									  							\
    }                                                                                 							\
	static float Get##PropertyName##Value(const FArcItemData* InItem)		 		  							\
	{                                                                                 							\
		return InItem->GetValue(Get##PropertyName##Data());					  		  							\
	}																											\
																												\
	static float Get##PropertyName##Value(const FArcScalableFloatItemFragment* InItem, float Level)		 		\
	{                                                                                 							\
		return Get##PropertyName##Data().GetValue(InItem, Level);					  							\
	}                                                                                 							\
private:																			  							\
