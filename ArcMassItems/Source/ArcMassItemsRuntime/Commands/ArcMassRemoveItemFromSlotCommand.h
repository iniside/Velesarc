// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Items/ArcItemId.h"
#include "ArcMassRemoveItemFromSlotCommand.generated.h"

/**
 * Server-side command that clears the slot assignment for an item in the target
 * Mass entity's item store fragment. The item itself stays in the store.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassRemoveItemFromSlotCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Item whose slot assignment should be cleared. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

	FArcMassRemoveItemFromSlotCommand() = default;

	FArcMassRemoveItemFromSlotCommand(FMassEntityHandle InEntity,
		const FArcItemId& InItemId,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InEntity, InStoreType)
		, ItemId(InItemId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassRemoveItemFromSlotCommand::StaticStruct();
	}
	virtual ~FArcMassRemoveItemFromSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassRemoveItemFromSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassRemoveItemFromSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
