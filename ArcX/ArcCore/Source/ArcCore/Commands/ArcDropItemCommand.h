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

#include "Commands/ArcItemReplicatedCommand.h"
#include "Items/ArcItemId.h"
#include "Math/TransformVectorized.h"
#include "UObject/ObjectPtr.h"

#include "ArcDropItemCommand.generated.h"

class UArcItemsStoreComponent;

/**
 * Drops an item from an ItemsStoreComponent into the world as a Mass entity.
 * The item definition must have FArcItemFragment_Droppable with a valid DropEntityConfig.
 * The dropped entity reuses FArcLootContainerFragment to hold the item spec.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcDropItemCommand : public FArcItemReplicatedCommand
{
	GENERATED_BODY()

	/** Items store from which the item will be dropped. */
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcItemsStoreComponent> ItemsStoreComponent;

	/** Id of the item to drop. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

	/** Transform at which the dropped entity will be spawned. */
	UPROPERTY(BlueprintReadWrite)
	FTransform SpawnTransform;

	/** Number of stacks to drop. 0 means drop all stacks. */
	UPROPERTY(BlueprintReadWrite)
	int32 StacksToDrop = 0;

public:
	FArcDropItemCommand()
		: ItemsStoreComponent(nullptr)
		, ItemId()
		, SpawnTransform(FTransform::Identity)
		, StacksToDrop(0)
	{
	}

	FArcDropItemCommand(UArcItemsStoreComponent* InItemsStoreComponent
		, const FArcItemId& InItemId
		, const FTransform& InSpawnTransform
		, int32 InStacksToDrop = 0)
		: ItemsStoreComponent(InItemsStoreComponent)
		, ItemId(InItemId)
		, SpawnTransform(InSpawnTransform)
		, StacksToDrop(InStacksToDrop)
	{
	}

	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;
	virtual bool NeedsConfirmation() const override { return true; }
	virtual void CommandConfirmed(bool bSuccess) override;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcDropItemCommand::StaticStruct();
	}
	virtual ~FArcDropItemCommand() override {}
};

template <>
struct TStructOpsTypeTraits<FArcDropItemCommand>
	: public TStructOpsTypeTraitsBase2<FArcDropItemCommand>
{
	enum { WithCopy = true };
};
