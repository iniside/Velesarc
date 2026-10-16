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

#include "ArcCraft/ArcCraftTags.h"
#include "ArcCraft/Recipe/ArcCraftModifierSlot.h"
#include "ArcCraft/Recipe/ArcCraftSlotSelector.h"
#include "Effects/ArcEffectDefinition.h"
#include "Math/RandomStream.h"

namespace ArcCraftSlotResolverImpl
{
	struct FStatKey
	{
		EArcCraftModifierResultType Type = EArcCraftModifierResultType::None;
		const FArcItemAttributeStat* Stat = nullptr;
		const FArcDirectModifier* Direct = nullptr;
		const UArcEffectDefinition* Effect = nullptr;
		float Magnitude = 0.0f;
	};

	struct FChairCandidate
	{
		FGameplayTagContainer ChairTags;
		TArray<FStatKey> Keys;
	};

	struct FGroup
	{
		int32 ModifierIndex = INDEX_NONE;
		int32 GroupIndex = INDEX_NONE;
		TArray<int32> Members;
	};

	bool IsPenalty(const FArcCraftPendingModifier& Pending)
	{
		return Pending.SlotTags.HasTag(ArcCraftTags::Modifier_Penalty);
	}

	bool SameKey(const FStatKey& A, const FStatKey& B)
	{
		if (A.Type != B.Type)
		{
			return false;
		}
		switch (A.Type)
		{
		case EArcCraftModifierResultType::Stat:
			return *A.Stat == *B.Stat;
		case EArcCraftModifierResultType::AttributeModifier:
			return A.Direct->Attribute == B.Direct->Attribute
				&& A.Direct->SourceTagReqs == B.Direct->SourceTagReqs
				&& A.Direct->TargetTagReqs == B.Direct->TargetTagReqs;
		case EArcCraftModifierResultType::Effect:
			return A.Effect == B.Effect;
		default:
			return false;
		}
	}

	bool SharesKey(const TArray<FStatKey>& Keys, const TArray<FStatKey>& Others)
	{
		for (const FStatKey& Key : Keys)
		{
			for (const FStatKey& Other : Others)
			{
				if (SameKey(Key, Other))
				{
					return true;
				}
			}
		}
		return false;
	}

	void CollectKeys(const FArcCraftModifierResult& Result, TArray<FStatKey>& OutKeys)
	{
		switch (Result.Type)
		{
		case EArcCraftModifierResultType::Stat:
			{
				FStatKey& Key = OutKeys.AddDefaulted_GetRef();
				Key.Type = Result.Type;
				Key.Stat = &Result.Stat;
				const float Value = Result.Stat.Value.Value;
				Key.Magnitude = Result.Stat.Type == EArcModType::Additive ? FMath::Abs(Value) : FMath::Abs(Value - 1.0f);
			}
			break;
		case EArcCraftModifierResultType::AttributeModifier:
			for (const FArcDirectModifier& Modifier : Result.AttributeModifiers)
			{
				FStatKey& Key = OutKeys.AddDefaulted_GetRef();
				Key.Type = Result.Type;
				Key.Direct = &Modifier;
				Key.Magnitude = FMath::Abs(Modifier.Magnitude);
			}
			break;
		case EArcCraftModifierResultType::Effect:
			if (Result.Effect)
			{
				FStatKey& Key = OutKeys.AddDefaulted_GetRef();
				Key.Type = Result.Type;
				Key.Effect = Result.Effect;
			}
			break;
		default:
			break;
		}
	}

	FArcCraftChairCandidate MakeSelectorCandidate(int32 Index, const FArcCraftPendingModifier& Pending)
	{
		FArcCraftChairCandidate Candidate;
		Candidate.Index = Index;
		Candidate.ModifierIndex = Pending.ModifierIndex;
		Candidate.GroupIndex = Pending.GroupIndex;
		Candidate.Weight = Pending.EffectiveWeight;
		Candidate.IngredientShare = Pending.IngredientShare;
		Candidate.MatchedTags = Pending.MatchedTags;
		return Candidate;
	}

	void OrderCandidates(TArray<FArcCraftChairCandidate>& Order, const FArcCraftSlotSelector& Selector, const FRandomStream& Stream)
	{
		Order.Sort([](const FArcCraftChairCandidate& A, const FArcCraftChairCandidate& B)
		{
			if (A.ModifierIndex != B.ModifierIndex)
			{
				return A.ModifierIndex < B.ModifierIndex;
			}
			if (A.GroupIndex != B.GroupIndex)
			{
				return A.GroupIndex < B.GroupIndex;
			}
			return A.Index < B.Index;
		});
		Selector.Order(Order, Stream);
	}

	TArray<int32> CollectApplied(const TArray<bool>& bApplies)
	{
		TArray<int32> Applied;
		for (int32 Idx = 0; Idx < bApplies.Num(); ++Idx)
		{
			if (bApplies[Idx])
			{
				Applied.Add(Idx);
			}
		}
		return Applied;
	}

	/** Returns the candidate index owning each chair (parallel to Chairs), INDEX_NONE for an empty chair. */
	TArray<int32> FillChairs(
		const TArray<FArcCraftChairCandidate>& Order,
		const TArray<FChairCandidate>& Candidates,
		const TArray<FArcCraftModifierSlot>& Slots,
		const TArray<int32>& Chairs)
	{
		TArray<int32> Owners;
		Owners.Init(INDEX_NONE, Chairs.Num());

		for (const FArcCraftChairCandidate& Ordered : Order)
		{
			const FChairCandidate& Incoming = Candidates[Ordered.Index];

			TArray<int32> Conflicting;
			bool bIncomingWins = true;
			for (int32 Pos = 0; Pos < Chairs.Num(); ++Pos)
			{
				if (Owners[Pos] == INDEX_NONE)
				{
					continue;
				}
				bool bConflict = false;
				for (const FStatKey& IncomingKey : Incoming.Keys)
				{
					for (const FStatKey& PlacedKey : Candidates[Owners[Pos]].Keys)
					{
						if (SameKey(IncomingKey, PlacedKey))
						{
							bConflict = true;
							bIncomingWins = bIncomingWins && IncomingKey.Magnitude > PlacedKey.Magnitude;
						}
					}
				}
				if (bConflict)
				{
					Conflicting.Add(Pos);
				}
			}
			if (!bIncomingWins)
			{
				continue;
			}

			int32 BestPos = INDEX_NONE;
			int32 BestTagCount = MAX_int32;
			for (int32 Pos = 0; Pos < Chairs.Num(); ++Pos)
			{
				if (Owners[Pos] != INDEX_NONE && !Conflicting.Contains(Pos))
				{
					continue;
				}
				const FGameplayTagContainer& ChairTags = Slots[Chairs[Pos]].SlotTags;
				if (!ChairTags.HasAll(Incoming.ChairTags))
				{
					continue;
				}
				if (ChairTags.Num() < BestTagCount)
				{
					BestTagCount = ChairTags.Num();
					BestPos = Pos;
				}
			}
			if (BestPos == INDEX_NONE)
			{
				continue;
			}

			for (const int32 Pos : Conflicting)
			{
				Owners[Pos] = INDEX_NONE;
			}
			Owners[BestPos] = Ordered.Index;
		}

		return Owners;
	}
}

TArray<int32> FArcCraftSlotResolver::Resolve(
	const TArray<FArcCraftPendingModifier>& PendingModifiers,
	const TArray<FArcCraftModifierSlot>& Slots,
	const FArcCraftSlotSelector* Selector,
	const FRandomStream& Stream)
{
	using namespace ArcCraftSlotResolverImpl;

	static const FArcCraftSlotSelector_HighestWeight DefaultSelector;
	const FArcCraftSlotSelector& ActiveSelector = Selector ? *Selector : static_cast<const FArcCraftSlotSelector&>(DefaultSelector);

	TArray<bool> bApplies;
	bApplies.SetNumZeroed(PendingModifiers.Num());

	TArray<FGroup> Groups;
	for (int32 Idx = 0; Idx < PendingModifiers.Num(); ++Idx)
	{
		const FArcCraftPendingModifier& Pending = PendingModifiers[Idx];
		if (Pending.GroupIndex == INDEX_NONE)
		{
			bApplies[Idx] = true;
			continue;
		}
		FGroup* Group = Groups.FindByPredicate([&Pending](const FGroup& Existing)
		{
			return Existing.ModifierIndex == Pending.ModifierIndex && Existing.GroupIndex == Pending.GroupIndex;
		});
		if (Group == nullptr)
		{
			Group = &Groups.AddDefaulted_GetRef();
			Group->ModifierIndex = Pending.ModifierIndex;
			Group->GroupIndex = Pending.GroupIndex;
		}
		Group->Members.Add(Idx);
	}

	TArray<int32> BonusChairs;
	TArray<int32> PenaltyChairs;
	for (int32 ChairIdx = 0; ChairIdx < Slots.Num(); ++ChairIdx)
	{
		const FGameplayTagContainer& ChairTags = Slots[ChairIdx].SlotTags;
		if (ChairTags.IsEmpty())
		{
			continue;
		}
		if (ChairTags.HasTag(ArcCraftTags::Modifier_Penalty))
		{
			PenaltyChairs.Add(ChairIdx);
		}
		else
		{
			BonusChairs.Add(ChairIdx);
		}
	}

	TArray<FChairCandidate> BonusCandidates;
	BonusCandidates.SetNum(Groups.Num());
	TArray<FArcCraftChairCandidate> BonusOrder;
	for (int32 GroupIdx = 0; GroupIdx < Groups.Num(); ++GroupIdx)
	{
		FChairCandidate& Candidate = BonusCandidates[GroupIdx];
		for (const int32 Member : Groups[GroupIdx].Members)
		{
			const FArcCraftPendingModifier& Pending = PendingModifiers[Member];
			if (IsPenalty(Pending) || Pending.SlotTags.IsEmpty())
			{
				continue;
			}
			Candidate.ChairTags.AppendTags(Pending.SlotTags);
			CollectKeys(Pending.Result, Candidate.Keys);
		}

		if (Candidate.ChairTags.IsEmpty())
		{
			for (const int32 Member : Groups[GroupIdx].Members)
			{
				bApplies[Member] = !IsPenalty(PendingModifiers[Member]);
			}
			continue;
		}
		BonusOrder.Add(MakeSelectorCandidate(GroupIdx, PendingModifiers[Groups[GroupIdx].Members[0]]));
	}

	if (BonusChairs.Num() == 0 || BonusOrder.Num() == 0)
	{
		return CollectApplied(bApplies);
	}

	OrderCandidates(BonusOrder, ActiveSelector, Stream);
	const TArray<int32> BonusOwners = FillChairs(BonusOrder, BonusCandidates, Slots, BonusChairs);

	TArray<FStatKey> PlacedBonusKeys;
	for (const int32 GroupIdx : BonusOwners)
	{
		if (GroupIdx != INDEX_NONE)
		{
			PlacedBonusKeys.Append(BonusCandidates[GroupIdx].Keys);
		}
	}

	TArray<int32> PenaltyPending;
	TArray<FChairCandidate> PenaltyCandidates;
	TArray<FArcCraftChairCandidate> PenaltyOrder;
	for (const int32 GroupIdx : BonusOwners)
	{
		if (GroupIdx == INDEX_NONE)
		{
			continue;
		}
		for (const int32 Member : Groups[GroupIdx].Members)
		{
			const FArcCraftPendingModifier& Pending = PendingModifiers[Member];
			if (!IsPenalty(Pending))
			{
				bApplies[Member] = true;
				continue;
			}

			FChairCandidate Candidate;
			Candidate.ChairTags = Pending.SlotTags;
			CollectKeys(Pending.Result, Candidate.Keys);
			if (SharesKey(Candidate.Keys, PlacedBonusKeys))
			{
				continue;
			}
			PenaltyOrder.Add(MakeSelectorCandidate(PenaltyCandidates.Num(), Pending));
			PenaltyCandidates.Add(MoveTemp(Candidate));
			PenaltyPending.Add(Member);
		}
	}

	if (PenaltyChairs.Num() > 0 && PenaltyOrder.Num() > 0)
	{
		OrderCandidates(PenaltyOrder, ActiveSelector, Stream);
		for (const int32 PenaltyIdx : FillChairs(PenaltyOrder, PenaltyCandidates, Slots, PenaltyChairs))
		{
			if (PenaltyIdx != INDEX_NONE)
			{
				bApplies[PenaltyPending[PenaltyIdx]] = true;
			}
		}
	}

	return CollectApplied(bApplies);
}
