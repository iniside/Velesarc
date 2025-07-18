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
#include "Items/ArcItemId.h"
#include "ArcAddItemToItemSocketCommand.generated.h"

class UArcItemsStoreComponent;

USTRUCT()
struct ARCCORE_API FArcAddItemToItemSocketCommand : public FArcReplicatedCommand
{
	
	GENERATED_BODY()
	
protected:
	UPROPERTY()
	TObjectPtr<UArcItemsStoreComponent> ToComponent = nullptr;
	
	UPROPERTY()
	FArcItemId TargetItem;

	UPROPERTY()
	FArcItemId AttachmentItem;
	
	UPROPERTY()
	FGameplayTag SocketSlot;
	
public:
	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;


	FArcAddItemToItemSocketCommand()
		: ToComponent(nullptr)
	{}
	
	FArcAddItemToItemSocketCommand(UArcItemsStoreComponent* InToComponent
		, const FArcItemId& InTargetItem
		, const FArcItemId& InAttachmentItem
		, const FGameplayTag& InSocketSlot)
		: ToComponent(InToComponent)
		, TargetItem(InTargetItem)
		, AttachmentItem(InAttachmentItem)
		, SocketSlot(InSocketSlot)
	{

	}
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcAddItemToItemSocketCommand::StaticStruct();
	}
	virtual ~FArcAddItemToItemSocketCommand() override = default;
};
