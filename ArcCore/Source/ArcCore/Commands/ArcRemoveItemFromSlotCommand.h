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

#include "ArcReplicatedCommand.h"

#include "GameplayTagContainer.h"
#include "ArcRemoveItemFromSlotCommand.generated.h"

class UArcItemsStoreComponent;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcRemoveItemFromSlotCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<UArcItemsStoreComponent> ItemsStore = nullptr;

	UPROPERTY()
	FGameplayTag SlotId;

public:
	virtual bool CanSendCommand() const override;

	virtual void PreSendCommand() override;

	virtual bool Execute() override; ;

	FArcRemoveItemFromSlotCommand()
		: ItemsStore(nullptr)
	{
	}

	FArcRemoveItemFromSlotCommand(UArcItemsStoreComponent* InItemsStore
								  , const FGameplayTag& InSlotId)
		: ItemsStore(InItemsStore)
		, SlotId(InSlotId)
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcRemoveItemFromSlotCommand::StaticStruct();
	}

	virtual ~FArcRemoveItemFromSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcRemoveItemFromSlotCommand>
		: public TStructOpsTypeTraitsBase2<FArcRemoveItemFromSlotCommand>
{
	enum
	{
		WithCopy = true // Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};
