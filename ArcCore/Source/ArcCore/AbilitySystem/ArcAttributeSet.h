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
#include "ArcAttributeSet.generated.h"

class UArcCoreAbilitySystemComponent;
struct FGameplayEffectModCallbackData;

#define ARC_DEFINE_ATTRIBUTE_HANDLER(AttributeName)                    \
	TFunction<void(const struct FGameplayEffectModCallbackData& Data)> \
		AttributeName##Handler();

#define ARC_DECLARE_ATTRIBUTE_HANDLER(ClassName, AttributeName)        \
	TFunction<void(const struct FGameplayEffectModCallbackData& Data)> \
		ClassName::AttributeName##Handler()

#define ARC_REGISTER_ATTRIBUTE_HANDLER(AttributeName)              \
	AttributeHandlers.FindOrAdd(Get##AttributeName##Attribute()) = \
		AttributeName##Handler();

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

protected:
	mutable TWeakObjectPtr<UArcCoreAbilitySystemComponent> ArcASC;

	TMap<FGameplayAttribute, TFunction<void(const FGameplayEffectModCallbackData& Data)>> AttributeHandlers;
	mutable TMap<FGameplayAttribute, FAggregator*> Aggregators;
	
public:
	void SetArcASC(class UArcCoreAbilitySystemComponent* InNxASC);

	UArcCoreAbilitySystemComponent* GetArcASC() const;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute
									, float& NewValue) override;

	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	
	virtual void OnAttributeAggregatorCreated(const FGameplayAttribute& Attribute, FAggregator* NewAggregator) const override;
};
