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

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "ArcCore/AbilitySystem/ArcAttributesTypes.h"
#include "ArcCore/Items/ArcItemTypes.h"
#include "ArcCore/Items/ArcItemData.h"

#include "GameplayEffect.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ArcAbilitiesBPF.generated.h"

class UGameplayEffect;
struct FActiveGameplayEffect;
struct FArcActiveGameplayEffectForRPC;

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAbilitiesBPF : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure
		, Category = "Arc Core|Ability|Helpers")
	static FString GetAttributeName(const FGameplayAttribute& Attribute);

	static FArcItemId GetSourceItem(const FGameplayEffectSpec& Spec);

	static class UArcCoreAbilitySystemComponent* GetInstigatorNxASC(const FGameplayEffectSpec& Spec);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	static FString GetSpecName(const FGameplayEffectSpecHandle& Spec);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	static const UArcItemDefinition* GetOwnerItemDef(const FGameplayEffectContextHandle& Context, bool& bSuccess);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	static FArcItemDataHandle GetOwnerItemData(const FGameplayEffectContextHandle& Context, bool& bSuccess);
	
	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability")
	static void AddSpecTags(const FGameplayEffectSpecHandle& Spec
							, const FGameplayTagContainer& Tags);

	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability")
	static void AddSpecTag(const FGameplayEffectSpecHandle& Spec
						   , const FGameplayTag& Tag);

	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability")
	static FGameplayEffectSpecHandle MakeSpecHandle(TSubclassOf<UGameplayEffect> InGameplayEffect
													, AActor* InInstigator
													, AActor* InEffectCauser
													, float InLevel);

	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability")
	static void SetTargetHandleInContext(FGameplayEffectContextHandle Context
										 , FArcTargetDataId TargetDataHandle);

	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability")
	static void SetContextInSpec(FGameplayEffectSpecHandle SpecHandle
								 , FGameplayEffectContextHandle Context);

	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability")
	static void AddInstigator(FGameplayEffectContextHandle Context
							  , AActor* InInstigator
							  , AActor* InEffectCauser);

	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability")
	static FGameplayEffectContextHandle DuplicateEffectContext(FGameplayEffectContextHandle Context);

	UFUNCTION(BlueprintCallable
		, Category = "Arc Core|Ability")
	static void RemoveEffectsFromTarget(const TArray<FActiveGameplayEffectHandle>& Handles
										, FGameplayAbilityTargetDataHandle Target);

	UFUNCTION(BlueprintPure
		, Category = "Arc Core|Ability|Helpers")
	static void GetCooldownTimeRemainingAndDuration(class UGameplayAbility* Ability
													, float& TimeRemaining
													, float& CooldownDuration);

	UFUNCTION(BlueprintPure
		, meta = (DisplayName = "Equal (GameplayAttribute)", CompactNodeTitle = "==", Keywords = "== equal",
			ScriptOperator = "==")
		, Category = "Arc Core|Ability")
	static bool EqualEqual_GameplayAttribute(const FGameplayAttribute& A
											 , const FGameplayAttribute& B)
	{
		return A == B;
	}


	UFUNCTION(BlueprintPure
		, Category = "Arc Core")
	static FGameplayCueParameters ArcMakeGameplayCueParametersFromHitResult(const FHitResult& HitResult);

	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static FGameplayAbilityTargetDataHandle AbilityTargetDataFromHitResultArray(const TArray<FHitResult>& HitResults);

	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static FActiveGameplayEffectHandle GetActiveEffectHandle(const FActiveGameplayEffect& ActiveGE);
	
	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static const UGameplayEffect* GetEffectDef(const FActiveGameplayEffect& ActiveGE);
	
	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static float GetEffectDuration(const FActiveGameplayEffect& ActiveGE);

	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static float GetEffectPeriod(const FActiveGameplayEffect& ActiveGE);
	
	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static float GetEffectEndTime(const FActiveGameplayEffect& ActiveGE);
	
	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static float GetEffectTimeRemaining(const FActiveGameplayEffect& ActiveGE, float WorldTime);

	UFUNCTION(BlueprintPure, Category = "Arc Core", meta = (ItemClass = "/Script/GameplayAbilities.GameplayEffectComponent", DeterminesOutputType= "ItemClass"))
	static const UGameplayEffectComponent* GetEffectComponent(const FActiveGameplayEffect& ActiveGE, TSubclassOf<UGameplayEffectComponent> ItemClass);
	
	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static FActiveGameplayEffectHandle GetActiveEffectRPCHandle(const FArcActiveGameplayEffectForRPC& ActiveGE);
	
	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static const UGameplayEffect* GetEffectRPCDef(const FArcActiveGameplayEffectForRPC& ActiveGE);
	
	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static float GetEffectRPCDuration(const FArcActiveGameplayEffectForRPC& ActiveGE);

	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static float GetEffectRPCPeriod(const FArcActiveGameplayEffectForRPC& ActiveGE);
	
	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static float GetEffectRPCEndTime(const FArcActiveGameplayEffectForRPC& ActiveGE);
	
	UFUNCTION(BlueprintPure, Category = "Arc Core")
	static float GetEffectRPCTimeRemaining(const FArcActiveGameplayEffectForRPC& ActiveGE, float WorldTime);
};
