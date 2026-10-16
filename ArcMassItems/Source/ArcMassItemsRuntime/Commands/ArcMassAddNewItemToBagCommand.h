// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "Bags/ArcBagId.h"
#include "UObject/PrimaryAssetId.h"
#include "ArcMassAddNewItemToBagCommand.generated.h"

/**
 * Server-side command that creates a new item from an item record (referenced
 * by FPrimaryAssetId) and lands it in the target Mass entity's bag overlay in a
 * single round-trip. With an invalid BagId the item goes to the first free accepting
 * slot across all bags; with a valid BagId + SlotIndex it goes to exactly that slot.
 *
 * The inherited StoreType is unused: the bag path derives the target store from
 * FArcMassBagStoreFragment::TargetStoreType so it can never mismatch the bag.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassAddNewItemToBagCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Id of the item record to instantiate on the server. */
	UPROPERTY(BlueprintReadWrite)
	FPrimaryAssetId ItemDefinitionId;

	/** Item level for the created spec. */
	UPROPERTY(BlueprintReadWrite)
	uint8 Level = 1;

	/** Stack amount for the created spec. */
	UPROPERTY(BlueprintReadWrite)
	int32 Amount = 1;

	/** Target bag. Invalid → first free accepting slot across all bags (SlotIndex ignored). */
	UPROPERTY(BlueprintReadWrite)
	FArcBagId BagId;

	/** Zero-based slot index within BagId; required (!= INDEX_NONE) when BagId is valid. */
	UPROPERTY(BlueprintReadWrite)
	int32 SlotIndex = INDEX_NONE;

	FArcMassAddNewItemToBagCommand() = default;

	FArcMassAddNewItemToBagCommand(FMassEntityHandle InEntity,
		const FPrimaryAssetId& InItemDefinitionId,
		uint8 InLevel,
		int32 InAmount,
		FArcBagId InBagId = FArcBagId(),
		int32 InSlotIndex = INDEX_NONE)
		: FArcMassItemReplicatedCommand(InEntity)
		, ItemDefinitionId(InItemDefinitionId)
		, Level(InLevel)
		, Amount(InAmount)
		, BagId(InBagId)
		, SlotIndex(InSlotIndex)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassAddNewItemToBagCommand::StaticStruct();
	}
	virtual ~FArcMassAddNewItemToBagCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassAddNewItemToBagCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassAddNewItemToBagCommand>
{
	enum
	{
		WithCopy = true
	};
};
