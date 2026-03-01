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

#include "Commands/ArcItemReplicatedCommand.h"
#include "Items/ArcItemId.h"

#include "ArcDepositItemToCraftStationCommand.generated.h"

class UArcItemsStoreComponent;
class UArcCraftStationComponent;

/**
 * Replicated command: transfer an item from a UArcItemsStoreComponent
 * into a UArcCraftStationComponent via DepositItem().
 *
 * The station internally resolves whether to write to the entity fragment
 * or actor-side store based on its configured FArcCraftItemSource.
 *
 * Stacks == 0 means transfer all stacks of the item.
 */
USTRUCT()
struct ARCCRAFT_API FArcDepositItemToCraftStationCommand : public FArcItemReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<UArcItemsStoreComponent> SourceStore;

	UPROPERTY()
	FArcItemId ItemId;

	UPROPERTY()
	TObjectPtr<UArcCraftStationComponent> CraftStation;

	/** Number of stacks to transfer. 0 = all stacks. */
	UPROPERTY()
	int32 Stacks = 0;

public:
	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;
	virtual bool NeedsConfirmation() const override { return true; }
	virtual void CommandConfirmed(bool bSuccess) override;

	FArcDepositItemToCraftStationCommand()
		: SourceStore(nullptr)
		, ItemId()
		, CraftStation(nullptr)
		, Stacks(0)
	{
	}

	FArcDepositItemToCraftStationCommand(
		UArcItemsStoreComponent* InSourceStore,
		const FArcItemId& InItemId,
		UArcCraftStationComponent* InCraftStation,
		int32 InStacks = 0)
		: SourceStore(InSourceStore)
		, ItemId(InItemId)
		, CraftStation(InCraftStation)
		, Stacks(InStacks)
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcDepositItemToCraftStationCommand::StaticStruct();
	}

	virtual ~FArcDepositItemToCraftStationCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcDepositItemToCraftStationCommand>
	: public TStructOpsTypeTraitsBase2<FArcDepositItemToCraftStationCommand>
{
	enum
	{
		WithCopy = true
	};
};
