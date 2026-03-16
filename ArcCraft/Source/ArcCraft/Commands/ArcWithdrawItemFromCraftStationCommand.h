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

#include "Commands/ArcReplicatedCommand.h"

#include "ArcWithdrawItemFromCraftStationCommand.generated.h"

class UArcItemsStoreComponent;
class UArcCraftStationComponent;

/**
 * Replicated command: withdraw an item from a UArcCraftStationComponent
 * and place it into a target UArcItemsStoreComponent.
 *
 * The station resolves whether to read from entity fragments or actor-side
 * stores based on its configured FArcCraftItemSource / FArcCraftOutputDelivery.
 *
 * bWithdrawOutput == false => withdraw from input ingredients
 * bWithdrawOutput == true  => withdraw from crafted output
 *
 * Stacks == 0 means withdraw all stacks of the item at the given index.
 */
USTRUCT()
struct ARCCRAFT_API FArcWithdrawItemFromCraftStationCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<UArcCraftStationComponent> CraftStation;

	UPROPERTY()
	TObjectPtr<UArcItemsStoreComponent> TargetStore;

	/** Index of the item to withdraw in the station's items array. */
	UPROPERTY()
	int32 ItemIndex = INDEX_NONE;

	/** Number of stacks to withdraw. 0 = all stacks. */
	UPROPERTY()
	int32 Stacks = 0;

	/** If true, withdraw from output (crafted items). If false, withdraw from input (ingredients). */
	UPROPERTY()
	bool bWithdrawOutput = false;

public:
	virtual bool CanSendCommand() const override;
	virtual bool Execute() override;

	FArcWithdrawItemFromCraftStationCommand()
		: CraftStation(nullptr)
		, TargetStore(nullptr)
		, ItemIndex(INDEX_NONE)
		, Stacks(0)
		, bWithdrawOutput(false)
	{
	}

	FArcWithdrawItemFromCraftStationCommand(
		UArcCraftStationComponent* InCraftStation,
		UArcItemsStoreComponent* InTargetStore,
		int32 InItemIndex,
		int32 InStacks,
		bool bInWithdrawOutput)
		: CraftStation(InCraftStation)
		, TargetStore(InTargetStore)
		, ItemIndex(InItemIndex)
		, Stacks(InStacks)
		, bWithdrawOutput(bInWithdrawOutput)
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcWithdrawItemFromCraftStationCommand::StaticStruct();
	}

	virtual ~FArcWithdrawItemFromCraftStationCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcWithdrawItemFromCraftStationCommand>
	: public TStructOpsTypeTraitsBase2<FArcWithdrawItemFromCraftStationCommand>
{
	enum
	{
		WithCopy = true
	};
};
