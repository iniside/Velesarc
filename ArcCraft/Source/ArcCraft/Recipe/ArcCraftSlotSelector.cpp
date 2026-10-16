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

#include "ArcCraft/Recipe/ArcCraftSlotSelector.h"

#include "Math/RandomStream.h"

void FArcCraftSlotSelector::Order(TArray<FArcCraftChairCandidate>& Candidates, const FRandomStream& Stream) const
{
}

void FArcCraftSlotSelector_HighestWeight::Order(TArray<FArcCraftChairCandidate>& Candidates, const FRandomStream& Stream) const
{
	Candidates.StableSort([](const FArcCraftChairCandidate& A, const FArcCraftChairCandidate& B)
	{
		return A.Weight > B.Weight;
	});
}

void FArcCraftSlotSelector_WeightedRandom::Order(TArray<FArcCraftChairCandidate>& Candidates, const FRandomStream& Stream) const
{
	TArray<bool> Picked;
	Picked.SetNumZeroed(Candidates.Num());

	TArray<FArcCraftChairCandidate> Ordered;
	Ordered.Reserve(Candidates.Num());

	while (Ordered.Num() < Candidates.Num())
	{
		float TotalWeight = 0.0f;
		for (int32 Idx = 0; Idx < Candidates.Num(); ++Idx)
		{
			if (!Picked[Idx] && Candidates[Idx].Weight > 0.0f)
			{
				TotalWeight += Candidates[Idx].Weight;
			}
		}
		if (TotalWeight <= 0.0f)
		{
			break;
		}

		const float Roll = Stream.FRand() * TotalWeight;
		float Accumulator = 0.0f;
		int32 PickedIdx = INDEX_NONE;
		for (int32 Idx = 0; Idx < Candidates.Num(); ++Idx)
		{
			if (Picked[Idx] || Candidates[Idx].Weight <= 0.0f)
			{
				continue;
			}
			Accumulator += Candidates[Idx].Weight;
			PickedIdx = Idx;
			if (Roll < Accumulator)
			{
				break;
			}
		}

		Picked[PickedIdx] = true;
		Ordered.Add(Candidates[PickedIdx]);
	}

	for (int32 Idx = 0; Idx < Candidates.Num(); ++Idx)
	{
		if (!Picked[Idx])
		{
			Ordered.Add(Candidates[Idx]);
		}
	}

	Candidates = MoveTemp(Ordered);
}

void FArcCraftSlotSelector_IngredientShare::Order(TArray<FArcCraftChairCandidate>& Candidates, const FRandomStream& Stream) const
{
	Candidates.StableSort([](const FArcCraftChairCandidate& A, const FArcCraftChairCandidate& B)
	{
		return A.IngredientShare > B.IngredientShare;
	});
}

void FArcCraftSlotSelector_TagPriority::Order(TArray<FArcCraftChairCandidate>& Candidates, const FRandomStream& Stream) const
{
	TMap<int32, int32> RankByIndex;
	RankByIndex.Reserve(Candidates.Num());
	for (const FArcCraftChairCandidate& Candidate : Candidates)
	{
		int32 Rank = Tags.Num();
		for (int32 TagIdx = 0; TagIdx < Tags.Num(); ++TagIdx)
		{
			if (Candidate.MatchedTags.HasTag(Tags[TagIdx]))
			{
				Rank = TagIdx;
				break;
			}
		}
		RankByIndex.Add(Candidate.Index, Rank);
	}

	Candidates.StableSort([&RankByIndex](const FArcCraftChairCandidate& A, const FArcCraftChairCandidate& B)
	{
		return RankByIndex.FindChecked(A.Index) < RankByIndex.FindChecked(B.Index);
	});
}
