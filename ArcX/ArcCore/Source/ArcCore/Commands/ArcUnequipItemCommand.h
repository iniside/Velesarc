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

#include "Items/ArcItemId.h"
#include "ArcReplicatedCommand.h"
#include "GameplayTagContainer.h"
#include "ArcUnequipItemCommand.generated.h"

class UArcEquipmentComponent;

/**
 * 
 */
USTRUCT()
struct ARCCORE_API FArcUnequipItemCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()
	
protected:
	UPROPERTY()
	TObjectPtr<UArcEquipmentComponent> EquipmentComponent = nullptr;
	
	UPROPERTY()
	FGameplayTag SlotId;
	
public:
	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;


	FArcUnequipItemCommand()
		: EquipmentComponent(nullptr)
	{}
	
	FArcUnequipItemCommand(UArcEquipmentComponent* InEquipmentComponent
		, const FGameplayTag& InSlotId)
		: EquipmentComponent(InEquipmentComponent)
		, SlotId(InSlotId)
	{

	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcUnequipItemCommand::StaticStruct();
	}
	virtual ~FArcUnequipItemCommand() override = default;
};
