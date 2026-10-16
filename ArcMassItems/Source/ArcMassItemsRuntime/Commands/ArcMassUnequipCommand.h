// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "GameplayTagContainer.h"
#include "ArcMassUnequipCommand.generated.h"

/**
 * Server-side command that removes the item currently bound to EquipmentSlotId from
 * the entity's FArcMassEquipmentFragment AND frees its item-store slot via
 * ArcMassItems::RemoveItemFromSlot. The item itself remains in the store (unslotted).
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassUnequipCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Equipment slot the bound item is removed from. */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag EquipmentSlotId;

	FArcMassUnequipCommand() = default;

	FArcMassUnequipCommand(FMassEntityHandle InEntity,
		const FGameplayTag& InEquipmentSlotId,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InEntity, InStoreType)
		, EquipmentSlotId(InEquipmentSlotId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassUnequipCommand::StaticStruct();
	}
	virtual ~FArcMassUnequipCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassUnequipCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassUnequipCommand>
{
	enum
	{
		WithCopy = true
	};
};
