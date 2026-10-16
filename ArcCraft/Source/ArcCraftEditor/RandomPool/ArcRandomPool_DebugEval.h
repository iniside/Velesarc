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

#include "CoreMinimal.h"
#include "Widgets/ArcModifierDebugContext.h"

struct FArcRandomPoolDefinitionData;
struct FInstancedStruct;

namespace ArcRandomPool_DebugEval
{

/**
 * Evaluates all entries (and their child modifiers) in a random pool record
 * against the given debug context.
 *
 * Row key encoding:
 *   - Entry rows:           EntryIndex
 *   - Modifier child rows:  EntryIndex * 10000 + ModifierIndex
 *
 * Entry rows carry bRulePassed = bEligible, EffectiveWeight, MatchedTags and IngredientShare.
 * Modifier child rows carry bModifierEligible based on MinR checked
 * against the debug average R.
 */
FArcModifierDebugEvaluation EvaluateRandomPoolForDebug(
	const FArcRandomPoolDefinitionData& PoolDef,
	const FArcModifierDebugContext& Ctx);

/**
 * Perform one weighted random roll using the runtime selection logic.
 * Returns the selected entry indices.
 * DebugSelectionMode must contain a FArcRandomPoolSelectionMode subtype;
 * if it is empty, falls back to FArcRandomPoolSelection_SimpleRandom{MaxSelections=1}.
 */
TArray<int32> RollOnce(
	const FArcRandomPoolDefinitionData& PoolDef,
	const FArcModifierDebugContext& Ctx,
	const FInstancedStruct& DebugSelectionMode);

} // namespace ArcRandomPool_DebugEval
