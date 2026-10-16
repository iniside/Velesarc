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
#include "Commands/ArcReplicatedCommand.h"
#include "ArcNamedPrimaryAssetId.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityHandle.h"
#include "NetSerializers/ArcMassEntityHandleIrisSerializer.h"
#include "ArcCraftFromStationInputCommand.generated.h"

struct FMassEntityManager;
struct FArcRecipeDefinitionData;

USTRUCT()
struct ARCCRAFT_API FArcCraftInputItem
{
	GENERATED_BODY()

	UPROPERTY()
	FArcItemId ItemId;

	UPROPERTY()
	int32 Stacks = 0;
};

USTRUCT()
struct ARCCRAFT_API FArcCraftInputSourceGroup
{
	GENERATED_BODY()

	UPROPERTY()
	FMassEntityHandle OwnerEntity;

	UPROPERTY()
	TObjectPtr<UScriptStruct> StoreType = nullptr;

	UPROPERTY()
	TArray<FArcCraftInputItem> Items;
};

namespace ArcCraftFromStationInput
{
	/** Read-only. All-or-nothing: false when any group or row of Sources cannot be moved into the station input. */
	ARCCRAFT_API bool ValidateSources(
		FMassEntityManager& Mgr,
		FMassEntityHandle Station,
		FMassEntityHandle Instigator,
		TConstArrayView<FArcCraftInputSourceGroup> Sources);

	/** Runs ValidateSources, then deposits through the station's ingredient source, one deposit per (owner, store type, item id) with the rows' stacks summed. False = nothing moved. */
	ARCCRAFT_API bool MoveSourcesToInput(
		FMassEntityManager& Mgr,
		FMassEntityHandle Station,
		FMassEntityHandle Instigator,
		TConstArrayView<FArcCraftInputSourceGroup> Sources);

	/**
	 * Direct-mutation queue operation against the station's already-deposited input.
	 * Called from inside a deferred Mass command (in Execute) and directly from tests (on GT).
	 *
	 * Recipe-agnostic: consumes nothing here — it only computes how many crafts the current
	 * FArcCraftInputFragment::InputItems can fund (behaviour-equivalent to activation), clamps
	 * the requested Amount to that fundable count, and appends/coalesces a Pending queue entry.
	 * The actual ingredient consumption happens later, at queue-entry activation in
	 * UArcCraftTickProcessor.
	 *
	 * Takes the recipe record directly — no asset-manager lookup — so in-memory recipe records
	 * used in tests are fully supported. RecipeId is stamped on the queue entry.
	 *
	 * Returns the number of crafts actually queued (0 if the input cannot fund even one craft,
	 * in which case nothing is appended and no signal is posted).
	 */
	ARCCRAFT_API int32 ApplyQueueFromInput(
		FMassEntityManager& Mgr,
		FMassEntityHandle Station,
		FMassEntityHandle Instigator,
		const FPrimaryAssetId& RecipeId,
		const FArcRecipeDefinitionData& Recipe,
		int32 Amount);
} // namespace ArcCraftFromStationInput

/**
 * Moves the Sources rows into the station's FArcCraftInputFragment, then queues the recipe against
 * that input, clamped to what it can fund. A rejected Sources list moves nothing and queues nothing.
 */
USTRUCT(meta = (Tooltip = "Moves the sent items into the station input, then queues a craft clamped to what that input can fund."))
struct ARCCRAFT_API FArcCraftFromStationInputCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Primary asset ID of the recipe to queue. */
	UPROPERTY()
	FArcNamedPrimaryAssetId RecipeId;

	/** Mass entity of the crafting station. Round-tripped via FArcMassEntityHandleNetSerializer. */
	UPROPERTY()
	FMassEntityHandle StationEntity;

	/** Mass entity of the instigator (player). Round-tripped via FArcMassEntityHandleNetSerializer.
	 *  Still required — UArcCraftTickProcessor routes built output to Entry.InstigatorEntity. */
	UPROPERTY()
	FMassEntityHandle InstigatorEntity;

	/** Number of times to craft this recipe. Must be >= 1. The command self-clamps to the
	 *  truly fundable count inside the deferred lambda. */
	UPROPERTY()
	int32 Amount = 1;

	/** Items to move into the station input before queueing, grouped by (owner entity, store type). */
	UPROPERTY()
	TArray<FArcCraftInputSourceGroup> Sources;

	FArcCraftFromStationInputCommand() = default;

	FArcCraftFromStationInputCommand(
		FArcNamedPrimaryAssetId InRecipeId,
		FMassEntityHandle InStationEntity,
		FMassEntityHandle InInstigatorEntity,
		int32 InAmount,
		TArray<FArcCraftInputSourceGroup> InSources = {})
		: RecipeId(InRecipeId)
		, StationEntity(InStationEntity)
		, InstigatorEntity(InInstigatorEntity)
		, Amount(InAmount)
		, Sources(MoveTemp(InSources))
	{
	}

	/**
	 * Client-side sanity check called before sending the RPC.
	 * Does not perform any server-authoritative checks.
	 */
	virtual bool CanSendCommand() const override;

	/**
	 * Server-side execution. Validates the request against the station's configuration,
	 * then defers the queue-from-input operation.
	 */
	virtual bool Execute(UObject* WorldContext) override;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcCraftFromStationInputCommand::StaticStruct();
	}

	virtual ~FArcCraftFromStationInputCommand() override = default;

private:
	UWorld* ResolveWorld(UObject* WorldContext) const
	{
		return WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	}
};

template <>
struct TStructOpsTypeTraits<FArcCraftFromStationInputCommand>
	: public TStructOpsTypeTraitsBase2<FArcCraftFromStationInputCommand>
{
	enum
	{
		WithCopy = true
	};
};
