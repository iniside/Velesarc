// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcMassItemReplicatedCommand.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemSpec.h"
#include "ArcMassEquipFromDefinitionCommand.generated.h"

/**
 * Server-side command that creates a new item from an FArcItemSpec on the entity's
 * item store, assigns it to an item slot, then writes its FArcItemId into the
 * equipment fragment under the provided equipment slot tag. CanEquip is validated
 * before any mutation.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassEquipFromDefinitionCommand : public FArcMassItemReplicatedCommand
{
	GENERATED_BODY()

	/** Spec describing the item to instantiate. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemSpec Spec;

	/** Slot in the item store the new item is assigned to. */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag ItemSlotId;

	/** Slot in the equipment fragment to put the new item id under. */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag EquipmentSlotId;

	FArcMassEquipFromDefinitionCommand() = default;

	FArcMassEquipFromDefinitionCommand(FMassEntityHandle InEntity,
		const FArcItemSpec& InSpec,
		const FGameplayTag& InItemSlotId,
		const FGameplayTag& InEquipmentSlotId,
		UScriptStruct* InStoreType = FArcMassItemStoreFragment::StaticStruct())
		: FArcMassItemReplicatedCommand(InEntity, InStoreType)
		, Spec(InSpec)
		, ItemSlotId(InItemSlotId)
		, EquipmentSlotId(InEquipmentSlotId)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassEquipFromDefinitionCommand::StaticStruct();
	}
	virtual ~FArcMassEquipFromDefinitionCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassEquipFromDefinitionCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassEquipFromDefinitionCommand>
{
	enum
	{
		WithCopy = true
	};
};
