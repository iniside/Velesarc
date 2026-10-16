// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityHandle.h"
#include "ArcMassMoveItemCommand.generated.h"

/**
 * Moves the item and its attached subtree between entity stores.
 * Base Entity/StoreType are the source; ToEntity/ToStoreType are the destination.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassMoveItemCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Destination entity that will receive the item. */
	UPROPERTY()
	FMassEntityHandle ToEntity;

	/** Store fragment type on the destination entity. Defaults to FArcMassItemStoreFragment. */
	UPROPERTY()
	TObjectPtr<UScriptStruct> ToStoreType = nullptr;

	/** Item to move. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

	FArcMassMoveItemCommand() = default;

	FArcMassMoveItemCommand(FMassEntityHandle InFromEntity,
		const FArcItemId& InItemId,
		FMassEntityHandle InToEntity,
		UScriptStruct* InFromStoreType = FArcMassItemStoreFragment::StaticStruct(),
		UScriptStruct* InToStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InFromEntity, InFromStoreType)
		, ToEntity(InToEntity)
		, ToStoreType(InToStoreType)
		, ItemId(InItemId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassMoveItemCommand::StaticStruct();
	}
	virtual ~FArcMassMoveItemCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassMoveItemCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassMoveItemCommand>
{
	enum
	{
		WithCopy = true
	};
};
