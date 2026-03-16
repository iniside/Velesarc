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

#pragma once

#include "Commands/ArcItemReplicatedCommand.h"
#include "Items/ArcItemId.h"
#include "ArcUseItemCommand.generated.h"

class UArcItemsStoreComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogArcUseItemCommand, Log, All);

/**
 * Triggers the "use" ability configured on an item via FArcItemFragment_UseAbility.
 * Validates the ability is granted, sends a gameplay event with item context.
 * The triggered ability decides what to do (e.g., consume stacks).
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcUseItemCommand : public FArcItemReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcItemsStoreComponent> ItemsStore = nullptr;

	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

public:
	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;
	virtual bool NeedsConfirmation() const override { return true; }
	virtual void CommandConfirmed(bool bSuccess) override;

	FArcUseItemCommand()
		: ItemsStore(nullptr)
	{
	}

	FArcUseItemCommand(UArcItemsStoreComponent* InItemsStore, const FArcItemId& InItemId)
		: ItemsStore(InItemsStore)
		, ItemId(InItemId)
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcUseItemCommand::StaticStruct();
	}

	virtual ~FArcUseItemCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcUseItemCommand>
	: public TStructOpsTypeTraitsBase2<FArcUseItemCommand>
{
	enum { WithCopy = true };
};
