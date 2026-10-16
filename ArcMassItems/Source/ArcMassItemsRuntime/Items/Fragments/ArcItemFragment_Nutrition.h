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

#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemFragment_Nutrition.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Nutrition", Category = "Consumable", ToolTip = "Hunger removed when one portion is eaten. Nutrition is subtracted from the eater's hunger need (0-100 scale)."))
struct ARCMASSITEMSRUNTIME_API FArcItemFragment_Nutrition : public FArcItemFragment
{
	GENERATED_BODY()

public:
	/** recipes/items.yaml cooked portions sit in the 25-50 range against the 0-100 hunger scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nutrition")
	float Nutrition = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nutrition")
	float Satiation = 0.0f;

	virtual ~FArcItemFragment_Nutrition() override = default;

#if WITH_EDITOR
	virtual FArcFragmentDescription GetDescription(const UScriptStruct* InStruct) const override;
#endif
};
