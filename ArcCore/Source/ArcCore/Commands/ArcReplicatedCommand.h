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

#include "ArcReplicatedCommand.generated.h"

class UScriptStruct;
class AArcCorePlayerController;

USTRUCT()
struct ARCCORE_API FArcReplicatedCommand
{
	GENERATED_BODY()

public:
	/**
	 * Called before command is send to server, to check if it even worth sending RPC.
	 */
	virtual bool CanSendCommand() const
	{
		return true;
	};
	
	/*
	 * Called before sending to server (On client).
	 */
	virtual void PreSendCommand()
	{
	}

	virtual bool Execute()
	{
		return false;
	};

	FArcReplicatedCommand()
	{
	}

	virtual ~FArcReplicatedCommand() = default;

	virtual UScriptStruct* GetScriptStruct() const
	{
		return FArcReplicatedCommand::StaticStruct();
	}
};

template <>
struct TStructOpsTypeTraits<FArcReplicatedCommand> : public TStructOpsTypeTraitsBase2<FArcReplicatedCommand>
{
	enum
	{
		WithCopy = true // Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};

USTRUCT()
struct FArcReplicatedCommandId
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = INDEX_NONE;

	friend uint32 GetTypeHash(const FArcReplicatedCommandId& In)
	{
		return In.Id;
	}

	bool operator==(const FArcReplicatedCommandId& Other) const
	{
		return Id == Other.Id;
	}
	
	bool IsValid() const
	{
		return Id > -1;
	}

	static FArcReplicatedCommandId Generate()
	{
		static int32 Idx = INDEX_NONE;
		Idx++;
		FArcReplicatedCommandId NewId {Idx};

		return NewId;
	}
};

DECLARE_DELEGATE_OneParam(FArcCommandConfirmedDelegate, bool);

USTRUCT(BlueprintType)
struct ARCCORE_API FArcReplicatedCommandHandle
{
	GENERATED_BODY()

private:
	TSharedPtr<FArcReplicatedCommand> Data;

	UPROPERTY()
	FArcReplicatedCommandId CommandId;
	
public:
	FArcReplicatedCommandHandle()
		: Data(nullptr)
	{
	}

	FArcReplicatedCommandHandle(const TSharedPtr<FArcReplicatedCommand>& InData)
		: Data(InData)
	{
	}

	FArcReplicatedCommandHandle(const TSharedPtr<FArcReplicatedCommand>& InData, const FArcReplicatedCommandId& InCommandId)
		: Data(InData)
		, CommandId(InCommandId)
	{
	}

public:
	TSharedPtr<FArcReplicatedCommand>& GetData()
	{
		return Data;
	}

	bool Execute() const
	{
		return Data->Execute();
	}

	bool CanSendCommand() const
	{
		return Data->CanSendCommand();
	};

	void PreSendCommand() const
	{
		Data->PreSendCommand();
	}

	bool IsValid() const
	{
		return Data.IsValid();
	}

	bool operator==(const FArcReplicatedCommandHandle& Other) const
	{
		return Data.IsValid() && Other.IsValid() && Data->GetScriptStruct() == Other.Data->GetScriptStruct();
	}
	
	const FArcReplicatedCommandId& GetCommandId() const
	{
		return CommandId;
	}
};