// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "ArcMassAssignItemSlotCommand.generated.h"

/**
 * Server-side command that assigns an existing item (already present in the target
 * Mass entity's item store fragment) to a specific item slot tag.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassAssignItemSlotCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Existing item already stored on the entity that should be slotted. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

	/** Slot tag to assign the item to. */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag SlotId;

	FArcMassAssignItemSlotCommand() = default;

	FArcMassAssignItemSlotCommand(FMassEntityHandle InEntity,
		const FArcItemId& InItemId,
		const FGameplayTag& InSlotId,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InEntity, InStoreType)
		, ItemId(InItemId)
		, SlotId(InSlotId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassAssignItemSlotCommand::StaticStruct();
	}
	virtual ~FArcMassAssignItemSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassAssignItemSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassAssignItemSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
