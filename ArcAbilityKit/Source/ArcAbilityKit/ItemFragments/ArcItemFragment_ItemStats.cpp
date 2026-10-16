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

#include "ItemFragments/ArcItemFragment_ItemStats.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"

void FArcItemFragment_ItemStats::OnItemInitialize(const FArcItemData* InItem) const
{
	FArcItemInstance_ItemStats* Instance = ArcItemsHelper::FindMutableInstance<FArcItemInstance_ItemStats>(InItem);
	if (Instance == nullptr)
	{
		return;
	}
	Recalculate(*Instance, InItem);
}

void FArcItemFragment_ItemStats::OnItemChanged(const FArcItemData* InItem) const
{
	FArcItemInstance_ItemStats* Instance = ArcItemsHelper::FindMutableInstance<FArcItemInstance_ItemStats>(InItem);
	if (Instance == nullptr)
	{
		return;
	}
	Recalculate(*Instance, InItem);
}

float FArcItemInstance_ItemStats::GetStatValue(const FArcAttributeRef& InStat, const FGameplayTagContainer& InTags) const
{
	return ArcItemStats::ReadStat(ReplicatedItemStats, InStat, InTags).Get(0.f);
}

namespace ArcItemFragmentItemStatsPrivate
{
	struct FStatAggregator
	{
		FArcAttributeRef Stat;
		FGameplayTagContainer StatTags;
		TOptional<float> Additive;
		TOptional<float> Multiply;
		TOptional<float> Division;

		// D-77 e: the same stat twice on one item keeps the highest value per modifier type.
		void Accumulate(const FArcItemAttributeStat& S)
		{
			TOptional<float>* Slot = nullptr;
			switch (S.Type)
			{
				case EArcModType::Additive:
					Slot = &Additive;
					break;
				case EArcModType::Multiply:
					Slot = &Multiply;
					break;
				case EArcModType::Division:
					Slot = &Division;
					break;
				default:
					break;
			}
			if (Slot == nullptr)
			{
				return;
			}

			const float RawValue = S.Value.GetValue();
			*Slot = Slot->IsSet() ? FMath::Max(Slot->GetValue(), RawValue) : RawValue;
		}
	};

	void AccumulateStats(TArray<FStatAggregator>& Aggregators, const FArcItemFragment_ItemStats& Fragment)
	{
		for (const FArcItemAttributeStat& S : Fragment.DefaultStats)
		{
			if (!S.IsValid())
			{
				continue;
			}
			FStatAggregator* Found = Aggregators.FindByPredicate([&S](const FStatAggregator& Entry)
			{
				return Entry.Stat == S.Stat && Entry.StatTags == S.StatTags;
			});
			if (Found == nullptr)
			{
				Found = &Aggregators.AddDefaulted_GetRef();
				Found->Stat = S.Stat;
				Found->StatTags = S.StatTags;
			}
			Found->Accumulate(S);
		}
	}

	struct FStatRead
	{
		float PositiveAdditive = 0.f;
		float PenaltyAdditive = 0.f;
		float PositiveMultiply = 1.f;
		float PenaltyMultiply = 0.f;
		float PositiveDivision = 1.f;
		float PenaltyDivision = 0.f;

		// D42: positives take max(specific, general) per modifier kind; penalties (Additive < 0, Multiply < 1, Division > 1) all stack additively.
		void Add(const FArcItemStatReplicated& Entry)
		{
			if (Entry.Additive >= 0.f)
			{
				PositiveAdditive = FMath::Max(PositiveAdditive, Entry.Additive);
			}
			else
			{
				PenaltyAdditive += Entry.Additive;
			}

			if (Entry.Multiply >= 1.f)
			{
				PositiveMultiply = FMath::Max(PositiveMultiply, Entry.Multiply);
			}
			else
			{
				PenaltyMultiply += Entry.Multiply - 1.f;
			}

			if (Entry.Division <= 1.f)
			{
				PositiveDivision = FMath::Min(PositiveDivision, Entry.Division);
			}
			else
			{
				PenaltyDivision += Entry.Division - 1.f;
			}
		}

		float Resolve() const
		{
			const float Additive = PositiveAdditive + PenaltyAdditive;
			const float Multiply = PositiveMultiply + PenaltyMultiply;
			const float Division = PositiveDivision + PenaltyDivision;
			return (Additive * Multiply) / (FMath::IsNearlyZero(Division) ? 1.f : Division);
		}
	};
}

void FArcItemFragment_ItemStats::Recalculate(FArcItemInstance_ItemStats& Instance, const FArcItemData* Item)
{
	TArray<const FArcItemFragment_ItemStats*> Fragments;

	const FArcItemFragment_ItemStats* HostFragment = ArcItemsHelper::FindFragment<FArcItemFragment_ItemStats>(Item);
	if (HostFragment != nullptr)
	{
		Fragments.Add(HostFragment);
	}

	const TArray<const FArcItemData*>& ItemsInSockets = Item->GetItemsInSockets();
	for (const FArcItemData* SocketItem : ItemsInSockets)
	{
		if (SocketItem == nullptr)
		{
			continue;
		}
		const FArcItemFragment_ItemStats* SocketFragment = ArcItemsHelper::FindFragment<FArcItemFragment_ItemStats>(SocketItem);
		if (SocketFragment != nullptr)
		{
			Fragments.Add(SocketFragment);
		}
	}

	ArcItemStats::BuildEntries(Fragments, Instance.ReplicatedItemStats);
}

namespace ArcItemStats
{
	void BuildEntries(TConstArrayView<const FArcItemFragment_ItemStats*> Fragments, TArray<FArcItemStatReplicated>& OutEntries)
	{
		TArray<ArcItemFragmentItemStatsPrivate::FStatAggregator> Aggregators;
		for (const FArcItemFragment_ItemStats* Fragment : Fragments)
		{
			if (Fragment != nullptr)
			{
				ArcItemFragmentItemStatsPrivate::AccumulateStats(Aggregators, *Fragment);
			}
		}

		OutEntries.Empty(Aggregators.Num());
		for (const ArcItemFragmentItemStatsPrivate::FStatAggregator& Aggregator : Aggregators)
		{
			FArcItemStatReplicated& Rep = OutEntries.AddDefaulted_GetRef();
			Rep.Stat = Aggregator.Stat;
			Rep.StatTags = Aggregator.StatTags;
			Rep.Additive = Aggregator.Additive.Get(0.f);
			Rep.Multiply = Aggregator.Multiply.Get(1.f);
			Rep.Division = Aggregator.Division.Get(1.f);
		}
	}

	TOptional<float> ReadStat(TConstArrayView<FArcItemStatReplicated> Entries, const FArcAttributeRef& Stat,
	                          const FGameplayTagContainer& Tags)
	{
		ArcItemFragmentItemStatsPrivate::FStatRead Read;
		bool bFound = false;
		for (const FArcItemStatReplicated& Entry : Entries)
		{
			if (Entry.Stat == Stat && Tags.HasAll(Entry.StatTags))
			{
				Read.Add(Entry);
				bFound = true;
			}
		}
		if (!bFound)
		{
			return TOptional<float>();
		}
		return Read.Resolve();
	}
}
