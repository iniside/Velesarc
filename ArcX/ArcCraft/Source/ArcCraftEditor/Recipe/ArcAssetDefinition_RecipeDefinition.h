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

#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "AssetDefinitionDefault.h"

#include "ArcAssetDefinition_RecipeDefinition.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_RecipeDefinition"

UCLASS()
class UArcAssetDefinition_RecipeDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("RecipeDefinition", "Arc Recipe Definition");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(72, 191, 145)); // Teal-green, distinct from item orange
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcRecipeDefinition::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static FAssetCategoryPath AssetPaths[] = {
			FAssetCategoryPath(LOCTEXT("ArcCraft", "Arc Craft"))
		};
		return AssetPaths;
	}
};

#undef LOCTEXT_NAMESPACE
