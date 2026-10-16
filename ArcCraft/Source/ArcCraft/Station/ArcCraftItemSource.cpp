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

#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Station/ArcCraftStationXfer.h"

#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftQueueChangedSignal.h"
#include "ArcCraft/Rarity/ArcCraftRarity.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_InstanceTags.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Pawn/ArcMassFrameworkAgentComponent.h"

// -------------------------------------------------------------------
// FArcCraftItemSource (base)
// -------------------------------------------------------------------

namespace ArcCraftItemSourceLocal
{
	struct FSlotDraw
	{
		int32 ItemIndex = INDEX_NONE;
		int32 Amount = 0;
	};

	bool DrawSlotFromCandidates(
		const TArray<FArcItemSpec>& Items,
		TConstArrayView<int32> Candidates,
		const FArcRecipeIngredient& Ingredient,
		TArray<FSlotDraw>& OutDraws,
		float& OutStep)
	{
		TArray<FArcCraftSlotCandidate> SlotCandidates;
		SlotCandidates.Reserve(Candidates.Num());
		for (const int32 CandidateIndex : Candidates)
		{
			FArcCraftSlotCandidate& SlotCandidate = SlotCandidates.AddDefaulted_GetRef();
			SlotCandidate.DefinitionId = Items[CandidateIndex].GetItemDefinitionId();
			SlotCandidate.InstanceTags = ArcMassItems::GetSpecInstanceTags(Items[CandidateIndex]);
			SlotCandidate.Step = Ingredient.GetItemRarityStep(Items[CandidateIndex]);
			SlotCandidate.Amount = static_cast<int32>(Items[CandidateIndex].Amount);
		}

		TArray<FArcCraftSlotTake> Takes;
		if (!ArcCraftRarity::ComputeSlotDraw(SlotCandidates, Ingredient.Amount, Takes, OutStep))
		{
			return false;
		}

		OutDraws.Reset();
		for (const FArcCraftSlotTake& Take : Takes)
		{
			OutDraws.Add({Candidates[Take.CandidateIndex], Take.Amount});
		}
		return true;
	}
} // namespace ArcCraftItemSourceLocal

bool FArcCraftItemSource::MatchAndConsumeFromSpecs(
	TArray<FArcItemSpec>& Items,
	const FArcRecipeDefinitionData& Recipe,
	bool bConsume,
	TArray<FArcItemSpec>* OutMatchedItems,
	TArray<float>* OutSlotRaritySteps,
	TArray<bool>* OutSlotFilled)
{
	const int32 NumSlots = Recipe.Ingredients.Num();

	TArray<int32> SlotOrder;
	SlotOrder.Reserve(NumSlots);
	for (int32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
	{
		const FArcRecipeIngredient* Ingredient = Recipe.GetIngredientBase(SlotIdx);
		if (!Ingredient)
		{
			return false;
		}
		if (!Ingredient->bOptional)
		{
			SlotOrder.Add(SlotIdx);
		}
	}
	for (int32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
	{
		if (Recipe.IsIngredientOptional(SlotIdx))
		{
			SlotOrder.Add(SlotIdx);
		}
	}

	TSet<int32> UsedIndices;
	TArray<FArcItemSpec> MatchedIngredients;
	TArray<float> SlotRaritySteps;
	TArray<bool> SlotFilled;
	MatchedIngredients.SetNum(NumSlots);
	SlotRaritySteps.Init(1.0f, NumSlots);
	SlotFilled.Init(false, NumSlots);

	TArray<TPair<int32, int32>> ItemsToConsume;

	for (const int32 SlotIdx : SlotOrder)
	{
		const FArcRecipeIngredient* Ingredient = Recipe.GetIngredientBase(SlotIdx);

		TArray<int32> Candidates;
		for (int32 ItemIdx = 0; ItemIdx < Items.Num(); ++ItemIdx)
		{
			if (UsedIndices.Contains(ItemIdx))
			{
				continue;
			}

			const FArcItemSpec& Spec = Items[ItemIdx];
			if (!Spec.GetItemDefinitionId().IsValid())
			{
				continue;
			}

			if (!Ingredient->DoesItemSatisfy(Spec))
			{
				continue;
			}

			Candidates.Add(ItemIdx);
		}

		TArray<ArcCraftItemSourceLocal::FSlotDraw> Draws;
		float SlotStep = 0.0f;
		if (!ArcCraftItemSourceLocal::DrawSlotFromCandidates(Items, Candidates, *Ingredient, Draws, SlotStep))
		{
			if (Ingredient->bOptional)
			{
				continue;
			}
			return false;
		}

		MatchedIngredients[SlotIdx] = Items[Draws[0].ItemIndex];
		SlotRaritySteps[SlotIdx] = SlotStep;
		SlotFilled[SlotIdx] = true;

		for (const ArcCraftItemSourceLocal::FSlotDraw& Draw : Draws)
		{
			UsedIndices.Add(Draw.ItemIndex);
			if (Ingredient->bConsumeOnCraft && Draw.Amount > 0)
			{
				ItemsToConsume.Add({Draw.ItemIndex, Draw.Amount});
			}
		}
	}

	if (bConsume)
	{
		// Remove consumed amounts from the Items array. Process in reverse index order
		// so that removals don't invalidate earlier indices.
		ItemsToConsume.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B)
		{
			return A.Key > B.Key;
		});

		for (const TPair<int32, int32>& Pair : ItemsToConsume)
		{
			FArcItemSpec& Spec = Items[Pair.Key];
			if (Spec.Amount <= static_cast<uint16>(Pair.Value))
			{
				Items.RemoveAt(Pair.Key);
			}
			else
			{
				Spec.Amount -= static_cast<uint16>(Pair.Value);
			}
		}
	}

	if (OutMatchedItems)
	{
		*OutMatchedItems = MoveTemp(MatchedIngredients);
	}
	if (OutSlotRaritySteps)
	{
		*OutSlotRaritySteps = MoveTemp(SlotRaritySteps);
	}
	if (OutSlotFilled)
	{
		*OutSlotFilled = MoveTemp(SlotFilled);
	}

	return true;
}

// -------------------------------------------------------------------
// ArcCraftStationInputXfer — shared deposit/withdraw helpers
// -------------------------------------------------------------------

namespace ArcCraftStationInputXfer
{
	static bool DepositToStationInput(
		FMassEntityManager& EntityManager,
		FMassEntityHandle StationEntity,
		FMassEntityHandle InstigatorEntity,
		const UScriptStruct* StoreType,
		FArcItemId ItemId,
		int32 Stacks)
	{
		if (!EntityManager.IsEntityActive(StationEntity) || !EntityManager.IsEntityActive(InstigatorEntity))
		{
			return false;
		}
		if (!ItemId.IsValid() || Stacks <= 0)
		{
			return false;
		}
		if (StoreType == nullptr || !StoreType->IsChildOf(FArcMassItemStoreFragment::StaticStruct()))
		{
			return false;
		}

		FArcCraftInputFragment* InputFrag =
			EntityManager.GetFragmentDataPtr<FArcCraftInputFragment>(StationEntity);
		if (InputFrag == nullptr)
		{
			return false;
		}

		const FArcItemData* ItemData = ArcMassItems::FindItemById(EntityManager, InstigatorEntity, StoreType, ItemId);
		if (ItemData == nullptr || static_cast<int32>(ItemData->GetStacks()) < Stacks)
		{
			return false;
		}

		const bool bSplit = Stacks < static_cast<int32>(ItemData->GetStacks());
		FArcItemSpec DepositSpec = ItemData->Spec;
		DepositSpec.Amount = static_cast<uint16>(Stacks);
		DepositSpec.SetItemId(bSplit ? FArcItemId::Generate() : ItemId);

		if (!ArcMassItems::RemoveItem(EntityManager, InstigatorEntity, StoreType, ItemId, Stacks))
		{
			return false;
		}

		InputFrag->InputItems.Add(DepositSpec);
		return true;
	}

	/**
	 * Move up to Stacks of ItemId from StationEntity's FArcCraftInputFragment::InputItems
	 * back into InstigatorEntity's FArcMassItemStoreFragment via a deferred Set command.
	 * Returns true = deferred command queued (NOT "stacks fully moved").
	 * Withdraw partial-caps StacksToWithdraw = min(Stacks, Available) inside the deferred
	 * lambda — a request exceeding available stacks silently transfers only what exists.
	 */
	static bool WithdrawFromStationInput(
		FMassEntityManager& EntityManager,
		FMassEntityHandle StationEntity,
		FMassEntityHandle InstigatorEntity,
		FArcItemId ItemId,
		int32 Stacks)
	{
		// Synchronous pre-validation: both entities must be active.
		if (!EntityManager.IsEntityActive(StationEntity) || !EntityManager.IsEntityActive(InstigatorEntity))
		{
			return false;
		}
		if (!ItemId.IsValid() || Stacks <= 0)
		{
			return false;
		}

		// Push a deferred Set; the actual mutation happens on the game thread.
		EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
			[StationEntity, InstigatorEntity, ItemId, Stacks](FMassEntityManager& Mgr)
			{
				if (!Mgr.IsEntityActive(StationEntity) || !Mgr.IsEntityActive(InstigatorEntity))
				{
					return;
				}

				FArcCraftInputFragment* InputFrag =
					Mgr.GetFragmentDataPtr<FArcCraftInputFragment>(StationEntity);
				if (InputFrag == nullptr)
				{
					return;
				}

				// Locate the spec in the station input by ItemId.
				const int32 InputIndex = InputFrag->InputItems.IndexOfByKey(ItemId);
				if (InputIndex == INDEX_NONE)
				{
					return;
				}

				FArcItemSpec& InputSpec = InputFrag->InputItems[InputIndex];
				const int32 Available = static_cast<int32>(InputSpec.Amount);
				const int32 StacksToWithdraw = (Stacks >= Available) ? Available : Stacks;

				// Build the withdrawn spec.
				FArcItemSpec WithdrawnSpec = InputSpec;
				WithdrawnSpec.Amount = static_cast<uint16>(StacksToWithdraw);
				WithdrawnSpec.SetItemId(StacksToWithdraw < Available ? FArcItemId::Generate() : ItemId);

				// Add-to-bag FIRST: deliver to the instigator (bag-aware spilling, or a plain store add
				// when bag-less). DeliverSpecToInstigator touches only the instigator's stores, so the
				// InputSpec reference / InputIndex into the station array stay valid below.
				const int32 Placed = ArcCraftStationXfer::DeliverSpecToInstigator(Mgr, InstigatorEntity, WithdrawnSpec);
				if (Placed <= 0)
				{
					// Nothing seated on the instigator — leave the station untouched, nothing lost.
					return;
				}

				// Decrement or remove from station input by the placed count ONLY (never the request) so
				// a partial/full-bag fill leaves the unplaced remainder in the station.
				if (Placed >= Available)
				{
					InputFrag->InputItems.RemoveAt(InputIndex);
				}
				else
				{
					InputSpec.Amount = static_cast<uint16>(Available - Placed);
				}

				if (UWorld* World = Mgr.GetWorld())
				{
					if (UMassSignalSubsystem* Sig = World->GetSubsystem<UMassSignalSubsystem>())
					{
						Sig->SignalEntity(ArcCraftSignals::InputChanged, StationEntity);
					}
				}
			});

		return true;
	}
} // namespace ArcCraftStationInputXfer

// -------------------------------------------------------------------
// FArcCraftItemSource_EntityStore — DepositItem_Mass / WithdrawItem_Mass
// -------------------------------------------------------------------

bool FArcCraftItemSource_EntityStore::DepositItem_Mass(
	FMassEntityManager& EntityManager,
	FMassEntityHandle StationEntity,
	FMassEntityHandle InstigatorEntity,
	const UScriptStruct* StoreType,
	FArcItemId ItemId,
	int32 Stacks) const
{
	return ArcCraftStationInputXfer::DepositToStationInput(EntityManager, StationEntity, InstigatorEntity, StoreType, ItemId, Stacks);
}

bool FArcCraftItemSource_EntityStore::WithdrawItem_Mass(
	FMassEntityManager& EntityManager,
	FMassEntityHandle StationEntity,
	FMassEntityHandle InstigatorEntity,
	FArcItemId ItemId,
	int32 Stacks) const
{
	return ArcCraftStationInputXfer::WithdrawFromStationInput(EntityManager, StationEntity, InstigatorEntity, ItemId, Stacks);
}

