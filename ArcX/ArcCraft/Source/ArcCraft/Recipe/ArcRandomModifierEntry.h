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
#include "StructUtils/InstancedStruct.h"

#include "ArcRandomModifierEntry.generated.h"

/**
 * Data asset defining a single random modifier entry.
 * Referenced by rows in a UChooserTable used by FArcRecipeOutputModifier_Random.
 *
 * Each entry contains a set of output modifiers (Stats, Abilities, Effects, etc.)
 * that will be applied to the crafted item when this entry is selected.
 */
UCLASS(BlueprintType)
class ARCCRAFT_API UArcRandomModifierEntry : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Display name for UI and debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Entry")
	FText DisplayName;

	/** Modifiers to apply when this entry is selected.
	 *  Uses the same instanced struct pattern as UArcRecipeDefinition::OutputModifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Entry",
		meta = (BaseStruct = "/Script/ArcCraft.ArcRecipeOutputModifier", ExcludeBaseStruct))
	TArray<FInstancedStruct> Modifiers;

	/** Multiplied into stat values from the granted modifiers.
	 *  Passed as a scale factor to the effective quality used by sub-modifiers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Entry|Scaling")
	float ValueScale = 1.0f;

	/** Added to stat values after ValueScale is applied.
	 *  Provides a fixed offset to the effective quality. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Entry|Scaling")
	float ValueSkew = 0.0f;

	/** If true, stat values are additionally scaled by the average quality multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Entry|Scaling")
	bool bScaleByQuality = true;
};
