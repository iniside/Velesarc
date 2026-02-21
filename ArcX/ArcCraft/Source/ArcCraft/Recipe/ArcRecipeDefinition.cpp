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

#include "ArcCraft/Recipe/ArcRecipeDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UArcRecipeDefinition::GetPrimaryAssetId() const
{
	if (RecipeType.IsValid())
	{
		return FPrimaryAssetId(RecipeType, *RecipeId.ToString());
	}
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("ArcRecipe")), *RecipeId.ToString());
}

void UArcRecipeDefinition::PreSave(FObjectPreSaveContext SaveContext)
{
	if (SaveContext.IsCooking())
	{
		Super::PreSave(SaveContext);
		return;
	}

	if (!RecipeId.IsValid())
	{
		RecipeId = FGuid::NewGuid();
	}

	Super::PreSave(SaveContext);
}

void UArcRecipeDefinition::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		RecipeId = FGuid::NewGuid();
	}
}

void UArcRecipeDefinition::RegenerateRecipeId()
{
	RecipeId = FGuid::NewGuid();
}

#if WITH_EDITOR
EDataValidationResult UArcRecipeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!RecipeId.IsValid())
	{
		Context.AddWarning(FText::FromString(TEXT("Recipe has no valid RecipeId GUID. It will be generated on save.")));
	}

	if (!OutputItemDefinition.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("Recipe must have a valid output item definition.")));
		Result = EDataValidationResult::Invalid;
	}

	if (Ingredients.Num() == 0)
	{
		Context.AddWarning(FText::FromString(TEXT("Recipe has no ingredients.")));
	}

	if (CraftTime <= 0.0f)
	{
		Context.AddWarning(FText::FromString(TEXT("Recipe craft time is zero or negative.")));
	}

	for (int32 Idx = 0; Idx < Ingredients.Num(); ++Idx)
	{
		const FArcRecipeIngredient* Ingredient = Ingredients[Idx].GetPtr<FArcRecipeIngredient>();
		if (!Ingredient)
		{
			Context.AddError(FText::Format(
				FText::FromString(TEXT("Ingredient slot {0} has invalid type.")), FText::AsNumber(Idx)));
			Result = EDataValidationResult::Invalid;
		}
		else if (Ingredient->Amount <= 0)
		{
			Context.AddError(FText::Format(
				FText::FromString(TEXT("Ingredient slot {0} has zero or negative amount.")), FText::AsNumber(Idx)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
