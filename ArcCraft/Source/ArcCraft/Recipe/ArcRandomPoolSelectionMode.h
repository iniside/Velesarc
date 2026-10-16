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

#include "ArcRandomPoolSelectionMode.generated.h"

struct FArcRandomPoolEntry;
struct FRandomStream;

/**
 * Context data passed to selection mode implementations.
 * Contains pre-computed eligibility and weight information.
 * Not a USTRUCT — only used as a parameter to the Select() method.
 */
struct FArcRandomPoolSelectionContext
{
	/** All entries in the pool (full array, use EligibleIndices to access valid ones). */
	const TArray<FArcRandomPoolEntry>& Entries;

	/** Indices into Entries that passed eligibility checks. */
	const TArray<int32>& EligibleIndices;

	/** Effective weight for each eligible entry (parallel to EligibleIndices). */
	const TArray<float>& EffectiveWeights;

	/** Average R across all ingredient slots. */
	float AverageR = 1.0f;

	/** The craft's stream; every pick draws from it. */
	const FRandomStream& Stream;
};

/**
 * Base struct for random pool selection strategies.
 * Subclasses implement Select() to determine which entries are chosen.
 * Uses instanced struct polymorphism for extensibility.
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Base struct for random pool selection strategies; subclasses implement Select() to determine which entries are chosen."))
struct ARCCRAFT_API FArcRandomPoolSelectionMode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection",
		meta = (Tooltip = "If false, the same entry cannot be selected more than once per evaluation."))
	bool bAllowDuplicates = false;

	/**
	 * Select entries from the pool based on the given context.
	 *
	 * @param Context        Pre-computed eligibility, weights, and R data.
	 * @param OutSelectedIndices   Output: indices into Context.Entries of selected entries.
	 */
	virtual void Select(
		const FArcRandomPoolSelectionContext& Context,
		TArray<int32>& OutSelectedIndices) const;

	virtual ~FArcRandomPoolSelectionMode() = default;

protected:
	/**
	 * Shared utility: perform a single weighted random pick from candidates.
	 * Returns the index into Context.Entries, or INDEX_NONE if nothing is available.
	 *
	 * @param Context          Selection context with weights.
	 * @param AlreadySelected  Set of entry indices to exclude (when !bAllowDuplicates).
	 */
	static int32 WeightedRandomPick(
		const FArcRandomPoolSelectionContext& Context,
		const TSet<int32>& AlreadySelected);
};

/**
 * Simple weighted random selection.
 * Rolls a configurable number of times, each time picking one entry by weight.
 * R can grant bonus selections.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Simple Random", Tooltip = "Simple weighted random selection; rolls a configurable number of times, each time picking one entry by weight."))
struct ARCCRAFT_API FArcRandomPoolSelection_SimpleRandom : public FArcRandomPoolSelectionMode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection",
		meta = (ClampMin = 1, Tooltip = "Number of entries to select."))
	int32 MaxSelections = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection|R",
		meta = (Tooltip = "If true, R above RBonusThreshold grants additional selections."))
	bool bRAffectsSelections = false;

	/** Each full multiple of this threshold adds one bonus selection.
	 *  BonusSelections = floor(AverageR / RBonusThreshold). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection|R",
		meta = (EditCondition = "bRAffectsSelections", ClampMin = "0.01"))
	float RBonusThreshold = 2.0f;

	virtual void Select(
		const FArcRandomPoolSelectionContext& Context,
		TArray<int32>& OutSelectedIndices) const override;
};

/**
 * Budget-based selection.
 * A point budget is derived from R. Entries are selected by weighted random
 * as long as their cost fits within the remaining budget.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Budget", Tooltip = "Budget-based selection; a point budget is derived from R and entries are selected by weighted random until budget is exhausted."))
struct ARCCRAFT_API FArcRandomPoolSelection_Budget : public FArcRandomPoolSelectionMode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection|Budget",
		meta = (ClampMin = "0.0", Tooltip = "Base budget points available for entry selection."))
	float BaseBudget = 3.0f;

	/** Additional budget per unit of average R above 1.0.
	 *  TotalBudget = BaseBudget + max(0, AverageR - 1.0) * BudgetPerR. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection|Budget",
		meta = (ClampMin = "0.0"))
	float BudgetPerR = 1.0f;

	/** Maximum number of entries that can be selected regardless of remaining budget.
	 *  0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection|Budget",
		meta = (ClampMin = "0"))
	int32 MaxBudgetSelections = 0;

	virtual void Select(
		const FArcRandomPoolSelectionContext& Context,
		TArray<int32>& OutSelectedIndices) const override;
};
