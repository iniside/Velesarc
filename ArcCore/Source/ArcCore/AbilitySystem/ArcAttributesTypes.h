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

#define ARC_GAMEPLAYATTRIBUTE_PROPERTY_STRUCT_GETTER(StructName, PropertyName)   \
	static FGameplayAttribute& Get##PropertyName##Attribute()                 \
	{                                                                            \
		static FProperty* Prop =                                                 \
			FindFProperty<FProperty>(StructName::StaticStruct(), #PropertyName); \
		static FGameplayAttribute A(Prop);                                    \
		return A;                                                                \
	}

#define ARC_GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)         \
	static FGameplayAttribute& Get##PropertyName##Attribute()               \
	{                                                                          \
		static FProperty* Prop =                                               \
			FindFProperty<FProperty>(ClassName::StaticClass(), #PropertyName); \
		static FGameplayAttribute A(Prop);                                  \
		return A;                                                              \
	}

#define ARC_GAMEPLAYATTRIBUTE_VALUE__FILTERED_GETTER(ClassName, PropertyName)      \
	float Get##PropertyName##Filtered(const FGameplayTagContainer& InFilter) const \
	{                                                                              \
		{                                                                          \
			FGameplayTagRequirements ReqTags;                                      \
			ReqTags.RequireTags = InFilter;                                        \
			return ArcASC->GetFilteredAttributeValue(                              \
				Get##PropertyName##Attribute(), ReqTags, FGameplayTagContainer()); \
		};                                                                         \
	}

#define ARC_GAMEPLAYATTRIBUTE_OVERRIDE_VALUE(PropertyName) \
	FORCEINLINE void Override##PropertyName##Value(        \
		float NewVal, FGameplayEffectModCallbackData Data) \
	{                                                      \
		ArcASC->ApplyModToAttributeCallback(               \
			Get##PropertyName##Attribute(),                \
			EGameplayModOp::Type::Override,                \
			NewVal,                                        \
			&Data);                                        \
	}

#define ARC_ATTRIBUTE_ACCESSORS(ClassName, PropertyName)                  \
	ARC_GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)        \
	ARC_GAMEPLAYATTRIBUTE_VALUE__FILTERED_GETTER(ClassName, PropertyName) \
	ARC_GAMEPLAYATTRIBUTE_OVERRIDE_VALUE(PropertyName)                    \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                          \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                          \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

#define ARC_ATTRIBUTE_CAPTURE_SOURCE(ClassName, AttributeName)                        \
	static FGameplayEffectAttributeCaptureDefinition AttributeName##SourceDef()       \
	{                                                                                 \
		static FGameplayEffectAttributeCaptureDefinition CaptureDef(                  \
			ClassName::Get##AttributeName##Attribute(),                               \
			EGameplayEffectAttributeCaptureSource::Source,                            \
			false);                                                                   \
		return CaptureDef;                                                            \
	}                                                                                 \
	static FGameplayEffectAttributeCaptureSpec Get##AttributeName##CaptureSpecSource( \
		UAbilitySystemComponent* InASC)                                               \
	{                                                                                 \
		check(InASC->GetAvatarActor()->GetLocalRole() != ROLE_SimulatedProxy);							  \
		static FGameplayEffectAttributeCaptureDefinition CaptureDef(                  \
			ClassName::Get##AttributeName##Attribute(),                               \
			EGameplayEffectAttributeCaptureSource::Source,                            \
			false);                                                                   \
		FGameplayEffectAttributeCaptureSpec CaptureSpec(CaptureDef);           \
		InASC->CaptureAttributeForGameplayEffect(CaptureSpec);                        \
		return CaptureSpec;                                                           \
	}

#define ARC_ATTRIBUTE_CAPTURE_TARGET(ClassName, AttributeName)                        \
	static FGameplayEffectAttributeCaptureDefinition AttributeName##TargetDef()       \
	{                                                                                 \
		static FGameplayEffectAttributeCaptureDefinition CaptureDef(                  \
			ClassName::Get##AttributeName##Attribute(),                               \
			EGameplayEffectAttributeCaptureSource::Target,                            \
			false);                                                                   \
		return CaptureDef;                                                            \
	}                                                                                 \
	static FGameplayEffectAttributeCaptureSpec Get##AttributeName##CaptureSpecTarget( \
		UAbilitySystemComponent* InASC)                                               \
	{                                                                                 \
			check(InASC->GetAvatarActor()->GetLocalRole() != ROLE_SimulatedProxy);							  \
			static FGameplayEffectAttributeCaptureDefinition CaptureDef(                  \
				ClassName::Get##AttributeName##Attribute(),                               \
				EGameplayEffectAttributeCaptureSource::Target,                            \
				false);                                                                   \
			FGameplayEffectAttributeCaptureSpec CaptureSpec(CaptureDef);           \
			InASC->CaptureAttributeForGameplayEffect(CaptureSpec);                        \
			return CaptureSpec;                                                        \
	}

#define ARC_DEFINE_ATTRIBUTE_CAPTUREDEF(S, P, T, B)                      \
	{                                                                    \
		P##Property = FindFieldChecked<FProperty>(S::StaticClass(), #P); \
		P##Def = FGameplayEffectAttributeCaptureDefinition(              \
			P##Property, EGameplayEffectAttributeCaptureSource::T, B);   \
	}

//  
#define ARC_GAMEPLAYATTRIBUTE_REPNOTIFY(ClassName, PropertyName, OldValue) \
{ \
	GetOwningAbilitySystemComponentChecked()->SetBaseAttributeValueFromReplication(Get##PropertyName##Attribute(), PropertyName, OldValue); \
	if (GetArcASC() && !Aggregators.Contains(Get##PropertyName##Attribute())) \
	{ \
		ArcASC->PostAttributeChanged(Get##PropertyName##Attribute(), OldValue.GetCurrentValue(), Get##PropertyName()); \
	} \
}