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
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemFragment_RecipeUnlock.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Recipe Unlock", Tooltip = "Definition-side fragment listing the recipes this item teaches."))
struct ARCCRAFT_API FArcItemFragment_RecipeUnlock : public FArcItemFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipes", meta = (AllowedTypes = "ArcRecipe", Tooltip = "Recipes this item teaches. Data only; behavior TBD."))
	TArray<FArcNamedPrimaryAssetId> Recipes;
};
