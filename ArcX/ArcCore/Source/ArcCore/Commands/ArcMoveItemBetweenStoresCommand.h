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


#include "GameplayTagContainer.h"
#include "ArcReplicatedCommand.h"
#include "Items/ArcItemId.h"

#include "ArcMoveItemBetweenStoresCommand.generated.h"

class UArcItemsStoreComponent;

/**
 * 
 */
USTRUCT()
struct ARCCORE_API FArcMoveItemBetweenStoresCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()
protected:
	UPROPERTY()
	TObjectPtr<class UArcItemsStoreComponent> SourceStore;
	
	UPROPERTY()
	TObjectPtr<class UArcItemsStoreComponent> TargetStore;
	
	UPROPERTY()
	FArcItemId ItemId;

	UPROPERTY()
	int32 Stacks = 1;
	
public:
	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;

	FArcMoveItemBetweenStoresCommand()
		: SourceStore(nullptr)
		, TargetStore(nullptr)
		, ItemId()
		, Stacks(1)
	{}
	
	FArcMoveItemBetweenStoresCommand(UArcItemsStoreComponent* InSourceStore
									, UArcItemsStoreComponent* InTargetStore
									, const FArcItemId& InItemId
									, int32 InStacks = 1)
		: SourceStore(InSourceStore)
		, TargetStore(InTargetStore)
		, ItemId(InItemId)
		, Stacks(InStacks)
	{

	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMoveItemBetweenStoresCommand::StaticStruct();
	}
	
	virtual ~FArcMoveItemBetweenStoresCommand() override = default;
};
