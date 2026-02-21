/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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
#include "Engine/DataAsset.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"

#include "ArcMaterialPropertyTable.generated.h"

class UArcQualityTierTable;

/**
 * Data asset that maps material tag patterns to quality-banded modifier contributions.
 * Referenced by FArcRecipeOutputModifier_MaterialProperties in a recipe's OutputModifiers.
 *
 * When a craft completes, ingredient tags are evaluated per-slot and matched against the rules.
 * Each matching rule selects a quality band via weighted random (biased by quality and weight bonus),
 * and the band's modifiers are applied to the output item.
 *
 * Supports:
 *  - Tag combos via FGameplayTagQuery (AND/OR/NOT)
 *  - Quality bands with weighted random selection
 *  - Extra-ingredient and extra-time weight remedies
 *  - Optional band budget to limit total modifier power
 */
UCLASS(BlueprintType, meta = (LoadBehavior = "LazyOnDemand"))
class ARCCRAFT_API UArcMaterialPropertyTable : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Display name for editor and debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Table")
	FText TableName;

	/** All property rules in this table. Evaluated against per-slot ingredient tags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Table")
	TArray<FArcMaterialPropertyRule> Rules;

	/** Maximum number of rules that can fire per craft. 0 = unlimited (all matching rules fire). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Table|Limits", meta = (ClampMin = "0"))
	int32 MaxActiveRules = 0;

	/** Default quality tier table for evaluating ingredient quality.
	 *  Can be overridden by the recipe's own tier table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Table|Quality")
	TSoftObjectPtr<UArcQualityTierTable> DefaultTierTable;

	// ---- Remedies ----

	/** Weight bonus per extra ingredient beyond BaseIngredientCount.
	 *  Formula: BandWeightBonus += max(0, IngredientCount - BaseIngredientCount) * ExtraIngredientWeightBonus
	 *  Affects band WEIGHT only — does NOT unlock new bands (MinQuality threshold unchanged).
	 *  Addresses the "use more materials" quality remedy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Table|Remedies")
	float ExtraIngredientWeightBonus = 0.1f;

	/** Maximum weight bonus from extra craft time.
	 *  The actual bonus is min(ExtraCraftTimeBonus, ExtraTimeWeightBonusCap).
	 *  Affects band WEIGHT only — does NOT unlock new bands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Table|Remedies", meta = (ClampMin = "0.0"))
	float ExtraTimeWeightBonusCap = 1.0f;

	// ---- Band Budget ----

	/** If > 0, enables budget mode for band selection.
	 *  Each selected band costs (bandIndex + 1) budget points.
	 *  Higher quality increases available budget. 0 = no budget, all matching rules fire. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Table|Limits")
	float BaseBandBudget = 0.0f;

	/** Additional budget per unit of BandEligibilityQuality above 1.0.
	 *  TotalBudget = BaseBandBudget + max(0, BandEligibilityQuality - 1.0) * BudgetPerQuality. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Table|Limits")
	float BudgetPerQuality = 0.0f;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
