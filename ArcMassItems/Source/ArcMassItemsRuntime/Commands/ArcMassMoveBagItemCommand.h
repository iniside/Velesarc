// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Bags/ArcBagId.h"
#include "ArcMassMoveBagItemCommand.generated.h"

/**
 * Server-side command that moves or swaps an item between two bag slots on the same
 * target Mass entity. Source and destination may be the same bag (intra-bag move)
 * or different bags (inter-bag move).
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassMoveBagItemCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Source bag instance. */
	UPROPERTY(BlueprintReadWrite)
	FArcBagId FromBag;

	/** Zero-based slot index to move from. */
	UPROPERTY(BlueprintReadWrite)
	int32 FromSlot = 0;

	/** Destination bag instance. */
	UPROPERTY(BlueprintReadWrite)
	FArcBagId ToBag;

	/** Zero-based slot index to move into. */
	UPROPERTY(BlueprintReadWrite)
	int32 ToSlot = 0;

	FArcMassMoveBagItemCommand() = default;

	FArcMassMoveBagItemCommand(FMassEntityHandle InEntity,
		FArcBagId InFromBag,
		int32 InFromSlot,
		FArcBagId InToBag,
		int32 InToSlot)
		: FArcMassItemReplicatedCommand(InEntity)
		, FromBag(InFromBag)
		, FromSlot(InFromSlot)
		, ToBag(InToBag)
		, ToSlot(InToSlot)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassMoveBagItemCommand::StaticStruct();
	}
	virtual ~FArcMassMoveBagItemCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassMoveBagItemCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassMoveBagItemCommand>
{
	enum
	{
		WithCopy = true
	};
};
