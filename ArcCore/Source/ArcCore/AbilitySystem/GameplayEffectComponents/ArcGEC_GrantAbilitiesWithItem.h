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

#include "GameplayEffectComponents/AbilitiesGameplayEffectComponent.h"
#include "ArcGEC_GrantAbilitiesWithItem.generated.h"

USTRUCT()
struct FArcGameplayAbilitySpecConfig
{
	GENERATED_BODY()

	/** What ability to grant */
	UPROPERTY(EditDefaultsOnly, Category = "Ability Definition")
	TSubclassOf<UGameplayAbility> Ability;

	/** What level to grant this ability at */
	UPROPERTY(EditDefaultsOnly, Category = "Ability Definition", DisplayName = "Level", meta=(UIMin=0.0))
	FScalableFloat LevelScalableFloat = FScalableFloat{ 1.0f };

	/** Input ID to bind this ability to */
	UPROPERTY(EditDefaultsOnly, Category = "Ability Definition")
	int32 InputID = INDEX_NONE;

	/** What will remove this ability later */
	UPROPERTY(EditDefaultsOnly, Category = "Ability Definition")
	EGameplayEffectGrantedAbilityRemovePolicy RemovalPolicy = EGameplayEffectGrantedAbilityRemovePolicy::CancelAbilityImmediately;

	FArcGameplayAbilitySpecConfig() {};
	
	FArcGameplayAbilitySpecConfig(const FGameplayAbilitySpecConfig& Other)
	{
		Ability = Other.Ability;
		LevelScalableFloat = Other.LevelScalableFloat;
		InputID = Other.InputID;
		RemovalPolicy = Other.RemovalPolicy;
	}

	FArcGameplayAbilitySpecConfig& operator=(const FGameplayAbilitySpecConfig& Other)
	{
		Ability = Other.Ability;
		LevelScalableFloat = Other.LevelScalableFloat;
		InputID = Other.InputID;
		RemovalPolicy = Other.RemovalPolicy;

		return *this;
	}
};

/**
 * Grants ability and assigns source item, from ability, which triggered this effect.
 */
UCLASS(DisplayName="Arc Grant Gameplay Abilities With Itmm")
class ARCCORE_API UArcGEC_GrantAbilitiesWithItem : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	/** Constructor */
	UArcGEC_GrantAbilitiesWithItem();

	/** Register for the appropriate events when we're applied */
	virtual bool OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const override;

	/** Adds an entry for granting Gameplay Abilities */
	void AddGrantedAbilityConfig(const FArcGameplayAbilitySpecConfig& Config);

#if WITH_EDITOR
	/** Warn on misconfigured Gameplay Effect */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/** This allows us to be notified when the owning GameplayEffect has had its inhibition changed (which can happen on the initial application). */
	virtual void OnInhibitionChanged(FActiveGameplayEffectHandle ActiveGEHandle, bool bIsInhibited) const;

	/** We should grant the configured Gameplay Abilities */
	virtual void GrantAbilities(FActiveGameplayEffectHandle ActiveGEHandle) const;

	/** We should remove the configured Gameplay Abilities */
	virtual void RemoveAbilities(FActiveGameplayEffectHandle ActiveGEHandle) const;

private:
	/** We must undo all effects when removed */
	void OnActiveGameplayEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo) const;

protected:
	/** Abilities to Grant to the Target while this Gameplay Effect is active */
	UPROPERTY(EditDefaultsOnly, Category = GrantAbilities)
	TArray<FArcGameplayAbilitySpecConfig>	GrantAbilityConfigs;
};
