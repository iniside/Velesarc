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

#include "Commands/ArcReplicatedCommand.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "ArcEquipItemCommand.generated.h"

class UArcItemsStoreComponent;
class UArcEquipmentComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogArcEquipItemCommand, Log, All);

/**
 * This will equipt item from inventory to given slot. If slot is taken it will remove existing item from slot.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcEquipItemCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcItemsStoreComponent> ItemsStore = nullptr;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcEquipmentComponent> EquipmentComponent = nullptr;
	
	UPROPERTY(BlueprintReadWrite)
	FArcItemId Item;
	
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag SlotId;
	
public:
	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;


	FArcEquipItemCommand()
		: ItemsStore(nullptr)
	{}
	
	FArcEquipItemCommand(UArcItemsStoreComponent* InItemsStore
		, UArcEquipmentComponent* InEquipmentComponent
		, const FArcItemId& InItem
		, const FGameplayTag& InSlotId)
		: ItemsStore(InItemsStore)
		, EquipmentComponent(InEquipmentComponent)
		, Item(InItem)
		, SlotId(InSlotId)
	{

	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcEquipItemCommand::StaticStruct();
	}
	virtual ~FArcEquipItemCommand() override = default;
};

/**
 * This will equip new item to given slot. If the slot is taken, it will remove the existing item from the slot.
 * Optionally it can remove existing item from Items Store.
 * This command is used when you want to equip item not in inventory yet.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcEquipNewItemCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcItemsStoreComponent> ItemsStore = nullptr;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcEquipmentComponent> EquipmentComponent = nullptr;
	
	UPROPERTY(BlueprintReadWrite)
	FPrimaryAssetId ItemDefinitionId;
	
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag SlotId;

	// If true, will remove existing item from Items Store if slot is taken.
	UPROPERTY(BlueprintReadWrite)
	bool bRemoveExistingItemFromStore = true; 
	
public:
	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;

	FArcEquipNewItemCommand()
		: ItemsStore(nullptr)
	{}
	
	FArcEquipNewItemCommand(UArcItemsStoreComponent* InItemsStore
		, UArcEquipmentComponent* InEquipmentComponent
		, const FPrimaryAssetId& InItem
		, const FGameplayTag& InSlotId
		, const bool bInRemoveExistingItemFromStore)
		: ItemsStore(InItemsStore)
		, EquipmentComponent(InEquipmentComponent)
		, ItemDefinitionId(InItem)
		, SlotId(InSlotId)
		, bRemoveExistingItemFromStore(bInRemoveExistingItemFromStore)
	{

	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcEquipNewItemCommand::StaticStruct();
	}
	virtual ~FArcEquipNewItemCommand() override = default;
};