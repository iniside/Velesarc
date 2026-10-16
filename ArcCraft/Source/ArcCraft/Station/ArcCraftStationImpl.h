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
#include "GameplayTagContainer.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_InstanceTags.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"

namespace ArcCraftStationImpl
{

inline TArray<FPrimaryAssetId> ExtractDefIds(const TArray<FArcItemSpec>& Specs)
{
	TArray<FPrimaryAssetId> Out;
	Out.Reserve(Specs.Num());
	for (const FArcItemSpec& Spec : Specs)
	{
		Out.Add(Spec.GetItemDefinitionId());
	}
	return Out;
}

inline TArray<FGameplayTagContainer> ExtractInstanceTags(const TArray<FArcItemSpec>& Specs)
{
	TArray<FGameplayTagContainer> Out;
	Out.Reserve(Specs.Num());
	for (const FArcItemSpec& Spec : Specs)
	{
		Out.Add(ArcMassItems::GetSpecInstanceTags(Spec));
	}
	return Out;
}

inline TArray<FArcItemSpec> ReconstructMatchedSpecs(const TArray<FPrimaryAssetId>& DefIds, const TArray<FGameplayTagContainer>& SlotInstanceTags)
{
	TArray<FArcItemSpec> Out;
	Out.SetNum(DefIds.Num());
	for (int32 Idx = 0; Idx < DefIds.Num(); ++Idx)
	{
		Out[Idx].SetItemDefinition(DefIds[Idx]);
		if (SlotInstanceTags.IsValidIndex(Idx) && !SlotInstanceTags[Idx].IsEmpty())
		{
			FArcItemFragment_InstanceTags InstanceTags;
			InstanceTags.Tags = SlotInstanceTags[Idx];
			Out[Idx].AddInstanceData(&InstanceTags);
		}
	}
	return Out;
}

/**
 * Advance a completed queue entry: increment CompletedAmount, reset the craft timer, and remove the
 * entry when fully complete. Located by EntryId because RemoveAt shifts indices; no-op if the entry
 * is absent (e.g. cancelled meanwhile).
 */
inline void AdvanceCompletedEntry(FArcCraftQueueFragment& Queue, const FGuid& EntryId)
{
	const int32 FoundIndex = Queue.Entries.IndexOfByPredicate(
		[&EntryId](const FArcCraftQueueEntry& Entry)
		{
			return Entry.EntryId == EntryId;
		});

	if (FoundIndex == INDEX_NONE)
	{
		return;
	}

	FArcCraftQueueEntry& Entry = Queue.Entries[FoundIndex];
	Entry.CompletedAmount++;
	Entry.ElapsedTickTime = 0.0f;
	Entry.StartTimestamp = FDateTime::UtcNow().ToUnixTimestamp();

	if (Entry.CompletedAmount >= Entry.Amount)
	{
		Queue.Entries.RemoveAt(FoundIndex);
	}
}

} // namespace ArcCraftStationImpl
