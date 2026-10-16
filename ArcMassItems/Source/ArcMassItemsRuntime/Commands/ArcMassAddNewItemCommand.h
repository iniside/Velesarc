// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "UObject/PrimaryAssetId.h"
#include "ArcMassAddNewItemCommand.generated.h"

/**
 * Server-side command that creates a new item from an item record (referenced
 * by FPrimaryAssetId) on the target Mass entity's item store fragment.
 *
 * Definition variant of FArcMassAddItemCommand.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassAddNewItemCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Id of the item record to instantiate on the server. */
	UPROPERTY(BlueprintReadWrite)
	FPrimaryAssetId ItemDefinitionId;

	FArcMassAddNewItemCommand() = default;

	FArcMassAddNewItemCommand(FMassEntityHandle InEntity,
		const FPrimaryAssetId& InItemDefinitionId,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InEntity, InStoreType)
		, ItemDefinitionId(InItemDefinitionId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassAddNewItemCommand::StaticStruct();
	}
	virtual ~FArcMassAddNewItemCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassAddNewItemCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassAddNewItemCommand>
{
	enum
	{
		WithCopy = true
	};
};
