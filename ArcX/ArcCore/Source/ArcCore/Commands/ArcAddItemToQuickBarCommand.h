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

#include "ArcAddItemToQuickBarCommand.generated.h"

class UArcItemsStoreComponent;
class UArcQuickBarComponent;

/**
 * 
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcAddItemToQuickBarCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/**
	 * Quick to which we will try add item
	 */
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcQuickBarComponent> QuickBarComponent;

	/**
	 * Items component which own item we will add to quick bar.
	 */
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcItemsStoreComponent> ItemsStoreComponent;

	/**
	 * Id of item which will be added to quick bar.
	 */
	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

	/**
	 * Id quick bar to which we will add item.
	 */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickBar;

	/** 
	 * Id of QuickSlot to which we will add item.
	 */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickSlot;
	
public:
	FArcAddItemToQuickBarCommand()
		: QuickBarComponent(nullptr)
		, ItemsStoreComponent(nullptr)
		, ItemId()
		, QuickBar()
		, QuickSlot()
	{}

	FArcAddItemToQuickBarCommand(UArcQuickBarComponent* InQuickBarComponent
			, UArcItemsStoreComponent* InItemsStoreComponent
			, const FArcItemId& InItemId
			, const FGameplayTag& InQuickBar
			, const FGameplayTag& InQuickSlot)
		: QuickBarComponent(InQuickBarComponent)
		, ItemsStoreComponent(InItemsStoreComponent)
		, ItemId(InItemId)
		, QuickBar(InQuickBar)
		, QuickSlot(InQuickSlot)
	{}
	
	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcAddItemToQuickBarCommand::StaticStruct();
	}
	virtual ~FArcAddItemToQuickBarCommand() override {}
};


/**
 *
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcAddItemToQuickBarNoItemSlotCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/**
	 * Quick to which we will try add item
	 */
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcQuickBarComponent> QuickBarComponent;

	/**
	 * Items component which own item we will add to quick bar.
	 */
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcItemsStoreComponent> SourceItemsStoreComponent;
	/**
	 * Id of item which will be added to quick bar.
	 */
	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

	/**
	 * Id quick bar to which we will add item.
	 */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickBar;

	/**
	 * Id of QuickSlot to which we will add item.
	 */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickSlot;

public:
	FArcAddItemToQuickBarNoItemSlotCommand()
		: QuickBarComponent(nullptr)
		, ItemId()
		, QuickBar()
		, QuickSlot()
	{
	}

	FArcAddItemToQuickBarNoItemSlotCommand(UArcQuickBarComponent* InQuickBarComponent
		, UArcItemsStoreComponent* InSourceItemsStoreComponent
		, const FArcItemId& InItemId
		, const FGameplayTag& InQuickBar
		, const FGameplayTag& InQuickSlot)
		: QuickBarComponent(InQuickBarComponent)
		, SourceItemsStoreComponent(InSourceItemsStoreComponent)
		, ItemId(InItemId)
		, QuickBar(InQuickBar)
		, QuickSlot(InQuickSlot)
	{
	}

	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcAddItemToQuickBarNoItemSlotCommand::StaticStruct();
	}
	virtual ~FArcAddItemToQuickBarNoItemSlotCommand() override {}
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAddNewItemToQuickBarCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/**
	 * Quick to which we will try add item
	 */
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcQuickBarComponent> QuickBarComponent;

	/**
	 * Items component which own item we will add to quick bar.
	 */
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcItemsStoreComponent> ItemsStoreComponent;

	/**
	 * Id of item which will be added to quick bar.
	 */
	UPROPERTY(BlueprintReadWrite)
	FPrimaryAssetId ItemDefinitionId;

	/**
	 * Id quick bar to which we will add item.
	 */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickBar;

	/** 
	 * Id of QuickSlot to which we will add item.
	 */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickSlot;
	
public:
	FArcAddNewItemToQuickBarCommand()
		: QuickBarComponent(nullptr)
		, ItemsStoreComponent(nullptr)
		, ItemDefinitionId()
		, QuickBar()
		, QuickSlot()
	{}

	FArcAddNewItemToQuickBarCommand(UArcQuickBarComponent* InQuickBarComponent
			, UArcItemsStoreComponent* InItemsStoreComponent
			, const FPrimaryAssetId& InItemId
			, const FGameplayTag& InQuickBar
			, const FGameplayTag& InQuickSlot)
		: QuickBarComponent(InQuickBarComponent)
		, ItemsStoreComponent(InItemsStoreComponent)
		, ItemDefinitionId(InItemId)
		, QuickBar(InQuickBar)
		, QuickSlot(InQuickSlot)
	{}
	
	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcAddNewItemToQuickBarCommand::StaticStruct();
	}
	virtual ~FArcAddNewItemToQuickBarCommand() override {}
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcRemoveItemFromQuickSlotCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/**
	 * Quick to which we will try add item
	 */
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcQuickBarComponent> QuickBarComponent;

	/**
	 * Id quick bar to which we will add item.
	 */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickBar;

	/**
	 * Id of QuickSlot to which we will add item.
	 */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickSlot;

	UPROPERTY(BlueprintReadWrite)
	bool bRemoveFromItemSlot = false;
	
public:
	FArcRemoveItemFromQuickSlotCommand()
		: QuickBarComponent(nullptr)
		, QuickBar()
		, QuickSlot()
		, bRemoveFromItemSlot(false)
	{
	}

	FArcRemoveItemFromQuickSlotCommand(UArcQuickBarComponent* InQuickBarComponent
		, const FGameplayTag& InQuickBar
		, const FGameplayTag& InQuickSlot
		, bool bInRemoveFromItemSlot = false)
		: QuickBarComponent(InQuickBarComponent)
		, QuickBar(InQuickBar)
		, QuickSlot(InQuickSlot)
		, bRemoveFromItemSlot(bInRemoveFromItemSlot)
	{
	}

	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcRemoveItemFromQuickSlotCommand::StaticStruct();
	}
	virtual ~FArcRemoveItemFromQuickSlotCommand() override {}
};
