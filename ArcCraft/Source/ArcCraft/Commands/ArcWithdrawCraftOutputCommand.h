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
#include "Items/ArcItemId.h"
#include "Mass/EntityHandle.h"
#include "NetSerializers/ArcMassEntityHandleIrisSerializer.h"
#include "ArcWithdrawCraftOutputCommand.generated.h"

/**
 * Replicated command sent from client to server to withdraw one finished-output line from a
 * craft station's FArcCraftOutputFragment back into the instigator's bag.
 *
 * Bag-aware: delivery spills across bag slots via ArcCraftStationXfer::DeliverSpecToInstigator and
 * the station's OutputItems line is decremented by the placed count ONLY (add-first, decrement-by-
 * placed atomicity) so a full bag leaves the unplaced remainder in the station — nothing lost.
 *
 * Direct fragment access — no FArcCraftDeliveryConfigFragment/FArcCraftItemSource indirection. Per-
 * policy correctness is free: non-_EntityStore stations never populate OutputItems → find fails →
 * returns false.
 *
 * Self-contained: no world/PC/actor pointers stored; world resolved from WorldContext in Execute().
 */
USTRUCT(meta = (Tooltip = "Replicated command sent from client to server to withdraw one finished-output line from a craft station\'s FArcCraftOutputFragment back into the instigator\'s bag."))
struct ARCCRAFT_API FArcWithdrawCraftOutputCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Mass entity of the crafting station to withdraw output from. */
	UPROPERTY()
	FMassEntityHandle StationEntity;

	/** Mass entity of the player (instigator) receiving the withdrawn output. */
	UPROPERTY()
	FMassEntityHandle InstigatorEntity;

	/** Id of the output line to withdraw from the station's output fragment. */
	UPROPERTY()
	FArcItemId ItemId;

	/** Number of stacks to withdraw. Must be >= 1. */
	UPROPERTY()
	int32 Stacks = 1;

	FArcWithdrawCraftOutputCommand() = default;

	FArcWithdrawCraftOutputCommand(
		FMassEntityHandle InStationEntity,
		FMassEntityHandle InInstigatorEntity,
		FArcItemId InItemId,
		int32 InStacks)
		: StationEntity(InStationEntity)
		, InstigatorEntity(InInstigatorEntity)
		, ItemId(InItemId)
		, Stacks(InStacks)
	{
	}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcWithdrawCraftOutputCommand::StaticStruct();
	}

	virtual ~FArcWithdrawCraftOutputCommand() override = default;

private:
	UWorld* ResolveWorld(UObject* WorldContext) const
	{
		return WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	}
};

template <>
struct TStructOpsTypeTraits<FArcWithdrawCraftOutputCommand>
	: public TStructOpsTypeTraitsBase2<FArcWithdrawCraftOutputCommand>
{
	enum
	{
		WithCopy = true
	};
};
