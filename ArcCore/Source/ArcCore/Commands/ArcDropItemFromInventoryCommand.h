/**
 * This file is part of ArcX.
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
#include "CoreMinimal.h"
#include "Items/ArcItemId.h"
#include "ArcDropItemFromInventoryCommand.generated.h"

class UArcItemsStoreComponent;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcDropItemFromInventoryCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<UArcItemsStoreComponent> ItemsStore;

	UPROPERTY()
	FArcItemId Item;

public:
	virtual bool CanSendCommand() const override;

	virtual void PreSendCommand() override;

	virtual bool Execute() override; ;

	FArcDropItemFromInventoryCommand()
		: ItemsStore(nullptr)
	{
	}

	FArcDropItemFromInventoryCommand(UArcItemsStoreComponent* InItemsStore
									 , const FArcItemId& InItem)
		: ItemsStore(InItemsStore)
		, Item(InItem)
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcDropItemFromInventoryCommand::StaticStruct();
	}

	virtual ~FArcDropItemFromInventoryCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcDropItemFromInventoryCommand>
		: public TStructOpsTypeTraitsBase2<FArcDropItemFromInventoryCommand>
{
	enum
	{
		WithCopy = true // Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};
