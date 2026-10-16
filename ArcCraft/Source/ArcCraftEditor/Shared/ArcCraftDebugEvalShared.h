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
 * distributed under the LICENSE is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Widgets/ArcModifierDebugContext.h"

/**
 * Shared helpers used by all three debug-eval translation units
 * (MaterialPropertyTable, RandomPool, StaticModifierSet).
 *
 * Namespace is intentionally public/shared — this header is included by
 * multiple .cpp files, which is fine for header-declared namespaces.
 * Each consuming .cpp still has its own unique file-scoped impl namespace.
 *
 * ComputeAverageR and AggregateSlotTags are inline here because they
 * depend only on types available in this header and are trivially small.
 *
 * CollectApplyOutputs is declared separately in ArcCraftDebugEvalApply.inl
 * and must be included by each consuming .cpp AFTER including the item fragment
 * headers it needs. The .inl defines the function as static to avoid ODR issues.
 */
namespace ArcCraftDebugEval
{

/**
 * Compute the arithmetic mean R across all ingredient slots.
 * Returns 1.0f when Slots is empty (safe default).
 */
inline float ComputeAverageR(const TArray<FArcDebugIngredientSlot>& Slots)
{
	if (Slots.IsEmpty())
	{
		return 1.0f;
	}

	float StepSum = 0.0f;
	for (const FArcDebugIngredientSlot& Slot : Slots)
	{
		StepSum += Slot.RarityStep;
	}
	return StepSum / static_cast<float>(Slots.Num());
}

/**
 * Build the union of all tag containers across ingredient slots.
 * Returns an empty container when Slots is empty.
 */
inline FGameplayTagContainer AggregateSlotTags(const TArray<FArcDebugIngredientSlot>& Slots)
{
	FGameplayTagContainer Aggregated;
	for (const FArcDebugIngredientSlot& Slot : Slots)
	{
		Aggregated.AppendTags(Slot.Tags);
	}
	return Aggregated;
}

} // namespace ArcCraftDebugEval
