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

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Abilities/GameplayAbility.h"
#include "ArcCore/AbilitySystem/ArcAbilitiesTypes.h"


#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffectExtension.h"
#include "ArcCore/AbilitySystem/ArcAbilityTargetingComponent.h"

#include "GameplayEffectTypes.h"
#include "ArcGameplayEffectContext.h"
#include "Items/ArcItemsStoreComponent.h"

FGameplayAbilitySpecHandle ArcAbility::GetAbilityHandle(const FGameplayEffectModCallbackData& Info)
{
	if (Info.EffectSpec.GetEffectContext().GetAbilityInstance_NotReplicated())
	{
		return Info.EffectSpec.GetEffectContext().GetAbilityInstance_NotReplicated()->GetCurrentAbilitySpecHandle();
	}
	return FGameplayAbilitySpecHandle();
}

void ArcAbility::SetTargetDataHandle(FGameplayEffectContext* Context
	, const FArcTargetDataId& TargetDataHandle)
{
	static_cast<FArcGameplayEffectContext*>(Context)->SetTargetDataHandle(TargetDataHandle);
}

void ArcAbility::SetTargetDataHandle(FGameplayEffectContextHandle Context
	, const FArcTargetDataId& TargetDataHandle)
{
	static_cast<FArcGameplayEffectContext*>(Context.Get())->SetTargetDataHandle(TargetDataHandle);
}

void ArcAbility::SetSourceItem(FGameplayEffectContext* Context
	, const FArcItemId& SourceItemHandle
	, UArcItemsStoreComponent* ItemsStore)
{
	if (ItemsStore == nullptr)
	{
		return;
	}

	FArcGameplayEffectContext* Ctx = static_cast<FArcGameplayEffectContext*>(Context);
	if (Ctx == nullptr)
	{
		return;
	}

	Ctx->SetSourceItemHandle(SourceItemHandle);
	Ctx->ItemsStoreComponent = ItemsStore;
	Ctx->SourceItemPtr = ItemsStore->GetItemPtr(SourceItemHandle);
}

FArcTargetDataId ArcAbility::GetTargetDataHandle(const FGameplayEffectContextHandle& Context)
{
	return static_cast<const FArcGameplayEffectContext*>(Context.Get())->GetTargetDataHandle();
}

TArray<FActiveGameplayEffectHandle> ArcAbility::ApplyEffectSpecTargetHandle(FGameplayAbilityTargetDataHandle& Handle
																			, FGameplayEffectSpec& Spec
																			, FPredictionKey PredictionKey)
{
	TArray<FActiveGameplayEffectHandle> OutHandles;
	OutHandles.Reserve(Handle.Num());
	for (int32 Idx = 0; Idx < Handle.Num(); Idx++)
	{
		OutHandles.Append(Handle.Get(Idx)->ApplyGameplayEffectSpec(Spec
		, PredictionKey));
	}
	return OutHandles;
}

float ArcAbility::CalculateAttributeLog(const FAggregatorEvaluateParameters& Params
										, float ItemStat
										, const TArray<FGameplayEffectAttributeCaptureSpec>& BaseAttributes
										, const TArray<FGameplayEffectAttributeCaptureSpec>& Multipliers)
{
	float BaseAttributesSum = 1;
	float MultipliersSum = 1;
	for (const FGameplayEffectAttributeCaptureSpec& S : BaseAttributes)
	{
		float Val = 0;
		S.AttemptCalculateAttributeMagnitude(Params
			, Val);
		BaseAttributesSum += Val;
	}

	for (const FGameplayEffectAttributeCaptureSpec& S : Multipliers)
	{
		float Val = 0;
		S.AttemptCalculateAttributeMagnitude(Params
			, Val);
		MultipliersSum += Val;
	}
	BaseAttributesSum += ItemStat;

	const float RootValue = FMath::Sqrt(
		1 + (
					FMath::Fmod(
								BaseAttributesSum + MultipliersSum, BaseAttributesSum * MultipliersSum)
				  )
	);
	
	const float FinalScale = FMath::Pow(BaseAttributesSum*MultipliersSum, 1/RootValue);
	
	//const float BaseAttributeSumLog = 1 - BaseAttributesSum;
	//// this will apply diminishing returns for stacking multipliers, so we don't get
	//// into linear or exponential scaling. float FinalScale = 1 + (BaseAttributesSum *
	//// MultipliersSum) / (1 + (BaseAttributesSum * ReducingMult
	//// ));
	//float LogBase = 3.14 * (BaseAttributeSumLog * MultipliersSum);
	//float FinalScale = 1 + FMath::Abs<float>(FMath::LogX(LogBase
	//					   , (BaseAttributesSum * MultipliersSum)));

	return FinalScale;
}
