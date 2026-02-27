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
#include "Items/ArcItemId.h"
#include "ArcItemReplicatedCommand.generated.h"

class UArcItemsStoreComponent;

/** Pairs an item ID with the version snapshot taken at command send time. */
USTRUCT()
struct FArcItemExpectedVersion
{
	GENERATED_BODY()

	UPROPERTY()
	FArcItemId ItemId;

	UPROPERTY()
	uint32 Version = 0;

	FArcItemExpectedVersion() = default;
	FArcItemExpectedVersion(const FArcItemId& InItemId, uint32 InVersion)
		: ItemId(InItemId), Version(InVersion)
	{}
};

/**
 * Intermediate base for commands that interact with items.
 * Provides PendingItemIds tracking and version-based idempotency.
 */
USTRUCT()
struct ARCCORE_API FArcItemReplicatedCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

public:
	/** Captures the Version of each pending item from the store. Reads from PendingItemIds. */
	void CaptureExpectedVersions(const UArcItemsStoreComponent* Store);

	/** Returns false if any pending item's version on the server differs from what the client captured. */
	bool ValidateVersions(const UArcItemsStoreComponent* Store) const;

	FArcItemReplicatedCommand() {}
	virtual ~FArcItemReplicatedCommand() override = default;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcItemReplicatedCommand::StaticStruct();
	}

protected:
	/** Item IDs marked as pending while this command is in flight. Populated during PreSendCommand. */
	UPROPERTY()
	TArray<FArcItemId> PendingItemIds;

	UPROPERTY()
	TArray<FArcItemExpectedVersion> ExpectedVersions;
};

template <>
struct TStructOpsTypeTraits<FArcItemReplicatedCommand>
	: public TStructOpsTypeTraitsBase2<FArcItemReplicatedCommand>
{
	enum { WithCopy = true };
};
