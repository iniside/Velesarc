// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Items/ArcItemId.h"
#include "ArcMassDestroyItemCommand.generated.h"

/**
 * Destroys the item and its attachment subtree with full lifecycle
 * (OnPreRemove, detach, slot clear).
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassDestroyItemCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Item to destroy. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

	FArcMassDestroyItemCommand() = default;

	FArcMassDestroyItemCommand(FMassEntityHandle InEntity,
		const FArcItemId& InItemId,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InEntity, InStoreType)
		, ItemId(InItemId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassDestroyItemCommand::StaticStruct();
	}
	virtual ~FArcMassDestroyItemCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassDestroyItemCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassDestroyItemCommand>
{
	enum
	{
		WithCopy = true
	};
};
