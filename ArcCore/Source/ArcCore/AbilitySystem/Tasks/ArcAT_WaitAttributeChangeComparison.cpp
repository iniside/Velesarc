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

#include "ArcCore/AbilitySystem/Tasks/ArcAT_WaitAttributeChangeComparison.h"

#include "AbilitySystemComponent.h"

#include "AbilitySystemGlobals.h"
#include "GameplayEffectExtension.h"

UArcAT_WaitAttributeChangeComparison* UArcAT_WaitAttributeChangeComparison::AuWaitAttributeChangeComparison(
	UGameplayAbility* OwningAbility
	, FGameplayAttribute InAttribute
	, EArcWaitAttributeComprison InComparisonType
	, float InComparisonValue
	, FGameplayTag InWithTag
	, FGameplayTag InWithoutTag
	, bool TriggerOnce
	, AActor* OptionalExternalOwner)
{
	UArcAT_WaitAttributeChangeComparison* MyObj = NewAbilityTask<UArcAT_WaitAttributeChangeComparison>(OwningAbility);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->Attribute = InAttribute;
	MyObj->ComparisonType = InComparisonType;
	MyObj->ComparisonValue = InComparisonValue;
	MyObj->bTriggerOnce = TriggerOnce;
	MyObj->ExternalOwner = OptionalExternalOwner
						   ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OptionalExternalOwner)
						   : nullptr;

	return MyObj;
}

void UArcAT_WaitAttributeChangeComparison::Activate()
{
	if (UAbilitySystemComponent* ASC = GetFocusedASC())
	{
		OnAttributeChangeDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this
			, &UArcAT_WaitAttributeChangeComparison::OnAttributeChange);
	}
}

void UArcAT_WaitAttributeChangeComparison::OnAttributeChange(const FOnAttributeChangeData& CallbackData)
{
	float NewValue = CallbackData.NewValue;
	const FGameplayEffectModCallbackData* Data = CallbackData.GEModData;

	if (Data == nullptr)
	{
		// There may be no execution data associated with this change, for example a GE
		// being removed. In this case, we auto fail any WithTag requirement and auto pass
		// any WithoutTag requirement
		if (WithTag.IsValid())
		{
			return;
		}
	}
	else
	{
		if ((WithTag.IsValid() && !Data->EffectSpec.CapturedSourceTags.GetAggregatedTags()->HasTag(WithTag)) || (
				WithoutTag.IsValid() && Data->EffectSpec.CapturedSourceTags.GetAggregatedTags()->HasTag(WithoutTag)))
		{
			// Failed tag check
			return;
		}
	}
	bool PassedComparison = true;
	switch (ComparisonType)
	{
		case EArcWaitAttributeComprison::ExactlyEqualTo:
			PassedComparison = (NewValue == ComparisonValue);
			break;
		case EArcWaitAttributeComprison::GreaterThan:
			PassedComparison = (NewValue > ComparisonValue);
			break;
		case EArcWaitAttributeComprison::GreaterThanOrEqualTo:
			PassedComparison = (NewValue >= ComparisonValue);
			break;
		case EArcWaitAttributeComprison::LessThan:
			PassedComparison = (NewValue < ComparisonValue);
			break;
		case EArcWaitAttributeComprison::LessThanOrEqualTo:
			PassedComparison = (NewValue <= ComparisonValue);
			break;
		case EArcWaitAttributeComprison::NotEqualTo:
			PassedComparison = (NewValue != ComparisonValue);
			break;
		default:
			break;
	}
	if (PassedComparison)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnChange.Broadcast(CallbackData.NewValue);
		}
		if (bTriggerOnce)
		{
			EndTask();
		}
	}
}

UAbilitySystemComponent* UArcAT_WaitAttributeChangeComparison::GetFocusedASC()
{
	return ExternalOwner ? ExternalOwner.Get() : AbilitySystemComponent.Get();
}

void UArcAT_WaitAttributeChangeComparison::OnDestroy(bool AbilityEnded)
{
	if (UAbilitySystemComponent* ASC = GetFocusedASC())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(OnAttributeChangeDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}
