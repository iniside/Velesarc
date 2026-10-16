// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Items/ArcItemId.h"
#include "ArcMassRemoveItemCommand.generated.h"

/**
 * Server-side command that removes an item (or a specified number of stacks) from
 * the target Mass entity's item store fragment.
 *
 * StacksToRemove == 0 (default) removes the entire item; positive values remove
 * that many stacks and only fully remove the item once it reaches zero.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassRemoveItemCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Item to remove. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

	/** Number of stacks to remove. 0 (default) removes the whole item. */
	UPROPERTY(BlueprintReadWrite)
	int32 StacksToRemove = 0;

	FArcMassRemoveItemCommand() = default;

	FArcMassRemoveItemCommand(FMassEntityHandle InEntity,
		const FArcItemId& InItemId,
		int32 InStacksToRemove = 0,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InEntity, InStoreType)
		, ItemId(InItemId)
		, StacksToRemove(InStacksToRemove)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassRemoveItemCommand::StaticStruct();
	}
	virtual ~FArcMassRemoveItemCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassRemoveItemCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassRemoveItemCommand>
{
	enum
	{
		WithCopy = true
	};
};
