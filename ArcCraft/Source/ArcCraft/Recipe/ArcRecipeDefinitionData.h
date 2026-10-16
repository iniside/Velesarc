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
#include "UObject/PrimaryAssetId.h"
#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcCraft/Recipe/ArcCraftModifierSlot.h"
#include "ArcCraft/Recipe/ArcCraftSlotSelector.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Recipe/ArcRecipeModifierSource.h"
#include "Attributes/ArcAttribute.h"

#include "ArcRecipeDefinitionData.generated.h"

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcRecipeOutput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe",
		meta = (AllowedTypes = "ArcItem"))
	FArcNamedPrimaryAssetId ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	int32 Amount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	uint8 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FGameplayTagContainer InstanceTags;

	/** Read on the main output only: it takes the rarity of the first required ingredient instead of rolling one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	bool bKeepIngredientRarity = false;
};

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcRecipeDefinitionData
{
	GENERATED_BODY()

public:
	static FPrimaryAssetType JsonPrimaryAssetType()
	{
		return FPrimaryAssetType(TEXT("ArcRecipe"));
	}

	/** Unique recipe identifier (GUID). Auto-generated on first save, regenerated on duplicate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recipe")
	FGuid RecipeId;

	/** Display name for UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FText RecipeName;

	/** Tags identifying this recipe (for filtering by crafting station type, category, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable, Category = "Recipe")
	FGameplayTagContainer RecipeTags;

	/** Craft time in seconds for a single output. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	float CraftTime = 5.0f;

	// ---- Requirements ----

	/** Required tags on the crafting station entity (must be a subset of the station's StationTags). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable, Category = "Recipe")
	FGameplayTagContainer RequiredStationTags;

	/** Tag requirements on the instigator (player must have these). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FGameplayTagContainer RequiredInstigatorTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FArcAttributeRef RequiredSkill;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	int32 RequiredSkillLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	bool bDefaultUnlock = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	int32 Difficulty = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	float XpMultiplier = 1.0f;

	// ---- Ingredients ----

	/** Ingredient slots. Each is an instanced struct derived from FArcRecipeIngredient. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe",
		meta = (BaseStruct = "/Script/ArcCraft.ArcRecipeIngredient", ExcludeBaseStruct))
	TArray<FInstancedStruct> Ingredients;

	// ---- Output ----

	/** Items produced per craft. The first entry is the main output; modifiers apply to it only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	TArray<FArcRecipeOutput> Outputs;

	/** Start rarity of the main output; empty = base recipe (DefaultRarity, or Poor below PoorBelowR). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (Categories = "Item.Rarity"))
	FGameplayTag GuaranteedRarity;

	/** Base recipe of this rarity variant; empty on a base. An upgraded output takes the chairs and selector of the
	 *  family member (the base or a variant pointing at it) whose start rarity is the output rarity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (AllowedTypes = "ArcRecipe"))
	FArcNamedPrimaryAssetId BaseRecipe;

	/** Modifier sources — where modifiers come from for this recipe. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe",
		meta = (BaseStruct = "/Script/ArcCraft.ArcRecipeModifierSource", ExcludeBaseStruct))
	TArray<FInstancedStruct> ModifierSources;

	// ---- Modifier Slots ----

	/** Modifier slot definitions for this recipe; each entry is one chair a single group or penalty can fill.
	 *  Duplicate tags are allowed for several chairs of the same kind. Without chairs no slotted result applies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	TArray<FArcCraftModifierSlot> ModifierSlots;

	/** Orders the candidates for the bonus and penalty chairs. Empty = HighestWeight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe",
		meta = (BaseStruct = "/Script/ArcCraft.ArcCraftSlotSelector", ExcludeBaseStruct))
	FInstancedStruct SlotSelector;

	/** Type for primary asset ID registration. */
	UPROPERTY(EditAnywhere, Category = "Recipe")
	FPrimaryAssetType RecipeType = JsonPrimaryAssetType();

	template<typename T>
	const T* GetIngredient(int32 Index) const
	{
		if (Ingredients.IsValidIndex(Index))
		{
			return Ingredients[Index].GetPtr<T>();
		}
		return nullptr;
	}

	const FArcRecipeIngredient* GetIngredientBase(int32 Index) const
	{
		if (Ingredients.IsValidIndex(Index))
		{
			return Ingredients[Index].GetPtr<FArcRecipeIngredient>();
		}
		return nullptr;
	}

	int32 GetIngredientCount() const { return Ingredients.Num(); }

	bool IsIngredientOptional(int32 Index) const
	{
		const FArcRecipeIngredient* Ingredient = GetIngredientBase(Index);
		return Ingredient != nullptr && Ingredient->bOptional;
	}

	int32 GetRequiredIngredientCount() const
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < Ingredients.Num(); ++Index)
		{
			if (!IsIngredientOptional(Index))
			{
				++Count;
			}
		}
		return Count;
	}

	const FArcRecipeOutput* GetMainOutput() const
	{
		return Outputs.IsEmpty() ? nullptr : &Outputs[0];
	}

	/** RecipeName when set, else the main output item's UIDescription ItemName, else empty. */
	FText GetDisplayName() const;
};
