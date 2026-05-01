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

#include "ArcCraft/Mass/ArcCraftTickProcessor.h"

#include "MassExecutionContext.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Recipe/ArcCraftOutputBuilder.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "Items/ArcItemStackMethod.h"
#include "Items/ArcItemDefinition.h"

UArcCraftTickProcessor::UArcCraftTickProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcCraftTickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcCraftQueueFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcCraftOutputFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcCraftInputFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcCraftStationConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcCraftStationTag>(EMassFragmentPresence::All);
	// Only process AutoTick entities — InteractionCheck entities are never iterated
	EntityQuery.AddTagRequirement<FArcCraftAutoTickTag>(EMassFragmentPresence::All);
}

namespace ArcCraftTickProcessorInternal
{
	/** Match and consume recipe ingredients from the input fragment.
	 *  Returns true if all ingredients were satisfied and consumed. */
	bool ConsumeIngredients(
		TArray<FArcItemSpec>& InputItems,
		const UArcRecipeDefinition* Recipe)
	{
		if (!Recipe)
		{
			return false;
		}

		const int32 NumIngredients = Recipe->Ingredients.Num();
		if (NumIngredients == 0)
		{
			return true;
		}

		TSet<int32> UsedIndices;
		TArray<TPair<int32, int32>> ItemsToConsume;

		for (int32 SlotIdx = 0; SlotIdx < NumIngredients; ++SlotIdx)
		{
			const FArcRecipeIngredient* Ingredient = Recipe->GetIngredientBase(SlotIdx);
			if (!Ingredient)
			{
				return false;
			}

			int32 MatchedIndex = INDEX_NONE;
			for (int32 ItemIdx = 0; ItemIdx < InputItems.Num(); ++ItemIdx)
			{
				if (UsedIndices.Contains(ItemIdx))
				{
					continue;
				}

				const FArcItemSpec& Spec = InputItems[ItemIdx];
				if (!Spec.GetItemDefinitionId().IsValid())
				{
					continue;
				}

				if (!Ingredient->DoesItemSatisfy(Spec, nullptr))
				{
					continue;
				}

				if (Spec.Amount < static_cast<uint16>(Ingredient->Amount))
				{
					continue;
				}

				MatchedIndex = ItemIdx;
				break;
			}

			if (MatchedIndex == INDEX_NONE)
			{
				return false;
			}

			UsedIndices.Add(MatchedIndex);

			if (Ingredient->bConsumeOnCraft)
			{
				ItemsToConsume.Add({MatchedIndex, Ingredient->Amount});
			}
		}

		// Remove consumed amounts in reverse index order to preserve indices
		ItemsToConsume.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B)
		{
			return A.Key > B.Key;
		});

		for (const TPair<int32, int32>& Pair : ItemsToConsume)
		{
			FArcItemSpec& Spec = InputItems[Pair.Key];
			if (Spec.Amount <= static_cast<uint16>(Pair.Value))
			{
				InputItems.RemoveAt(Pair.Key);
			}
			else
			{
				Spec.Amount -= static_cast<uint16>(Pair.Value);
			}
		}

		return true;
	}
}

void UArcCraftTickProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcCraftTick);

	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context,
		[DeltaTime](FMassExecutionContext& Ctx)
		{
			const FArcCraftStationConfigFragment& Config = Ctx.GetConstSharedFragment<FArcCraftStationConfigFragment>();

			TArrayView<FArcCraftQueueFragment> QueueFragments = Ctx.GetMutableFragmentView<FArcCraftQueueFragment>();
			TArrayView<FArcCraftOutputFragment> OutputFragments = Ctx.GetMutableFragmentView<FArcCraftOutputFragment>();
			TArrayView<FArcCraftInputFragment> InputFragments = Ctx.GetMutableFragmentView<FArcCraftInputFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcCraftQueueFragment& Queue = QueueFragments[EntityIt];
				FArcCraftOutputFragment& Output = OutputFragments[EntityIt];
				FArcCraftInputFragment& Input = InputFragments[EntityIt];

				if (Queue.Entries.IsEmpty())
				{
					continue;
				}

				// Find or confirm active entry (lowest priority)
				if (Queue.ActiveEntryIndex == INDEX_NONE || !Queue.Entries.IsValidIndex(Queue.ActiveEntryIndex))
				{
					int32 BestIdx = INDEX_NONE;
					int32 BestPriority = INT_MAX;
					for (int32 Idx = 0; Idx < Queue.Entries.Num(); ++Idx)
					{
						if (Queue.Entries[Idx].Priority < BestPriority)
						{
							BestPriority = Queue.Entries[Idx].Priority;
							BestIdx = Idx;
						}
					}

					// Consume ingredients when activating a new entry
					if (BestIdx != INDEX_NONE)
					{
						FArcCraftQueueEntry& Candidate = Queue.Entries[BestIdx];
						if (!ArcCraftTickProcessorInternal::ConsumeIngredients(Input.InputItems, Candidate.Recipe))
						{
							// Leave entry as Pending — ProductionGate manages removal
							continue;
						}
						Candidate.State = EArcCraftQueueEntryState::Active;
					}

					Queue.ActiveEntryIndex = BestIdx;
				}

				if (Queue.ActiveEntryIndex == INDEX_NONE)
				{
					continue;
				}

				FArcCraftQueueEntry& Entry = Queue.Entries[Queue.ActiveEntryIndex];
				if (!Entry.Recipe)
				{
					continue;
				}

				const float CraftTime = Entry.Recipe->CraftTime;
				if (CraftTime <= 0.0f)
				{
					continue;
				}

				Entry.ElapsedTickTime += DeltaTime;

				if (Entry.ElapsedTickTime >= CraftTime)
				{
					// Quality data is not cached on queue entries (known simplification
					// shared with actor path) — use default 1.0 for all ingredient slots.
					const int32 NumIngredients = Entry.Recipe->Ingredients.Num();
					TArray<float> QualityMults;
					QualityMults.SetNum(NumIngredients);
					for (int32 i = 0; i < NumIngredients; ++i)
					{
						QualityMults[i] = 1.0f;
					}

					FArcItemSpec OutputSpec = FArcCraftOutputBuilder::Build(
						Entry.Recipe, Input.InputItems, QualityMults, 1.0f);

					const UArcItemDefinition* OutputDef = OutputSpec.GetItemDefinition();
					const FArcItemStackMethod* StackMethod = OutputDef ? OutputDef->GetStackMethod<FArcItemStackMethod>() : nullptr;
					if (!StackMethod || !StackMethod->TryStackSpec(Output.OutputItems, MoveTemp(OutputSpec)))
					{
						Output.OutputItems.Add(MoveTemp(OutputSpec));
					}
					Output.bOutputDirty = true;

					Entry.CompletedAmount++;
					Entry.ElapsedTickTime = 0.0f;
					Entry.StartTimestamp = FDateTime::UtcNow().ToUnixTimestamp();

					// Check if entry is fully complete
					if (Entry.CompletedAmount >= Entry.Amount)
					{
						Queue.Entries.RemoveAt(Queue.ActiveEntryIndex);
						Queue.ActiveEntryIndex = INDEX_NONE;
					}
				}
			}
		}
	);
}
