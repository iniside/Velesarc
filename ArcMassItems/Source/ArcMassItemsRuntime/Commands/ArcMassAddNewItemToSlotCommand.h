// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "ArcMassAddNewItemToSlotCommand.generated.h"

/**
 * Server-side command that creates a new item from an item record (referenced
 * by FPrimaryAssetId) on the target Mass entity's item store fragment AND assigns
 * the freshly-created item to a slot, all in a single round-trip.
 *
 * Definition variant of FArcMassAddItemToSlotCommand.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassAddNewItemToSlotCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Id of the item record to instantiate on the server. */
	UPROPERTY(BlueprintReadWrite)
	FPrimaryAssetId ItemDefinitionId;

	/** Slot tag to assign the newly created item to. */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag SlotId;

	FArcMassAddNewItemToSlotCommand() = default;

	FArcMassAddNewItemToSlotCommand(FMassEntityHandle InEntity,
		const FPrimaryAssetId& InItemDefinitionId,
		const FGameplayTag& InSlotId,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InEntity, InStoreType)
		, ItemDefinitionId(InItemDefinitionId)
		, SlotId(InSlotId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassAddNewItemToSlotCommand::StaticStruct();
	}
	virtual ~FArcMassAddNewItemToSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassAddNewItemToSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassAddNewItemToSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
