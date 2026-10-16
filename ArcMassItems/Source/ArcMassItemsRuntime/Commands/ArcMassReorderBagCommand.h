// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Bags/ArcBagId.h"
#include "ArcMassReorderBagCommand.generated.h"

/**
 * Server-side command that moves a bag instance to a new display position within the
 * target Mass entity's bag store fragment. The BagId remains stable after reorder.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassReorderBagCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Target bag instance to reorder. */
	UPROPERTY(BlueprintReadWrite)
	FArcBagId BagId;

	/** New zero-based display position for the bag instance (clamped to valid range). */
	UPROPERTY(BlueprintReadWrite)
	int32 NewDisplayIndex = 0;

	FArcMassReorderBagCommand() = default;

	FArcMassReorderBagCommand(FMassEntityHandle InEntity, FArcBagId InBagId, int32 InNewDisplayIndex)
		: FArcMassItemReplicatedCommand(InEntity)
		, BagId(InBagId)
		, NewDisplayIndex(InNewDisplayIndex)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassReorderBagCommand::StaticStruct();
	}
	virtual ~FArcMassReorderBagCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassReorderBagCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassReorderBagCommand>
{
	enum
	{
		WithCopy = true
	};
};
