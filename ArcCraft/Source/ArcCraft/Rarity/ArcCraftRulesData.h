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

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Attributes/ArcAttribute.h"
#include "ArcNamedPrimaryAssetId.h"

#include "ArcCraftRulesData.generated.h"

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftRarityStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rarity", meta = (Categories = "Item.Rarity"))
	FGameplayTag Rarity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rarity")
	float Step = 1.0f;

	/** Band cut from a stat's catalog range for outputs of this rarity, as fractions of the range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rarity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BandMinFraction = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rarity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BandMaxFraction = 1.0f;
};

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftUpgradeRoll
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	float Base = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	float PerSkillMargin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	float PerR = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Cap = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NextStepChance = 0.0f;
};

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftRulesData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rules")
	FGuid RulesId;

	/** Ordered from the lowest rarity to the highest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (TitleProperty = "Rarity"))
	TArray<FArcCraftRarityStep> Ladder;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (Categories = "Item.Rarity"))
	FGameplayTag DefaultRarity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	float PoorBelowR = 0.0f;

	/** Transfer stats rolled on the full ingredient range, with no rarity band and no skill margin bias. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	TArray<FArcAttributeRef> RarityBandExempt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	FArcCraftUpgradeRoll UpgradeRoll;

	/** Stat catalog that rule stat modifiers resolve their StatId in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (AllowedTypes = "ArcStatCatalog"))
	FArcNamedPrimaryAssetId StatCatalog;

	/** Band position bias per ingredient R above 1 (a in the band exponent). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	float BandBiasPerR = 0.0f;

	/** Band position bias per point of skill margin (b in the band exponent). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	float BandBiasPerSkillMargin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	FGameplayTagContainer NoRarityOutputTags;
};
