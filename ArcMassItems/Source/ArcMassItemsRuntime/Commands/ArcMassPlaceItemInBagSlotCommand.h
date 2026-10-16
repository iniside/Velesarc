// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Bags/ArcBagId.h"
#include "Items/ArcItemId.h"
#include "ArcMassPlaceItemInBagSlotCommand.generated.h"

/**
 * Server-side command that places a resident item into an empty, in-range bag slot
 * on the target Mass entity. Fails if the slot is occupied or the item is rejected
 * by the bag's acceptance rules.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassPlaceItemInBagSlotCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Target bag instance. */
	UPROPERTY(BlueprintReadWrite)
	FArcBagId BagId;

	/** Zero-based slot index within the bag. */
	UPROPERTY(BlueprintReadWrite)
	int32 SlotIndex = 0;

	/** Id of the item (already in the entity's item store) to place into the slot. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

	FArcMassPlaceItemInBagSlotCommand() = default;

	FArcMassPlaceItemInBagSlotCommand(FMassEntityHandle InEntity,
		FArcBagId InBagId,
		int32 InSlotIndex,
		FArcItemId InItemId)
		: FArcMassItemReplicatedCommand(InEntity)
		, BagId(InBagId)
		, SlotIndex(InSlotIndex)
		, ItemId(InItemId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassPlaceItemInBagSlotCommand::StaticStruct();
	}
	virtual ~FArcMassPlaceItemInBagSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassPlaceItemInBagSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassPlaceItemInBagSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
