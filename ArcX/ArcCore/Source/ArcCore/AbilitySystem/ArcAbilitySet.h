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

#include "Engine/DataAsset.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Targeting/ArcTargetingTypes.h"

#include "ArcAbilitySet.generated.h"

class UArcCoreAbilitySystemComponent;
class UArcCoreGameplayAbility;
class UGameplayAbility;
class UGameplayEffect;
DECLARE_LOG_CATEGORY_EXTERN(LogArcAbilitySet
	, Log
	, Log);

/**
 * FArcAbilitySet_GameplayAbility
 *
 *	Data used by the ability set to grant gameplay abilities.
 */
USTRUCT(BlueprintType)
struct FArcAbilitySet_GameplayAbility
{
	GENERATED_BODY()

public:
	// Gameplay ability to grant.
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayAbility> Ability = nullptr;

	// Level of ability to grant.
	UPROPERTY(EditDefaultsOnly)
	int32 AbilityLevel = 1;

	// Tag used to process input for the ability.
	UPROPERTY(EditDefaultsOnly
		, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * FArcAbilitySet_GameplayEffect
 *
 *	Data used by the ability set to grant gameplay effects.
 */
USTRUCT(BlueprintType)
struct FArcAbilitySet_GameplayEffect
{
	GENERATED_BODY()

public:
	// Gameplay effect to grant.
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;

	// Level of gameplay effect to grant.
	UPROPERTY(EditDefaultsOnly)
	float EffectLevel = 1.0f;
};

/**
 * FArcAbilitySet_AttributeSet
 *
 *	Data used by the ability set to grant attribute sets.
 */
USTRUCT(BlueprintType)
struct FArcAbilitySet_AttributeSet
{
	GENERATED_BODY()

public:
	// Gameplay effect to grant.
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly
		, Category = "Arc Core"
		, meta = (RequiredAssetDataTags = "RowStructure=/Script/GameplayAbilities.AttributeMetaData"))
	TObjectPtr<UDataTable> AttributeMetaData;

	FArcAbilitySet_AttributeSet()
		: AttributeSet(nullptr)
		, AttributeMetaData(nullptr)
	{
	};
};

/**
 * FArcAbilitySet_GrantedHandles
 *
 *	Data used to store handles to what has been granted by the ability set.
 */
USTRUCT(BlueprintType)
struct FArcAbilitySet_GrantedHandles
{
	GENERATED_BODY()

public:
	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);

	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);

	void AddAttributeSet(UAttributeSet* Set);

	void TakeFromAbilitySystem(UArcCoreAbilitySystemComponent* ArcASC);

protected:
	// Handles to the granted abilities.
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	// Handles to the granted gameplay effects.
	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	// Pointers to the granted attribute sets
	UPROPERTY()
	TArray<TObjectPtr<UAttributeSet>> GrantedAttributeSets;
};

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UArcAbilitySet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Grants the ability set to the specified ability system component.
	// The returned handles can be used later to take away anything that was granted.
	void GiveToAbilitySystem(UArcCoreAbilitySystemComponent* ArcASC
							 , FArcAbilitySet_GrantedHandles* OutGrantedHandles
							 , UObject* SourceObject = nullptr) const;

protected:
	// Gameplay abilities to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly
		, Category = "Gameplay Abilities"
		, meta = (TitleProperty = Ability))
	TArray<FArcAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	// Gameplay effects to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly
		, Category = "Gameplay Effects"
		, meta = (TitleProperty = GameplayEffect))
	TArray<FArcAbilitySet_GameplayEffect> GrantedGameplayEffects;

	// Attribute sets to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly
		, Category = "Attribute Sets"
		, meta = (TitleProperty = AttributeSet))
	TArray<FArcAbilitySet_AttributeSet> GrantedAttributes;

	UPROPERTY(EditDefaultsOnly
		, Category = "Targeting")
	TMap<FGameplayTag, FArcCoreGlobalTargetingEntry> GlobalTargetingPresets;
};
