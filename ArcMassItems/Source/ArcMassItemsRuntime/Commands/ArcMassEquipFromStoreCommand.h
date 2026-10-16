// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemTypes.h"
#include "ArcMassEquipFromStoreCommand.generated.h"

/**
 * Server-side command that equips an item already present inside the entity's item
 * store (FArcMassItemStoreFragment or derived). The item is moved to the requested
 * item-store slot via ChangeItemSlot/AddItemToSlot semantics, then its FArcItemId is
 * written into the equipment fragment's EquippedItems map under the requested
 * equipment slot tag. CanEquip is validated before any mutation.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassEquipFromStoreCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Existing item id inside the store. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

	/** Item-store slot to assign the item to. */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag ItemSlotId;

	/** Equipment slot the ItemId is written under. */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag EquipmentSlotId;

	FArcMassEquipFromStoreCommand() = default;

	FArcMassEquipFromStoreCommand(FMassEntityHandle InEntity,
		const FArcItemId& InItemId,
		const FGameplayTag& InItemSlotId,
		const FGameplayTag& InEquipmentSlotId,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InEntity, InStoreType)
		, ItemId(InItemId)
		, ItemSlotId(InItemSlotId)
		, EquipmentSlotId(InEquipmentSlotId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassEquipFromStoreCommand::StaticStruct();
	}
	virtual ~FArcMassEquipFromStoreCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassEquipFromStoreCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassEquipFromStoreCommand>
{
	enum
	{
		WithCopy = true
	};
};
