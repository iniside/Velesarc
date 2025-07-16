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

#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayPrediction.h"
#include "Targeting/ArcTargetDataId.h"
#include "Templates/SubclassOf.h"

#include "GameplayEffectExecutionCalculation.h"

class UArcItemsStoreComponent;
struct FGameplayEffectContext;
struct FGameplayEffectContextHandle;
struct FGameplayEffectModCallbackData;
struct FGameplayEffectSpec;
struct FArcGameplayEffectContext;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilityTargetDataHandle;
struct FGameplayEffectCustomExecutionParameters;
struct FAggregatorEvaluateParameters;
struct FGameplayEffectAttributeCaptureSpec;
struct FArcItemId;

struct ARCCORE_API ArcAbility
{
	template <typename T>
	static const T* GetActorInfo(const FGameplayAbilityActorInfo* Info)
	{
		return static_cast<const T*>(Info);
	}

	template <typename T>
	static const T* GetActorInfo(UGameplayAbility* Ability)
	{
		return static_cast<const T*>(Ability->GetCurrentActorInfo());
	}

	template <typename T>
	static TSharedPtr<T> GetActorInfo(UAbilitySystemComponent* ASC)
	{
		return StaticCastSharedPtr<T>(ASC->AbilityActorInfo);
	}

	template <typename T>
	static TSharedPtr<T> GetActorInfo(TSharedPtr<FGameplayAbilityActorInfo> Info)
	{
		return StaticCastSharedPtr<T>(Info);
	}

	template <typename T>
	static const T* GetEffectContext(const FGameplayEffectContext* Info)
	{
		return static_cast<const T*>(Info);
	}

	template <typename T>
	static const T* GetEffectContext(const FGameplayEffectContextHandle& Info)
	{
		return static_cast<const T*>(Info.Get());
	}

	template <typename T>
	static const T* GetEffectContext(const FGameplayEffectSpec& Info)
	{
		return static_cast<const T*>(Info.GetContext().Get());
	}

	template <typename T>
	static const T* GetEffectContext(const FGameplayEffectCustomExecutionParameters& Info)
	{
		return static_cast<const T*>(Info.GetOwningSpec().GetContext().Get());
	}

	template <typename T>
	static const T* GetEffectContext(const FGameplayEffectModCallbackData& Info)
	{
		return static_cast<const T*>(Info.EffectSpec.GetEffectContext().Get());
	}

	template <typename T>
	static T* GetSourceObject(const FGameplayEffectSpec& Info)
	{
		return Cast<T>(Info.GetContext().GetSourceObject());
	}

	template <typename T>
	static T* GetSourceObject(const FGameplayEffectCustomExecutionParameters& Info)
	{
		return Cast<T>(Info.GetOwningSpec().GetContext().GetSourceObject());
	}

	static FGameplayAbilitySpecHandle GetAbilityHandle(const FGameplayEffectModCallbackData& Info);

	static void SetTargetDataHandle(FGameplayEffectContext* Context
									, const FArcTargetDataId& TargetDataHandle);

	static void SetTargetDataHandle(FGameplayEffectContextHandle Context
									, const FArcTargetDataId& TargetDataHandle);

	static void SetSourceItem(FGameplayEffectContext* Context
								, const FArcItemId& SourceItemHandle
								, UArcItemsStoreComponent* ItemsStore);
	
	static FArcTargetDataId GetTargetDataHandle(const FGameplayEffectContextHandle& Context);

	/*
	 * This functions works under assumption that in TargetDataHandle there is ONE
	 * and only ONE target data.
	 */
	template <typename T>
	static T* GetAbilityTargetData(const FGameplayAbilityTargetDataHandle& Handle)
	{
		return static_cast<T*>(Handle.Get(0));
	}

	static TArray<FActiveGameplayEffectHandle> ApplyEffectSpecTargetHandle(FGameplayAbilityTargetDataHandle& Handle
																		   , FGameplayEffectSpec& Spec
																		   , FPredictionKey PredictionKey =
																				   FPredictionKey());

	static float CalculateAttributeLog(const FAggregatorEvaluateParameters& Params
									   , float ItemStat
									   , const TArray<FGameplayEffectAttributeCaptureSpec>& BaseAttributes
									   , const TArray<FGameplayEffectAttributeCaptureSpec>& Multipliers);;
};