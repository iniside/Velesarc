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
#include "Widgets/SCompoundWidget.h"
#include "Widgets/ArcModifierDebugContext.h"

class SVerticalBox;
class SExpandableArea;

/**
 * Shared results panel widget that renders simulated modifier output for all three
 * modifier-bearing asset editors (StaticModifierSet, MaterialPropertyTable, RandomPool).
 *
 * Displays an SExpandableArea titled "Simulated Output" whose body lists each eligible
 * modifier's ResolvedOutputs lines, grouped by their parent row (rule/entry) when present.
 *
 * Grouping is inferred from the ByRowKey encoding used by the three evaluators:
 *   - Keys < 10000 are parent rows (rule/entry level) or flat modifier rows (StaticModifierSet).
 *   - Keys >= 10000 are nested modifier rows: parent = key / 10000, child = key % 10000.
 *
 * Only rows where bModifierEligible == true and ResolvedOutputs is non-empty are shown.
 * The panel shows a placeholder message when there are no outputs to display.
 */
class SArcModifierResultsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcModifierResultsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Update the cached evaluation and rebuild the visible output list. */
	void SetEvaluation(const FArcModifierDebugEvaluation& InEvaluation);

private:
	void Rebuild();

	FArcModifierDebugEvaluation CachedEvaluation;
	TSharedPtr<SVerticalBox> Body;
};
