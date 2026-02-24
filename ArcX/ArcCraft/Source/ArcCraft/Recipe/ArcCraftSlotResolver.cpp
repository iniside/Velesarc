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

#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"

#include "ArcCraft/Recipe/ArcCraftModifierSlot.h"

TArray<int32> FArcCraftSlotResolver::Resolve(
	const TArray<FArcCraftPendingModifier>& PendingModifiers,
	const TArray<FArcCraftModifierSlot>& Slots,
	int32 MaxUsableSlots,
	EArcCraftSlotSelection SelectionMode)
{
	TArray<int32> ResultIndices;

	if (PendingModifiers.Num() == 0)
	{
		return ResultIndices;
	}

	// ---- Step 1: Partition into unslotted and slotted ----

	TArray<int32> UnslottedIndices;
	TArray<int32> SlottedIndices;

	for (int32 Idx = 0; Idx < PendingModifiers.Num(); ++Idx)
	{
		if (!PendingModifiers[Idx].SlotTag.IsValid())
		{
			// Unslotted — always applies, bypasses slot system
			UnslottedIndices.Add(Idx);
		}
		else
		{
			SlottedIndices.Add(Idx);
		}
	}

	// ---- Step 2: Count available capacity per tag ----
	// Each entry in Slots is one "chair". Duplicate tags = multiple chairs for that category.

	TMap<FGameplayTag, int32> TagCapacity;
	for (const FArcCraftModifierSlot& Slot : Slots)
	{
		if (Slot.SlotTag.IsValid())
		{
			TagCapacity.FindOrAdd(Slot.SlotTag) += 1;
		}
	}

	// ---- Step 3: Group slotted modifiers by tag, discard those with no matching slot ----

	TMap<FGameplayTag, TArray<int32>> TagToModifiers;
	for (int32 Idx : SlottedIndices)
	{
		const FGameplayTag& Tag = PendingModifiers[Idx].SlotTag;
		if (TagCapacity.Contains(Tag))
		{
			TagToModifiers.FindOrAdd(Tag).Add(Idx);
		}
		// else: modifier targets a slot that doesn't exist on this recipe — discard
	}

	// ---- Step 4: Determine effective slot count ----
	// Effective slots = min(total defined slots, MaxUsableSlots if > 0)

	const int32 TotalDefinedSlots = Slots.Num();
	const int32 EffectiveSlots = (MaxUsableSlots > 0)
		? FMath::Min(TotalDefinedSlots, MaxUsableSlots)
		: TotalDefinedSlots;

	if (EffectiveSlots <= 0)
	{
		// No slots available — only unslotted modifiers apply
		return UnslottedIndices;
	}

	// ---- Step 5: Select modifiers to fill slots using SelectionMode ----
	// Build a flat list of all eligible slotted modifier indices, ordered by selection mode.
	// Then greedily assign to slots, respecting per-tag capacity.

	TArray<int32> AllEligible;
	for (auto& Pair : TagToModifiers)
	{
		AllEligible.Append(Pair.Value);
	}

	// Order candidates based on selection mode
	switch (SelectionMode)
	{
	case EArcCraftSlotSelection::HighestWeight:
		{
			// Sort by EffectiveWeight descending, tiebreak by index for determinism
			AllEligible.Sort([&PendingModifiers](int32 A, int32 B)
			{
				if (PendingModifiers[A].EffectiveWeight != PendingModifiers[B].EffectiveWeight)
				{
					return PendingModifiers[A].EffectiveWeight > PendingModifiers[B].EffectiveWeight;
				}
				return A < B;
			});
		}
		break;

	case EArcCraftSlotSelection::Random:
		{
			// Fisher-Yates shuffle for random selection
			for (int32 i = AllEligible.Num() - 1; i > 0; --i)
			{
				const int32 j = FMath::RandRange(0, i);
				AllEligible.Swap(i, j);
			}
		}
		break;

	default:
		checkNoEntry();
		break;
	}

	// Greedily assign top-priority modifiers to slots, respecting per-tag capacity
	TArray<int32> Assigned;
	TMap<FGameplayTag, int32> RemainingCapacity = TagCapacity;

	for (int32 Idx : AllEligible)
	{
		if (Assigned.Num() >= EffectiveSlots)
		{
			break;
		}

		const FGameplayTag& Tag = PendingModifiers[Idx].SlotTag;
		int32* Remaining = RemainingCapacity.Find(Tag);
		if (Remaining && *Remaining > 0)
		{
			Assigned.Add(Idx);
			(*Remaining) -= 1;
		}
	}

	// ---- Step 6: Combine unslotted + assigned slotted ----

	ResultIndices.Reserve(UnslottedIndices.Num() + Assigned.Num());
	ResultIndices.Append(UnslottedIndices);
	ResultIndices.Append(Assigned);

	return ResultIndices;
}
