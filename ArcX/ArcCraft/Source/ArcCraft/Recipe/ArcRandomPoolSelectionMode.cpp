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

#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"

#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"

// -------------------------------------------------------------------
// FArcRandomPoolSelectionMode (base)
// -------------------------------------------------------------------

void FArcRandomPoolSelectionMode::Select(
	const FArcRandomPoolSelectionContext& Context,
	TArray<int32>& OutSelectedIndices) const
{
	// Base does nothing — subclasses implement selection strategies.
}

int32 FArcRandomPoolSelectionMode::WeightedRandomPick(
	const FArcRandomPoolSelectionContext& Context,
	const TSet<int32>& AlreadySelected)
{
	// Build filtered weight total from eligible entries not already selected
	float TotalWeight = 0.0f;
	for (int32 i = 0; i < Context.EligibleIndices.Num(); ++i)
	{
		const int32 EntryIdx = Context.EligibleIndices[i];
		if (AlreadySelected.Contains(EntryIdx))
		{
			continue;
		}
		TotalWeight += Context.EffectiveWeights[i];
	}

	if (TotalWeight <= 0.0f)
	{
		return INDEX_NONE;
	}

	const float Roll = FMath::FRandRange(0.0f, TotalWeight);
	float Accumulator = 0.0f;

	for (int32 i = 0; i < Context.EligibleIndices.Num(); ++i)
	{
		const int32 EntryIdx = Context.EligibleIndices[i];
		if (AlreadySelected.Contains(EntryIdx))
		{
			continue;
		}

		Accumulator += Context.EffectiveWeights[i];
		if (Roll <= Accumulator)
		{
			return EntryIdx;
		}
	}

	// Floating point edge case — return last eligible
	for (int32 i = Context.EligibleIndices.Num() - 1; i >= 0; --i)
	{
		const int32 EntryIdx = Context.EligibleIndices[i];
		if (!AlreadySelected.Contains(EntryIdx))
		{
			return EntryIdx;
		}
	}

	return INDEX_NONE;
}

// -------------------------------------------------------------------
// FArcRandomPoolSelection_SimpleRandom
// -------------------------------------------------------------------

void FArcRandomPoolSelection_SimpleRandom::Select(
	const FArcRandomPoolSelectionContext& Context,
	TArray<int32>& OutSelectedIndices) const
{
	if (Context.EligibleIndices.Num() == 0)
	{
		return;
	}

	int32 TotalSelections = MaxSelections;
	if (bQualityAffectsSelections && QualityBonusThreshold > 0.0f)
	{
		const int32 BonusSelections = FMath::FloorToInt32(Context.AverageQuality / QualityBonusThreshold);
		TotalSelections += BonusSelections;
	}

	TSet<int32> AlreadySelected;

	for (int32 Roll = 0; Roll < TotalSelections; ++Roll)
	{
		const int32 SelectedIdx = WeightedRandomPick(Context, bAllowDuplicates ? TSet<int32>() : AlreadySelected);
		if (SelectedIdx == INDEX_NONE)
		{
			break;
		}

		OutSelectedIndices.Add(SelectedIdx);

		if (!bAllowDuplicates)
		{
			AlreadySelected.Add(SelectedIdx);
		}
	}
}

// -------------------------------------------------------------------
// FArcRandomPoolSelection_Budget
// -------------------------------------------------------------------

void FArcRandomPoolSelection_Budget::Select(
	const FArcRandomPoolSelectionContext& Context,
	TArray<int32>& OutSelectedIndices) const
{
	if (Context.EligibleIndices.Num() == 0)
	{
		return;
	}

	float RemainingBudget = BaseBudget + FMath::Max(0.0f, Context.AverageQuality - 1.0f) * BudgetPerQuality;
	int32 SelectionCount = 0;

	TSet<int32> AlreadySelected;

	while (RemainingBudget > 0.0f)
	{
		if (MaxBudgetSelections > 0 && SelectionCount >= MaxBudgetSelections)
		{
			break;
		}

		// Build a filtered context with only affordable entries
		TArray<int32> AffordableIndices;
		TArray<float> AffordableWeights;
		AffordableIndices.Reserve(Context.EligibleIndices.Num());
		AffordableWeights.Reserve(Context.EligibleIndices.Num());

		for (int32 i = 0; i < Context.EligibleIndices.Num(); ++i)
		{
			const int32 EntryIdx = Context.EligibleIndices[i];

			if (!bAllowDuplicates && AlreadySelected.Contains(EntryIdx))
			{
				continue;
			}

			if (Context.Entries[EntryIdx].Cost <= RemainingBudget)
			{
				AffordableIndices.Add(EntryIdx);
				AffordableWeights.Add(Context.EffectiveWeights[i]);
			}
		}

		if (AffordableIndices.Num() == 0)
		{
			break;
		}

		// Create a temporary context for the affordable subset
		FArcRandomPoolSelectionContext AffordableContext
		{
			Context.Entries,
			AffordableIndices,
			AffordableWeights,
			Context.AverageQuality
		};

		const TSet<int32> EmptySet;
		const int32 SelectedIdx = WeightedRandomPick(AffordableContext, EmptySet);
		if (SelectedIdx == INDEX_NONE)
		{
			break;
		}

		OutSelectedIndices.Add(SelectedIdx);
		RemainingBudget -= Context.Entries[SelectedIdx].Cost;
		SelectionCount++;

		if (!bAllowDuplicates)
		{
			AlreadySelected.Add(SelectedIdx);
		}
	}
}
