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
#include "ArcTransferItemFromStationCommand.generated.h"

/**
 * Replicated command sent from client to server to withdraw an item from a craft
 * station's FArcCraftInputFragment back into the instigator's Mass item store.
 *
 * The station's configured FArcCraftItemSource routes the operation — only
 * FArcCraftItemSource_EntityStore implements the withdrawal; other sources return false.
 *
 * Self-contained: no world/PC/actor pointers stored; world resolved from WorldContext
 * in Execute(). The deferred lambda re-validates atomically before mutating.
 */
USTRUCT(meta = (Tooltip = "Replicated command sent from client to server to withdraw an item from a craft station\'s FArcCraftInputFragment back into the instigator\'s Mass item store."))
struct ARCCRAFT_API FArcTransferItemFromStationCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Mass entity of the crafting station to withdraw from. */
	UPROPERTY()
	FMassEntityHandle StationEntity;

	/** Mass entity of the player (instigator) receiving the withdrawn item. */
	UPROPERTY()
	FMassEntityHandle InstigatorEntity;

	/** Id of the item to withdraw from the station's input fragment. */
	UPROPERTY()
	FArcItemId ItemId;

	/** Number of stacks to withdraw. Must be >= 1. */
	UPROPERTY()
	int32 Stacks = 1;

	FArcTransferItemFromStationCommand() = default;

	FArcTransferItemFromStationCommand(
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
		return FArcTransferItemFromStationCommand::StaticStruct();
	}

	virtual ~FArcTransferItemFromStationCommand() override = default;

private:
	UWorld* ResolveWorld(UObject* WorldContext) const
	{
		return WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	}
};

template <>
struct TStructOpsTypeTraits<FArcTransferItemFromStationCommand>
	: public TStructOpsTypeTraitsBase2<FArcTransferItemFromStationCommand>
{
	enum
	{
		WithCopy = true
	};
};
