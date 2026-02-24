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

UArcCraftTickProcessor::UArcCraftTickProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server);
}

void UArcCraftTickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcCraftQueueFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcCraftOutputFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcCraftInputFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcCraftStationConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcCraftStationTag>(EMassFragmentPresence::All);
	// Only process AutoTick entities — InteractionCheck entities are never iterated
	EntityQuery.AddTagRequirement<FArcCraftAutoTickTag>(EMassFragmentPresence::All);
}

void UArcCraftTickProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context,
		[DeltaTime](FMassExecutionContext& Ctx)
		{
			const FArcCraftStationConfigFragment& Config = Ctx.GetConstSharedFragment<FArcCraftStationConfigFragment>();

			TArrayView<FArcCraftQueueFragment> QueueFragments = Ctx.GetMutableFragmentView<FArcCraftQueueFragment>();
			TArrayView<FArcCraftOutputFragment> OutputFragments = Ctx.GetMutableFragmentView<FArcCraftOutputFragment>();
			const TConstArrayView<FArcCraftInputFragment> InputFragments = Ctx.GetFragmentView<FArcCraftInputFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcCraftQueueFragment& Queue = QueueFragments[EntityIt];
				FArcCraftOutputFragment& Output = OutputFragments[EntityIt];
				const FArcCraftInputFragment& Input = InputFragments[EntityIt];

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
					// Build output using the static utility
					FArcItemSpec OutputSpec = FArcCraftOutputBuilder::BuildFromSpecs(
						Entry.Recipe, Input.InputItems);

					Output.OutputItems.Add(MoveTemp(OutputSpec));

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
