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
#include "GameplayTagContainer.h"
#include "Math/RandomStream.h"

struct FArcCraftContext
{
	/** Rarity step per ingredient slot, parallel to the recipe ingredients. */
	TArray<float> SlotRaritySteps;

	float AverageR = 1.0f;

	/** Crafter skill above the recipe's RequiredSkillLevel; never negative. */
	float SkillMargin = 0.0f;

	/** Decided before the modifier sources run; empty when the output does not roll rarity. */
	FGameplayTag OutputRarity;

	/** Consumed amount per ingredient slot, parallel to the recipe ingredients; 0 for a slot that is not consumed. */
	TArray<int32> SlotAmounts;

	/** The craft's only random source; every roll draws from it in craft order, so the seed reproduces the craft. */
	FRandomStream Stream;
};
