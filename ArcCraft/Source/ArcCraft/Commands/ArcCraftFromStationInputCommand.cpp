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

#include "ArcCraft/Commands/ArcCraftFromStationInputCommand.h"

#include "ArcCraft/Commands/ArcCraftAuth.h"
#include "Engine/World.h"
#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftQueueChangedSignal.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"
#include "ArcCraft/Mass/ArcCraftDeliveryConfigFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityView.h"
#include "Operations/ArcMassItemOperations.h"

namespace ArcCraftFromStationInputCommandLocal
{
	DEFINE_LOG_CATEGORY_STATIC(LogArcCraftFromStationInputCommand, Log, All);

	void PostSignal(FMassEntityManager& Mgr, FName Signal, FMassEntityHandle StationEntity)
	{
		if (UWorld* LambdaWorld = Mgr.GetWorld())
		{
			if (UMassSignalSubsystem* Sig = LambdaWorld->GetSubsystem<UMassSignalSubsystem>())
			{
				Sig->SignalEntity(Signal, StationEntity);
			}
		}
	}

	const FArcCraftItemSource* FindIngredientSource(FMassEntityManager& Mgr, FMassEntityHandle Station)
	{
		const FMassEntityView StationView(Mgr, Station);
		const FArcCraftDeliveryConfigFragment* DeliveryCfg =
			StationView.GetConstSharedFragmentDataPtr<FArcCraftDeliveryConfigFragment>();
		return DeliveryCfg != nullptr ? DeliveryCfg->IngredientSource.GetPtr<FArcCraftItemSource>() : nullptr;
	}

	bool Reject(const TCHAR* Reason)
	{
		UE_LOGF(LogArcCraftFromStationInputCommand, Warning, "FArcCraftFromStationInputCommand sources rejected: %ls", Reason);
		return false;
	}
} // namespace ArcCraftFromStationInputCommandLocal

// ---------------------------------------------------------------------------
// ArcCraftFromStationInput::ValidateSources / MoveSourcesToInput
// ---------------------------------------------------------------------------

bool ArcCraftFromStationInput::ValidateSources(
	FMassEntityManager& Mgr,
	FMassEntityHandle Station,
	FMassEntityHandle Instigator,
	TConstArrayView<FArcCraftInputSourceGroup> Sources)
{
	using namespace ArcCraftFromStationInputCommandLocal;

	if (Sources.IsEmpty())
	{
		return true;
	}

	if (!Mgr.IsEntityActive(Station) || !Mgr.IsEntityActive(Instigator))
	{
		return Reject(TEXT("station or instigator is not active"));
	}

	const FArcCraftItemSource* Source = FindIngredientSource(Mgr, Station);
	if (Source == nullptr || !Source->AcceptsDeposits())
	{
		return Reject(TEXT("the station's ingredient source does not accept deposits"));
	}
	if (Mgr.GetFragmentDataPtr<FArcCraftInputFragment>(Station) == nullptr)
	{
		return Reject(TEXT("the station has no input fragment"));
	}

	TMap<TTuple<FMassEntityHandle, const UScriptStruct*, FArcItemId>, int32> RequestedStacks;

	for (const FArcCraftInputSourceGroup& Group : Sources)
	{
		if (Group.OwnerEntity != Instigator)
		{
			return Reject(TEXT("a group's owner entity is not the instigator"));
		}

		const UScriptStruct* StoreType = Group.StoreType;
		if (StoreType == nullptr || !StoreType->IsChildOf(FArcMassItemStoreFragment::StaticStruct()))
		{
			return Reject(TEXT("a group's store type is not an item store"));
		}
		if (!Mgr.GetSparseElementDataForEntity(StoreType, Group.OwnerEntity).IsValid())
		{
			return Reject(TEXT("a group's store is not on the owner entity"));
		}

		for (const FArcCraftInputItem& Row : Group.Items)
		{
			if (Row.Stacks < 1)
			{
				return Reject(TEXT("a row has fewer than 1 stack"));
			}

			const FArcItemData* ItemData = ArcMassItems::FindItemById(Mgr, Group.OwnerEntity, StoreType, Row.ItemId);
			if (ItemData == nullptr)
			{
				return Reject(TEXT("an item is not in its store"));
			}
			if (ItemData->GetSlotId().IsValid() || ItemData->GetOwnerId().IsValid() || !ItemData->GetAttachedItems().IsEmpty())
			{
				return Reject(TEXT("an item is slotted, attached or holds attached items"));
			}

			int32& Requested = RequestedStacks.FindOrAdd(MakeTuple(Group.OwnerEntity, StoreType, Row.ItemId), 0);
			const int32 Live = static_cast<int32>(ItemData->GetStacks());
			if (Row.Stacks > Live - Requested)
			{
				return Reject(TEXT("the requested stacks exceed the stacks held"));
			}
			Requested += Row.Stacks;
		}
	}

	return true;
}

bool ArcCraftFromStationInput::MoveSourcesToInput(
	FMassEntityManager& Mgr,
	FMassEntityHandle Station,
	FMassEntityHandle Instigator,
	TConstArrayView<FArcCraftInputSourceGroup> Sources)
{
	using namespace ArcCraftFromStationInputCommandLocal;

	if (!ValidateSources(Mgr, Station, Instigator, Sources))
	{
		return false;
	}
	if (Sources.IsEmpty())
	{
		return true;
	}

	const FArcCraftItemSource* Source = FindIngredientSource(Mgr, Station);
	check(Source != nullptr);

	using FMoveKey = TTuple<FMassEntityHandle, const UScriptStruct*, FArcItemId>;
	TArray<TTuple<FMoveKey, int32>> Moves;
	for (const FArcCraftInputSourceGroup& Group : Sources)
	{
		for (const FArcCraftInputItem& Row : Group.Items)
		{
			const FMoveKey Key(Group.OwnerEntity, Group.StoreType.Get(), Row.ItemId);
			TTuple<FMoveKey, int32>* Existing = Moves.FindByPredicate([&Key](const TTuple<FMoveKey, int32>& Move)
			{
				return Move.Key == Key;
			});
			if (Existing != nullptr)
			{
				Existing->Value += Row.Stacks;
			}
			else
			{
				Moves.Emplace(Key, Row.Stacks);
			}
		}
	}

	for (const TTuple<FMoveKey, int32>& Move : Moves)
	{
		const bool bMoved = Source->DepositItem_Mass(Mgr, Station, Instigator, Move.Key.Get<1>(), Move.Key.Get<2>(), Move.Value);
		ensureMsgf(bMoved, TEXT("Deposit failed after its validation passed."));
	}

	PostSignal(Mgr, ArcCraftSignals::InputChanged, Station);
	return true;
}

// ---------------------------------------------------------------------------
// ArcCraftFromStationInput::ApplyQueueFromInput — single source of truth
// ---------------------------------------------------------------------------

int32 ArcCraftFromStationInput::ApplyQueueFromInput(
	FMassEntityManager& Mgr,
	FMassEntityHandle Station,
	FMassEntityHandle Instigator,
	const FPrimaryAssetId& RecipeId,
	const FArcRecipeDefinitionData& Recipe,
	int32 Amount)
{
	if (!RecipeId.IsValid() || Amount < 1)
	{
		return 0;
	}

	// Re-validate entity live (Instigator is stamped on the entry, not consumed here).
	if (!Mgr.IsEntityActive(Station))
	{
		return 0;
	}

	FArcCraftInputFragment* InputFrag =
		Mgr.GetFragmentDataPtr<FArcCraftInputFragment>(Station);
	if (InputFrag == nullptr)
	{
		return 0;
	}

	FArcCraftQueueFragment* QueueFrag =
		Mgr.GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
	if (QueueFrag == nullptr)
	{
		return 0;
	}

	// ----------------------------------------------------------------
	// Fundable-from-input — behaviour-equivalent to activation consume.
	// Repeatedly match-and-consume the recipe off a scratch copy of the
	// deposited input; each successful pass funds one craft. Honors
	// bConsumeOnCraft via MatchAndConsumeFromSpecs. A 0-ingredient recipe
	// consumes nothing each pass and still returns true → naturally reaches
	// Effective == Amount; the Effective < Amount guard bounds the loop.
	// ----------------------------------------------------------------
	TArray<FArcItemSpec> Scratch = InputFrag->InputItems;
	int32 Effective = 0;
	while (Effective < Amount
		&& FArcCraftItemSource::MatchAndConsumeFromSpecs(Scratch, Recipe, /*bConsume=*/true))
	{
		++Effective;
	}

	if (Effective == 0)
	{
		// Cannot fund even one craft — bail without side-effects (no stuck Pending entry).
		return 0;
	}

	// ----------------------------------------------------------------
	// Coalesce into a matching Pending tail, else append.
	// ----------------------------------------------------------------
	bool bCoalesced = false;
	if (!QueueFrag->Entries.IsEmpty())
	{
		FArcCraftQueueEntry& Tail = QueueFrag->Entries.Last();
		if (Tail.State == EArcCraftQueueEntryState::Pending
			&& Tail.RecipeId == RecipeId
			&& Tail.InstigatorEntity == Instigator)
		{
			Tail.Amount += Effective;
			bCoalesced = true;
		}
	}

	if (!bCoalesced)
	{
		FArcCraftQueueEntry NewEntry;
		NewEntry.EntryId          = FGuid::NewGuid();
		NewEntry.RecipeId         = RecipeId;
		NewEntry.Amount           = Effective;
		NewEntry.InstigatorEntity = Instigator;
		NewEntry.State            = EArcCraftQueueEntryState::Pending;
		QueueFrag->Entries.Add(NewEntry);
	}

	ArcCraftFromStationInputCommandLocal::PostSignal(Mgr, ArcCraftSignals::QueueChanged, Station);

	return Effective;
}

// ---------------------------------------------------------------------------
// CanSendCommand
// ---------------------------------------------------------------------------

bool FArcCraftFromStationInputCommand::CanSendCommand() const
{
	if (!RecipeId.IsValid()
		|| !StationEntity.IsSet()
		|| !InstigatorEntity.IsSet()
		|| Amount < 1)
	{
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------

bool FArcCraftFromStationInputCommand::Execute(UObject* WorldContext)
{
	UWorld* World = ResolveWorld(WorldContext);
	if (World == nullptr)
	{
		UE_LOGF(ArcCraftFromStationInputCommandLocal::LogArcCraftFromStationInputCommand, Warning,"FArcCraftFromStationInputCommand::Execute — WorldContext resolved to null world.");
		return false;
	}

	UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
	if (MassSub == nullptr)
	{
		UE_LOGF(ArcCraftFromStationInputCommandLocal::LogArcCraftFromStationInputCommand, Warning,"FArcCraftFromStationInputCommand::Execute — UMassEntitySubsystem not found.");
		return false;
	}

	FMassEntityManager& EM = MassSub->GetMutableEntityManager();

	if (!EM.IsEntityActive(StationEntity))
	{
		UE_LOGF(ArcCraftFromStationInputCommandLocal::LogArcCraftFromStationInputCommand, Warning,"FArcCraftFromStationInputCommand::Execute — StationEntity is not active.");
		return false;
	}
	if (!EM.IsEntityActive(InstigatorEntity))
	{
		UE_LOGF(ArcCraftFromStationInputCommandLocal::LogArcCraftFromStationInputCommand, Warning,"FArcCraftFromStationInputCommand::Execute — InstigatorEntity is not active.");
		return false;
	}

	// -----------------------------------------------------------------------
	// 1. Resolve the recipe.
	// -----------------------------------------------------------------------
	const FArcRecipeDefinitionData* Recipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId.AssetId);
	if (Recipe == nullptr)
	{
		UE_LOGF(ArcCraftFromStationInputCommandLocal::LogArcCraftFromStationInputCommand, Warning,"FArcCraftFromStationInputCommand::Execute — Recipe '%ls' could not be resolved/loaded.",
			*RecipeId.ToString());
		return false;
	}

	// -----------------------------------------------------------------------
	// 2. Auth gate: station-tag + pinned/tier + queue capacity.
	// -----------------------------------------------------------------------
	FText AuthReason;
	if (!ArcCraftAuth::CanQueue(EM, StationEntity, RecipeId.AssetId, *Recipe, &AuthReason))
	{
		UE_LOGF(ArcCraftFromStationInputCommandLocal::LogArcCraftFromStationInputCommand, Warning,"FArcCraftFromStationInputCommand::Execute — CanQueue failed: %ls", *AuthReason.ToString());
		return false;
	}

	// -----------------------------------------------------------------------
	// 3. Sources gate, read-only; the deferred lambda repeats it before moving.
	// -----------------------------------------------------------------------
	if (!ArcCraftFromStationInput::ValidateSources(EM, StationEntity, InstigatorEntity, Sources))
	{
		return false;
	}

	// -----------------------------------------------------------------------
	// 4. Push the move-and-queue deferred lambda.
	//    All entity mutation happens inside the deferred Set — no nested Defer().
	// -----------------------------------------------------------------------
	const FMassEntityHandle CapturedStation      = StationEntity;
	const FMassEntityHandle CapturedInstigator   = InstigatorEntity;
	const int32             CapturedAmount       = Amount;
	const FPrimaryAssetId   CapturedRecipeId     = RecipeId.AssetId;

	EM.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[CapturedStation, CapturedInstigator, CapturedAmount, CapturedRecipeId, CapturedSources = Sources]
		(FMassEntityManager& Mgr)
		{
			const FArcRecipeDefinitionData* DeferredRecipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(CapturedRecipeId);
			if (DeferredRecipe == nullptr)
			{
				return;
			}
			if (!ArcCraftFromStationInput::MoveSourcesToInput(Mgr, CapturedStation, CapturedInstigator, CapturedSources))
			{
				return;
			}
			ArcCraftFromStationInput::ApplyQueueFromInput(
				Mgr, CapturedStation, CapturedInstigator, CapturedRecipeId, *DeferredRecipe, CapturedAmount);
		});

	return true;
}
