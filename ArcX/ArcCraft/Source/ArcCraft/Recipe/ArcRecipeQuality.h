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
#include "UObject/ObjectSaveContext.h"

#include "ArcRecipeQuality.generated.h"

class UAssetImportData;

/**
 * Maps a single gameplay tag to a numerical tier value and quality multiplier.
 * Used within UArcQualityTierTable to define the quality hierarchy.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcQualityTierMapping
{
	GENERATED_BODY()

public:
	/** The tag representing this tier (e.g., Material.Tier.Common). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag TierTag;

	/** Numerical value for this tier. Higher = better quality. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 TierValue = 0;

	/** Multiplier applied to output properties when ingredients of this tier are used.
	 *  1.0 = baseline, 1.5 = 50% bonus, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float QualityMultiplier = 1.0f;
};

/**
 * Data asset holding the global quality tier table.
 * Different recipe families can reference different tier tables
 * (e.g., weapons use material tiers, cooking uses freshness tiers).
 */
UCLASS(BlueprintType)
class ARCCRAFT_API UArcQualityTierTable : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	/** Import data storing the source file path for reimport. */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "ImportSettings")
	TObjectPtr<UAssetImportData> AssetImportData;
#endif

public:
	/** Ordered list of tiers, lowest to highest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quality")
	TArray<FArcQualityTierMapping> Tiers;

	/** Get the tier value for a given tag. Returns -1 if not found. */
	UFUNCTION(BlueprintCallable, Category = "Arc Craft|Quality")
	int32 GetTierValue(const FGameplayTag& InTag) const;

	/** Get the quality multiplier for a given tag. Returns 1.0 if not found. */
	UFUNCTION(BlueprintCallable, Category = "Arc Craft|Quality")
	float GetQualityMultiplier(const FGameplayTag& InTag) const;

	/**
	 * Given a tag container from an item, find the highest-value matching tier tag.
	 * Returns an invalid tag if none match.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Craft|Quality")
	FGameplayTag FindBestTierTag(const FGameplayTagContainer& InTags) const;

	/**
	 * Evaluate the quality multiplier for an item based on its tags.
	 * Finds the best tier tag and returns its multiplier. Returns 1.0 if no tier matches.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Craft|Quality")
	float EvaluateQuality(const FGameplayTagContainer& InItemTags) const;

	/** Export this table to JSON at the asset's file location. */
	UFUNCTION(CallInEditor, Category = "Quality")
	void ExportToJson();

	/** Get the asset import data for reimport support. */
	UAssetImportData* GetAssetImportData() const;

	virtual void PostInitProperties() override;
};
