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
#include "Items/ArcItemSpec.h"
#include "ArcAddItemSpecCommand.generated.h"

class UArcItemsStoreComponent;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAddItemSpecCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<UArcItemsStoreComponent> ItemsStore;

	UPROPERTY()
	FArcItemSpec Item;

public:
	virtual bool CanSendCommand() const override;

	virtual void PreSendCommand() override;

	virtual bool Execute() override;

	FArcAddItemSpecCommand()
		: ItemsStore(nullptr)
	{
	}

	FArcAddItemSpecCommand(UArcItemsStoreComponent* InItemsStore
						   , const FArcItemSpec& InItem)
		: ItemsStore(InItemsStore)
		, Item(InItem)
	{
	}

	virtual ~FArcAddItemSpecCommand() override
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcAddItemSpecCommand::StaticStruct();
	}
};

template <>
struct TStructOpsTypeTraits<FArcAddItemSpecCommand> : public TStructOpsTypeTraitsBase2<FArcAddItemSpecCommand>
{
	enum
	{
		WithCopy = true // Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};
