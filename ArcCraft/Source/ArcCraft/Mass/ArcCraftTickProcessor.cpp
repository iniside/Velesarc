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

#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "ArcMass/Debugger/ArcEconomyTrace.h"
#include "ArcCraft/Mass/ArcCraftDeliveryConfigFragment.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftQueueChangedSignal.h"
#include "ArcCraft/Rarity/ArcCraftRarity.h"
#include "ArcCraft/Recipe/ArcCraftContext.h"
#include "ArcCraft/Recipe/ArcCraftOutputBuilder.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Station/ArcCraftStationImpl.h"
#include "ArcCraft/Station/ArcCraftOutputDelivery.h"
#include "Items/ArcItemStackMethod.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemId.h"
#include "Modifiers/ArcModifierFunctions.h"

UArcCraftTickProcessor::UArcCraftTickProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcCraftTickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcCraftQueueFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcCraftOutputFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcCraftInputFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcCraftStationConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcCraftDeliveryConfigFragment>(EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FArcCraftStationTag>(EMassFragmentPresence::All);
	// Only process AutoTick entities — InteractionCheck entities are never iterated
	EntityQuery.AddTagRequirement<FArcCraftAutoTickTag>(EMassFragmentPresence::All);

	// Require signal subsystem so we can post QueueChanged after every queue mutation.
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
}

namespace ArcCraftTickProcessorInternal
{
	DEFINE_LOG_CATEGORY_STATIC(LogArcCraftTick, Log, All);

	float ComputeSkillMargin(FMassEntityManager& EntityManager, const FMassEntityHandle Instigator, const FArcRecipeDefinitionData& Recipe)
	{
		if (!EntityManager.IsEntityValid(Instigator) || !Recipe.RequiredSkill.IsValid())
		{
			return 0.0f;
		}

		const float SkillLevel = ArcAttributes::GetValue(EntityManager, Instigator, Recipe.RequiredSkill);
		return FMath::Max(0.0f, SkillLevel - static_cast<float>(Recipe.RequiredSkillLevel));
	}

	FString DescribeEntity(const FMassEntityHandle Entity)
	{
		return FString::Printf(TEXT("%d:%d"), Entity.Index, Entity.SerialNumber);
	}

	FString DescribeRecipe(const FPrimaryAssetId& RecipeId)
	{
		return RecipeId.IsValid() ? RecipeId.ToString() : FString(TEXT("<null>"));
	}

	const FName TraceType_CraftStation(TEXT("CraftStation"));
	const FName TraceMetric_Stations(TEXT("Stations"));
	const FName TraceMetric_Chunks(TEXT("Chunks"));
	const FName TraceMetric_QueueEntries(TEXT("QueueEntries"));
	const FName TraceMetric_ActiveEntries(TEXT("ActiveEntries"));
	const FName TraceMetric_PendingEntries(TEXT("PendingEntries"));
	const FName TraceMetric_OutputBlockedEntries(TEXT("OutputBlockedEntries"));
	const FName TraceMetric_IngredientsUnsatisfied(TEXT("IngredientsUnsatisfied"));
	const FName TraceMetric_InvalidEntries(TEXT("InvalidEntries"));
}

void UArcCraftTickProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcCraftTick);

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();

	using namespace ArcCraftTickProcessorInternal;

	int32 DiagNumChunks = 0;
	int32 DiagNumEntities = 0;
	int32 TotalQueueEntries = 0;
	int32 TotalActiveEntries = 0;
	int32 TotalPendingEntries = 0;
	int32 TotalOutputBlocked = 0;
	int32 TotalIngredientsUnsatisfied = 0;
	int32 TotalInvalidEntries = 0;

	EntityQuery.ForEachEntityChunk(Context,
		[DeltaTime, &DiagNumChunks, &DiagNumEntities, &TotalQueueEntries, &TotalActiveEntries, &TotalPendingEntries,
		 &TotalOutputBlocked, &TotalIngredientsUnsatisfied, &TotalInvalidEntries, &SignalSubsystem](FMassExecutionContext& Ctx)
		{
			++DiagNumChunks;
			DiagNumEntities += Ctx.GetNumEntities();

			const FArcCraftStationConfigFragment& Config = Ctx.GetConstSharedFragment<FArcCraftStationConfigFragment>();
			const FArcCraftDeliveryConfigFragment* DeliveryCfg = Ctx.GetConstSharedFragmentPtr<FArcCraftDeliveryConfigFragment>();

			TArrayView<FArcCraftQueueFragment> QueueFragments = Ctx.GetMutableFragmentView<FArcCraftQueueFragment>();
			TArrayView<FArcCraftOutputFragment> OutputFragments = Ctx.GetMutableFragmentView<FArcCraftOutputFragment>();
			TArrayView<FArcCraftInputFragment> InputFragments = Ctx.GetMutableFragmentView<FArcCraftInputFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle StationEntity = EntityIt.GetEntityHandle();
				FArcCraftQueueFragment& Queue = QueueFragments[EntityIt];
				FArcCraftOutputFragment& Output = OutputFragments[EntityIt];
				FArcCraftInputFragment& Input = InputFragments[EntityIt];

				if (Queue.Entries.IsEmpty())
				{
					continue;
				}

				const int32 MaxActiveEntries = FMath::Max(1, Config.ConcurrentCraftSlots);

				TotalQueueEntries += Queue.Entries.Num();

				int32 NumActiveEntries = 0;
				TArray<int32> PendingIndices;
				for (int32 EntryIdx = 0; EntryIdx < Queue.Entries.Num(); ++EntryIdx)
				{
					TotalOutputBlocked += Queue.Entries[EntryIdx].bOutputBlocked ? 1 : 0;
					if (Queue.Entries[EntryIdx].State == EArcCraftQueueEntryState::Active)
					{
						++NumActiveEntries;
					}
					else if (Queue.Entries[EntryIdx].State == EArcCraftQueueEntryState::Pending)
					{
						PendingIndices.Add(EntryIdx);
					}
				}

				PendingIndices.StableSort([&Queue](const int32 LhsIdx, const int32 RhsIdx) -> bool
				{
					return Queue.Entries[LhsIdx].Priority < Queue.Entries[RhsIdx].Priority;
				});

				int32 NumPromoted = 0;
				for (int32 PendingIdx = 0;
					 PendingIdx < PendingIndices.Num() && NumActiveEntries < MaxActiveEntries;
					 ++PendingIdx)
				{
					FArcCraftQueueEntry& Candidate = Queue.Entries[PendingIndices[PendingIdx]];
					const FArcRecipeDefinitionData* CandidateRecipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(Candidate.RecipeId);
					if (CandidateRecipe == nullptr)
					{
						++TotalInvalidEntries;
						continue;
					}

					// Consume synchronously so each promotion matches against the already-reduced array.
					TArray<FArcItemSpec> MatchedSpecs;
					TArray<float> SlotRaritySteps;
					TArray<bool> SlotFilled;
					if (!FArcCraftItemSource::MatchAndConsumeFromSpecs(
						Input.InputItems, *CandidateRecipe, /*bConsume=*/true,
						&MatchedSpecs, &SlotRaritySteps, &SlotFilled))
					{
						++TotalIngredientsUnsatisfied;
						continue;
					}

					Candidate.ConsumedIngredientDefs = ArcCraftStationImpl::ExtractDefIds(MatchedSpecs);
					Candidate.SlotInstanceTags = ArcCraftStationImpl::ExtractInstanceTags(MatchedSpecs);
					Candidate.SlotRaritySteps = MoveTemp(SlotRaritySteps);
					Candidate.CachedAverageR = ArcCraftRarity::ComputeAverageR(
						*CandidateRecipe, Candidate.SlotRaritySteps, SlotFilled);
					Candidate.State = EArcCraftQueueEntryState::Active;
					++NumActiveEntries;
					++NumPromoted;

					TRACE_ARC_ECONOMY_BOOKMARKF(TEXT("PROMOTE-PENDING-TO-ACTIVE station=%s recipe=%s craftTime=%.2f active=%d/%d"),
						*ArcCraftTickProcessorInternal::DescribeEntity(StationEntity),
						*ArcCraftTickProcessorInternal::DescribeRecipe(Candidate.RecipeId),
						CandidateRecipe->CraftTime,
						NumActiveEntries,
						MaxActiveEntries);

					SignalSubsystem.SignalEntityDeferred(Ctx, ArcCraftSignals::QueueChanged, StationEntity);
					if (CandidateRecipe->Ingredients.Num() > 0)
					{
						SignalSubsystem.SignalEntityDeferred(Ctx, ArcCraftSignals::InputChanged, StationEntity);
					}
				}

				TotalActiveEntries += NumActiveEntries;
				TotalPendingEntries += PendingIndices.Num() - NumPromoted;

				if (NumActiveEntries == 0)
				{
					continue;
				}

				// Advance over an EntryId snapshot: AdvanceCompletedEntry may RemoveAt, which shifts
				// every later index and dangles any held FArcCraftQueueEntry reference.
				TArray<FGuid> ActiveEntryIds;
				ActiveEntryIds.Reserve(NumActiveEntries);
				for (const FArcCraftQueueEntry& ActiveEntry : Queue.Entries)
				{
					if (ActiveEntry.State != EArcCraftQueueEntryState::Active)
					{
						continue;
					}

					// An unstamped id aliases onto whichever entry sits first, so it would advance
					// and complete a sibling's craft; stall it instead and name the station.
					if (!ActiveEntry.EntryId.IsValid())
					{
						++TotalInvalidEntries;
						continue;
					}

					ActiveEntryIds.Add(ActiveEntry.EntryId);
				}

				for (const FGuid& ActiveEntryId : ActiveEntryIds)
				{
					const int32 EntryIndex = Queue.Entries.IndexOfByPredicate(
						[&ActiveEntryId](const FArcCraftQueueEntry& Candidate) -> bool
						{
							return Candidate.EntryId == ActiveEntryId;
						});
					if (EntryIndex == INDEX_NONE)
					{
						continue;
					}

					FArcCraftQueueEntry& Entry = Queue.Entries[EntryIndex];
					const FArcRecipeDefinitionData* EntryRecipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(Entry.RecipeId);
					if (EntryRecipe == nullptr)
					{
						++TotalInvalidEntries;
						continue;
					}

					// Output blocked (bag full): station is paused. Skip accumulation + completion until the
					// resume subsystem clears the flag on a bag-slot-freed event.
					if (Entry.bOutputBlocked)
					{
						continue;
					}

					const float CraftTime = EntryRecipe->CraftTime;
					if (CraftTime <= 0.0f)
					{
						++TotalInvalidEntries;
						continue;
					}

					Entry.ElapsedTickTime += DeltaTime;

					if (Entry.ElapsedTickTime < CraftTime)
					{
						continue;
					}

					// Rarity steps are cached at consume time; the rebuilt ingredient specs carry only the instance tags.
					const int32 NumIngredients = EntryRecipe->Ingredients.Num();
					FArcCraftContext CraftContext;
					CraftContext.SlotRaritySteps.SetNum(NumIngredients);
					CraftContext.SlotAmounts.SetNum(NumIngredients);
					for (int32 i = 0; i < NumIngredients; ++i)
					{
						CraftContext.SlotRaritySteps[i] = Entry.SlotRaritySteps.IsValidIndex(i)
							? Entry.SlotRaritySteps[i]
							: 1.0f;
						const FArcRecipeIngredient* Ingredient = EntryRecipe->GetIngredientBase(i);
						const bool bSlotFilled = Entry.ConsumedIngredientDefs.IsValidIndex(i)
							&& Entry.ConsumedIngredientDefs[i].IsValid();
						CraftContext.SlotAmounts[i] = bSlotFilled && Ingredient != nullptr && Ingredient->bConsumeOnCraft
							? FMath::Max(0, Ingredient->Amount)
							: 0;
					}
					CraftContext.AverageR = Entry.CachedAverageR;
					CraftContext.SkillMargin = ArcCraftTickProcessorInternal::ComputeSkillMargin(
						Ctx.GetEntityManagerChecked(), Entry.InstigatorEntity, *EntryRecipe);

					// On a resumed craft deliver the EXACT held specs (recipes may carry random rolls
					// that would re-roll differently on a rebuild); otherwise build fresh.
					TArray<FArcItemSpec> OutputSpecs;
					if (!Entry.HeldOutput.IsEmpty())
					{
						OutputSpecs = Entry.HeldOutput;
					}
					else
					{
						CraftContext.Stream.Initialize(FMath::Rand32());
						UE_LOGF(ArcCraftTickProcessorInternal::LogArcCraftTick, Log, "Craft seed %d recipe=%ls entry=%ls",
							CraftContext.Stream.GetInitialSeed(),
							*ArcCraftTickProcessorInternal::DescribeRecipe(Entry.RecipeId),
							*ActiveEntryId.ToString());

						const TArray<FArcItemSpec> ConsumedSpecs = ArcCraftStationImpl::ReconstructMatchedSpecs(Entry.ConsumedIngredientDefs, Entry.SlotInstanceTags);
						OutputSpecs = FArcCraftOutputBuilder::Build(Entry.RecipeId, *EntryRecipe, ConsumedSpecs, CraftContext);
					}

#if WITH_ARC_ECONOMY_TRACE
					const FString DiagRecipeName = ArcCraftTickProcessorInternal::DescribeRecipe(Entry.RecipeId);
					FString DiagOutputs;
					for (const FArcItemSpec& DiagSpec : OutputSpecs)
					{
						if (!DiagOutputs.IsEmpty())
						{
							DiagOutputs += TEXT(",");
						}
						DiagOutputs += FString::Printf(TEXT("%sx%d"),
							*DiagSpec.GetItemDefinitionId().ToString(), static_cast<int32>(DiagSpec.Amount));
					}
					const int32 DiagCompletedAmount = Entry.CompletedAmount;
					const int32 DiagTotalAmount = Entry.Amount;
#endif
					const FMassEntityHandle InstigatorEntity = Entry.InstigatorEntity;

					const FArcCraftOutputDelivery* MassDelivery = DeliveryCfg
						? DeliveryCfg->OutputDelivery.GetPtr<FArcCraftOutputDelivery>()
						: nullptr;

					// The legacy (no-MassDelivery) path resolves synchronously.
					EArcCraftDeliveryDisposition Disposition = EArcCraftDeliveryDisposition::ResolvedSync;
					if (MassDelivery)
					{
						// Mass-side delivery: routed to the configured store (item store, bag, or
						// output fragment) via a deferred command. The policy reports whether it
						// resolved synchronously or owns advance-or-block via its own deferred command.
						Disposition = MassDelivery->DeliverOutput_Mass(
							Ctx, Ctx.GetEntityManagerChecked(), StationEntity, InstigatorEntity,
							ActiveEntryId, OutputSpecs);
					}
					else
					{
						// Legacy fallback: write directly to FArcCraftOutputFragment.
						for (const FArcItemSpec& OutputSpec : OutputSpecs)
						{
							const FArcItemDefinitionData* OutputDef = OutputSpec.GetItemDefinition();
							const FArcItemStackMethod* StackMethod = OutputDef
								? OutputDef->GetStackMethod<FArcItemStackMethod>()
								: nullptr;
							FArcItemSpec StackSpec = OutputSpec;
							if (!StackMethod || !StackMethod->TryStackSpec(Output.OutputItems, MoveTemp(StackSpec)))
							{
								// New OutputItems line — stamp a fresh stable id so output can be
								// addressed by id. A successful TryStackSpec merge leaves the existing
								// line's id intact (no restamp).
								FArcItemSpec NewSpec = OutputSpec;
								NewSpec.SetItemId(FArcItemId::Generate());
								Output.OutputItems.Add(MoveTemp(NewSpec));
							}
						}
						Output.bOutputDirty = true;

						// OutputItems mutated → signal subscribers (worker-thread-safe deferred
						// form, mirroring the QueueChanged emission in this processor).
						SignalSubsystem.SignalEntityDeferred(Ctx, ArcCraftSignals::OutputChanged, StationEntity);
					}

					TRACE_ARC_ECONOMY_BOOKMARKF(TEXT("CRAFT-COMPLETE station=%s recipe=%s outputs=%s completed=%d/%d path=%s disposition=%s"),
						*ArcCraftTickProcessorInternal::DescribeEntity(StationEntity),
						*DiagRecipeName,
						*DiagOutputs,
						DiagCompletedAmount,
						DiagTotalAmount,
						MassDelivery ? TEXT("mass-delivery") : TEXT("legacy-output-fragment"),
						Disposition == EArcCraftDeliveryDisposition::ResolvedSync ? TEXT("ResolvedSync") : TEXT("DeferredResolve"));

					if (Disposition == EArcCraftDeliveryDisposition::ResolvedSync)
					{
						ArcCraftStationImpl::AdvanceCompletedEntry(Queue, ActiveEntryId);

						SignalSubsystem.SignalEntityDeferred(Ctx, ArcCraftSignals::QueueChanged, StationEntity);
					}
					// DeferredResolve (bag policy): its deferred command owns advance-or-block + the
					// QueueChanged signal. Do NOT advance here and do NOT reset ElapsedTickTime — if the
					// command blocks, the pause-gate stops re-entry next tick; if it advances, the
					// command resets ElapsedTickTime.
				}
			}
		}
	);

	TRACE_ARC_ECONOMY_COUNTER_INT(TraceType_CraftStation, TraceMetric_Stations, DiagNumEntities);
	TRACE_ARC_ECONOMY_COUNTER_INT(TraceType_CraftStation, TraceMetric_Chunks, DiagNumChunks);
	TRACE_ARC_ECONOMY_COUNTER_INT(TraceType_CraftStation, TraceMetric_QueueEntries, TotalQueueEntries);
	TRACE_ARC_ECONOMY_COUNTER_INT(TraceType_CraftStation, TraceMetric_ActiveEntries, TotalActiveEntries);
	TRACE_ARC_ECONOMY_COUNTER_INT(TraceType_CraftStation, TraceMetric_PendingEntries, TotalPendingEntries);
	TRACE_ARC_ECONOMY_COUNTER_INT(TraceType_CraftStation, TraceMetric_OutputBlockedEntries, TotalOutputBlocked);
	TRACE_ARC_ECONOMY_COUNTER_INT(TraceType_CraftStation, TraceMetric_IngredientsUnsatisfied, TotalIngredientsUnsatisfied);
	TRACE_ARC_ECONOMY_COUNTER_INT(TraceType_CraftStation, TraceMetric_InvalidEntries, TotalInvalidEntries);
}
