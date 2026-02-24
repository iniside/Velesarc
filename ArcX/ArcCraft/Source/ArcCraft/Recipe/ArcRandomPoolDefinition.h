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
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcRandomPoolDefinition.generated.h"

class UAssetImportData;

/**
 * Tag-conditional weight modifier for a random pool entry.
 * When all RequiredTags are present in the aggregated ingredient tags,
 * BonusWeight is added and WeightMultiplier is applied to the entry's effective weight.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcRandomPoolWeightModifier
{
	GENERATED_BODY()

	/** Tags that must ALL be present on ingredients for this bonus to apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weight")
	FGameplayTagContainer RequiredTags;

	/** Additive bonus to the entry's base weight when RequiredTags match. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weight")
	float BonusWeight = 0.0f;

	/** Multiplicative factor applied to the entry's weight when RequiredTags match.
	 *  Applied after BonusWeight is added. 1.0 = no change. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weight")
	float WeightMultiplier = 1.0f;
};

/**
 * Tag-conditional value scaling applied to a pool entry's sub-modifiers.
 * When RequiredTags match, the effective quality is scaled and offset.
 * Multiple rules stack sequentially.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcRandomPoolValueSkew
{
	GENERATED_BODY()

	/** Tags that must ALL be present for this skew to apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ValueSkew")
	FGameplayTagContainer RequiredTags;

	/** Multiplied into the effective quality passed to sub-modifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ValueSkew")
	float ValueScale = 1.0f;

	/** Added to the effective quality after ValueScale is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ValueSkew")
	float ValueOffset = 0.0f;
};

/**
 * A single entry in a random modifier pool.
 * Contains eligibility conditions, selection weight, budget cost,
 * value scaling rules, and the actual output modifiers to apply.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcRandomPoolEntry
{
	GENERATED_BODY()

	/** Display name for UI and debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry")
	FText DisplayName;

	// ---- Eligibility ----

	/** Tags that must ALL be present on at least one ingredient for this entry to be eligible.
	 *  Checked against the aggregated tag container from all consumed ingredients. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Eligibility")
	FGameplayTagContainer RequiredIngredientTags;

	/** Tags that must NOT be present on any ingredient for this entry to be eligible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Eligibility")
	FGameplayTagContainer DenyIngredientTags;

	/** Minimum average quality required for this entry to be eligible. 0 = always eligible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Eligibility",
		meta = (ClampMin = "0.0"))
	float MinQualityThreshold = 0.0f;

	// ---- Weight ----

	/** Base selection weight. Higher = more likely to be selected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Weight",
		meta = (ClampMin = "0.001"))
	float BaseWeight = 1.0f;

	/** How much the average quality scales the effective weight.
	 *  Formula: EffWeight *= (1 + (Quality - 1) * QualityWeightScaling)
	 *  0 = quality has no effect on weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Weight")
	float QualityWeightScaling = 0.0f;

	/** Tag-conditional weight modifiers. Multiple can fire simultaneously. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Weight")
	TArray<FArcRandomPoolWeightModifier> WeightModifiers;

	// ---- Budget ----

	/** Cost deducted from the budget when this entry is selected (budget mode only).
	 *  0 = free (always selected if eligible). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Budget",
		meta = (ClampMin = "0.0"))
	float Cost = 1.0f;

	// ---- Value Scaling ----

	/** Base multiplier applied to the effective quality for this entry's sub-modifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Value")
	float ValueScale = 1.0f;

	/** Base offset added to the effective quality after ValueScale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Value")
	float ValueSkew = 0.0f;

	/** If true, quality is factored in: EffQ = Quality * ValueScale + ValueSkew.
	 *  If false, uses flat value: EffQ = ValueScale + ValueSkew. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Value")
	bool bScaleByQuality = true;

	/** Tag-conditional value skew rules. Stacks sequentially with base ValueScale/ValueSkew. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Value")
	TArray<FArcRandomPoolValueSkew> ValueSkewRules;

	// ---- Modifiers ----

	/** Terminal modifiers to apply when this entry is selected.
	 *  Only Stats, Abilities, and Effects modifiers are allowed here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Modifiers",
		meta = (BaseStruct = "/Script/ArcCraft.ArcCraftModifier", ExcludeBaseStruct))
	TArray<FInstancedStruct> Modifiers;
};

/**
 * Self-contained data asset defining a pool of random modifier entries.
 * All entries are stored inline — no separate asset per entry.
 *
 * Referenced by FArcRecipeOutputModifier_RandomPool to provide the entry pool
 * for weighted random or budget-based selection during crafting.
 */
UCLASS(BlueprintType, meta = (LoadBehavior = "LazyOnDemand"))
class ARCCRAFT_API UArcRandomPoolDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Display name for UI and debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool")
	FText PoolName;

	/** All possible entries in this pool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool")
	TArray<FArcRandomPoolEntry> Entries;

#if WITH_EDITORONLY_DATA
	/** Import data storing the source file path for reimport. */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "ImportSettings")
	TObjectPtr<UAssetImportData> AssetImportData;
#endif

	/** Export this pool to JSON at the asset's file location. */
	UFUNCTION(CallInEditor, Category = "Pool")
	void ExportToJson();

	/** Get the asset import data for reimport support. */
	UAssetImportData* GetAssetImportData() const;

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
