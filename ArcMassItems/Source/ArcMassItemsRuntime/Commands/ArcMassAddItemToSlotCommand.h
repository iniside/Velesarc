// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemSpec.h"
#include "ArcMassAddItemToSlotCommand.generated.h"

/**
 * Server-side command that creates a new item from an FArcItemSpec on the target
 * Mass entity's item store fragment AND assigns the freshly-created item to a slot,
 * all in a single round-trip.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassAddItemToSlotCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Spec describing the item to instantiate. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemSpec Spec;

	/** Slot tag to assign the newly created item to. */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag SlotId;

	FArcMassAddItemToSlotCommand() = default;

	FArcMassAddItemToSlotCommand(FMassEntityHandle InEntity,
		const FArcItemSpec& InSpec,
		const FGameplayTag& InSlotId,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InEntity, InStoreType)
		, Spec(InSpec)
		, SlotId(InSlotId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassAddItemToSlotCommand::StaticStruct();
	}
	virtual ~FArcMassAddItemToSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassAddItemToSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassAddItemToSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
