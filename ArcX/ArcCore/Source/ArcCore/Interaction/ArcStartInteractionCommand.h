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

#include "ArcCoreInteractionCustomData.h"
#include "Commands/ArcReplicatedCommand.h"
#include "ArcStartInteractionCommand.generated.h"

class UArcInteractionReceiverComponent;
/**
 * 
 */
USTRUCT()
struct ARCCORE_API FArcStartInteractionCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UArcInteractionReceiverComponent> ReceiverComponent;

	FArcStartInteractionCommand()
		: ReceiverComponent(nullptr)
	{}

	FArcStartInteractionCommand(UArcInteractionReceiverComponent* InReceiverComponent)
		: ReceiverComponent(InReceiverComponent)
	{}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcStartInteractionCommand::StaticStruct();
	}
	
	virtual bool Execute() override;
};

USTRUCT()
struct ARCCORE_API FArcInteractionChangeBoolStateCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UArcInteractionReceiverComponent> ReceiverComponent;

	UPROPERTY()
	FGuid InteractionId;

	UPROPERTY()
	bool bNewState;
	
	FArcInteractionChangeBoolStateCommand()
		: ReceiverComponent(nullptr)
		, InteractionId()
		, bNewState(false)
	{}

	FArcInteractionChangeBoolStateCommand(UArcInteractionReceiverComponent* InReceiverComponent, const FGuid& InInteractionId, bool bInNewState)
		: ReceiverComponent(InReceiverComponent)
		, InteractionId(InInteractionId)
		, bNewState(bInNewState)
	{}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcInteractionChangeBoolStateCommand::StaticStruct();
	}
	
	virtual bool Execute() override;
};

USTRUCT()
struct ARCCORE_API FArcOpenRandomLootObjectInteractionCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UArcInteractionReceiverComponent>ReceiverComponent;

	UPROPERTY()
	TObjectPtr<AActor> From;
	
	UPROPERTY()
	FGuid InteractionId;
	
	FArcOpenRandomLootObjectInteractionCommand()
		: ReceiverComponent(nullptr)
		, From(nullptr)
		, InteractionId()
	{}

	FArcOpenRandomLootObjectInteractionCommand(UArcInteractionReceiverComponent* InReceiverComponent, AActor* InFrom, const FGuid& InInteractionId)
		: ReceiverComponent(InReceiverComponent)
		, From(InFrom)
		, InteractionId(InInteractionId)
	{}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcOpenRandomLootObjectInteractionCommand::StaticStruct();
	}
	
	virtual bool Execute() override;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcPickItemFromInteractionCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<class UArcItemsStoreComponent> ToComponent;

	UPROPERTY()
	FGuid InteractionId;
	
	UPROPERTY()
	FArcLocalEntryId Id;
	
public:
	virtual bool CanSendCommand() const override {return true;};

	virtual void PreSendCommand() override {};

	virtual bool Execute() override;

	FArcPickItemFromInteractionCommand()
		: ToComponent(nullptr)
	{
	}

	FArcPickItemFromInteractionCommand(UArcItemsStoreComponent* InToComponent
						   , const FGuid& InInteractionId
						   , const FArcLocalEntryId& InLocaEntryId)
		: ToComponent(InToComponent)
		, InteractionId(InInteractionId)
		, Id(InLocaEntryId)
	{
	}

	virtual ~FArcPickItemFromInteractionCommand() override
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcPickItemFromInteractionCommand::StaticStruct();
	}
};

template <>
struct TStructOpsTypeTraits<FArcPickItemFromInteractionCommand> : public TStructOpsTypeTraitsBase2<FArcPickItemFromInteractionCommand>
{
	enum
	{
		WithCopy = true // Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcPickAllItemsFromInteractionCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<class UArcItemsStoreComponent> ToComponent;

	UPROPERTY()
	FGuid InteractionId;
	
public:
	virtual bool CanSendCommand() const override {return true;};

	virtual void PreSendCommand() override {};

	virtual bool Execute() override;

	FArcPickAllItemsFromInteractionCommand()
		: ToComponent(nullptr)
	{
	}

	FArcPickAllItemsFromInteractionCommand(UArcItemsStoreComponent* InToComponent
						   , const FGuid& InInteractionId)
		: ToComponent(InToComponent)
		, InteractionId(InInteractionId)
	{
	}

	virtual ~FArcPickAllItemsFromInteractionCommand() override
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcPickAllItemsFromInteractionCommand::StaticStruct();
	}
};

template <>
struct TStructOpsTypeTraits<FArcPickAllItemsFromInteractionCommand> : public TStructOpsTypeTraitsBase2<FArcPickAllItemsFromInteractionCommand>
{
	enum
	{
		WithCopy = true // Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcPickAllItemsAndRemoveInteractionCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<class UArcItemsStoreComponent> ToComponent;

	UPROPERTY()
	FGuid InteractionId;
	
public:
	virtual bool CanSendCommand() const override {return true;};

	virtual void PreSendCommand() override {};

	virtual bool Execute() override;

	FArcPickAllItemsAndRemoveInteractionCommand()
		: ToComponent(nullptr)
	{
	}

	FArcPickAllItemsAndRemoveInteractionCommand(UArcItemsStoreComponent* InToComponent
						   , const FGuid& InInteractionId)
		: ToComponent(InToComponent)
		, InteractionId(InInteractionId)
	{
	}

	virtual ~FArcPickAllItemsAndRemoveInteractionCommand() override
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcPickAllItemsAndRemoveInteractionCommand::StaticStruct();
	}
};

template <>
struct TStructOpsTypeTraits<FArcPickAllItemsAndRemoveInteractionCommand> : public TStructOpsTypeTraitsBase2<FArcPickAllItemsAndRemoveInteractionCommand>
{
	enum
	{
		WithCopy = true // Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};