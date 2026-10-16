// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Bags/ArcBagId.h"
#include "ArcMassRemoveBagCommand.generated.h"

/**
 * Server-side command that removes a bag instance from the target Mass entity's
 * bag store fragment. Fails if any slot in the bag is still occupied.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassRemoveBagCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Id of the bag instance to remove. */
	UPROPERTY(BlueprintReadWrite)
	FArcBagId BagId;

	FArcMassRemoveBagCommand() = default;

	FArcMassRemoveBagCommand(FMassEntityHandle InEntity, FArcBagId InBagId)
		: FArcMassItemReplicatedCommand(InEntity)
		, BagId(InBagId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassRemoveBagCommand::StaticStruct();
	}
	virtual ~FArcMassRemoveBagCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassRemoveBagCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassRemoveBagCommand>
{
	enum
	{
		WithCopy = true
	};
};
