// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Bags/ArcBagId.h"
#include "ArcMassClearBagSlotCommand.generated.h"

/**
 * Server-side command that clears a bag slot's item reference on the target Mass entity.
 * The item itself remains in the entity's item store; only the slot reference is removed.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassClearBagSlotCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Target bag instance. */
	UPROPERTY(BlueprintReadWrite)
	FArcBagId BagId;

	/** Zero-based slot index to clear. */
	UPROPERTY(BlueprintReadWrite)
	int32 SlotIndex = 0;

	FArcMassClearBagSlotCommand() = default;

	FArcMassClearBagSlotCommand(FMassEntityHandle InEntity, FArcBagId InBagId, int32 InSlotIndex)
		: FArcMassItemReplicatedCommand(InEntity)
		, BagId(InBagId)
		, SlotIndex(InSlotIndex)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassClearBagSlotCommand::StaticStruct();
	}
	virtual ~FArcMassClearBagSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassClearBagSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassClearBagSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
