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
#include "ArcNamedPrimaryAssetId.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/ObjectSaveContext.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"

#include "ArcRecipeDefinition.generated.h"

class UArcQualityTierTable;

/**
 * Data asset defining a crafting recipe.
 *
 * A recipe is a first-class object separate from item definitions.
 * It specifies what ingredients are needed (by item definition or by tags),
 * what output item is produced, and how ingredient quality affects the output.
 */
UCLASS(BlueprintType, meta = (LoadBehavior = "LazyOnDemand"))
class ARCCRAFT_API UArcRecipeDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Unique recipe identifier (GUID). Auto-generated on first save, regenerated on duplicate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recipe")
	FGuid RecipeId;

	/** Display name for UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FText RecipeName;

	/** Tags identifying this recipe (for filtering by crafting station type, category, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FGameplayTagContainer RecipeTags;

	/** Craft time in seconds for a single output. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	float CraftTime = 5.0f;

	// ---- Requirements ----

	/** Required tags on the crafting station (UArcCraftComponent::ItemTags must match). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Requirements")
	FGameplayTagContainer RequiredStationTags;

	/** Tag requirements on the instigator (player must have these). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Requirements")
	FGameplayTagContainer RequiredInstigatorTags;

	// ---- Ingredients ----

	/** Ingredient slots. Each is an instanced struct derived from FArcRecipeIngredient. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Ingredients",
		meta = (BaseStruct = "/Script/ArcCraft.ArcRecipeIngredient", ExcludeBaseStruct))
	TArray<FInstancedStruct> Ingredients;

	// ---- Output ----

	/** Item definition to produce. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Output",
		meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId OutputItemDefinition;

	/** Number of output items produced per craft. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Output")
	int32 OutputAmount = 1;

	/** Base level of the output item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Output")
	uint8 OutputLevel = 1;

	/** Output modifier rules — how ingredients affect the crafted item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Output",
		meta = (BaseStruct = "/Script/ArcCraft.ArcRecipeOutputModifier", ExcludeBaseStruct))
	TArray<FInstancedStruct> OutputModifiers;

	// ---- Quality ----

	/** Quality tier table for evaluating ingredient quality. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Quality")
	TSoftObjectPtr<UArcQualityTierTable> QualityTierTable;

	/** If true, output level is determined by average ingredient quality tier value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe|Quality")
	bool bQualityAffectsLevel = false;

	// ---- Asset type ----

	/** Type for primary asset ID registration. */
	UPROPERTY(EditAnywhere, Category = "Recipe")
	FPrimaryAssetType RecipeType;

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

	/** Regenerate the unique recipe identifier. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Recipe")
	void RegenerateRecipeId();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Get a typed ingredient by index. Returns nullptr if out of range or wrong type. */
	template<typename T>
	const T* GetIngredient(int32 Index) const
	{
		if (Ingredients.IsValidIndex(Index))
		{
			return Ingredients[Index].GetPtr<T>();
		}
		return nullptr;
	}

	/** Get the base ingredient at an index. */
	const FArcRecipeIngredient* GetIngredientBase(int32 Index) const
	{
		if (Ingredients.IsValidIndex(Index))
		{
			return Ingredients[Index].GetPtr<FArcRecipeIngredient>();
		}
		return nullptr;
	}

	int32 GetIngredientCount() const { return Ingredients.Num(); }
};
