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

#include "ArcCore/AbilitySystem/ArcAttributeSet.h"

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "GameplayEffectExtension.h"

void UArcAttributeSet::SetArcASC(class UArcCoreAbilitySystemComponent* InNxASC)
{
	ArcASC = InNxASC;
}

UArcCoreAbilitySystemComponent* UArcAttributeSet::GetArcASC() const
{
	if (ArcASC.IsValid())
	{
		return ArcASC.Get();	
	}

	if (UArcCoreAbilitySystemComponent* Result = Cast<UArcCoreAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
	{
		ArcASC = Result;
	}
	return ArcASC.Get();
}

void UArcAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute
										  , float& NewValue)
{
	auto Handler = PreAttributeHandlers.Find(Attribute);
	if (Handler)
	{
		(*Handler)(Attribute, NewValue);
	}
	
	if (ArcASC.IsValid())
	{
		ArcASC->PreAttributeChanged(Attribute, NewValue);
	}
}

void UArcAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute
	, float OldValue
	, float NewValue)
{
	auto Handler = PostAttributeHandlers.Find(Attribute);
	if (Handler)
	{
		(*Handler)(Attribute, OldValue, NewValue);
	}
	
	if (ArcASC.IsValid())
	{
		ArcASC->PostAttributeChanged(Attribute, OldValue, NewValue);
	}
}

void UArcAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	auto Handler = AttributeHandlers.Find(Data.EvaluatedData.Attribute);
	if (Handler)
	{
		(*Handler)(Data);
	}

	if (ArcASC.IsValid() && ArcASC->GetOwner())
	{
		ArcASC->ForceReplication();	
	}
	
}

void UArcAttributeSet::OnAttributeAggregatorCreated(const FGameplayAttribute& Attribute
	, FAggregator* NewAggregator) const
{
	Aggregators.FindOrAdd(Attribute) = NewAggregator;
}
