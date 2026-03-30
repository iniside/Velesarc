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

#include "MassEntityHandle.h"
#include "Commands/ArcReplicatedCommand.h"
#include "MassEntityTypes.h"
#include "UObject/ObjectPtr.h"

#include "ArcLootItemCommand.generated.h"

class UArcItemsStoreComponent;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcLootItemCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcItemsStoreComponent> TargetItemsStore;

	UPROPERTY(BlueprintReadWrite)
	FMassEntityHandle LootEntity;

public:
	FArcLootItemCommand()
		: TargetItemsStore(nullptr)
		, LootEntity()
	{
	}

	FArcLootItemCommand(UArcItemsStoreComponent* InTargetItemsStore, FMassEntityHandle InLootEntity)
		: TargetItemsStore(InTargetItemsStore)
		, LootEntity(InLootEntity)
	{
	}

	virtual bool CanSendCommand() const override;
	virtual bool Execute() override;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcLootItemCommand::StaticStruct();
	}
	virtual ~FArcLootItemCommand() override {}
};

template <>
struct TStructOpsTypeTraits<FArcLootItemCommand>
	: public TStructOpsTypeTraitsBase2<FArcLootItemCommand>
{
	enum { WithCopy = true };
};
