// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcReplicatedCommand.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "ArcMassRemoveItemFromQuickSlotCommand.generated.h"

/**
 * Server-side command that removes an item from a specific QuickBar / QuickSlot pair
 * on the target Mass entity. Reverses the operations performed by
 * FArcMassAddItemToQuickSlotCommand: clears the item's slot assignment in the item
 * store fragment and removes the slot entry from the QuickBar shared fragment.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassRemoveItemFromQuickSlotCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Target Mass entity owning the quick bar. */
	UPROPERTY()
	FMassEntityHandle Entity;

	/** QuickBar BarId (matches FArcMassQuickBarEntry::BarId). */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickBar;

	/** QuickBar slot id (matches FArcMassQuickBarSlot::QuickBarSlotId). */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickSlot;

	FArcMassRemoveItemFromQuickSlotCommand() = default;

	FArcMassRemoveItemFromQuickSlotCommand(FMassEntityHandle InEntity,
		const FGameplayTag& InQuickBar,
		const FGameplayTag& InQuickSlot)
		: Entity(InEntity)
		, QuickBar(InQuickBar)
		, QuickSlot(InQuickSlot)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassRemoveItemFromQuickSlotCommand::StaticStruct();
	}
	virtual ~FArcMassRemoveItemFromQuickSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassRemoveItemFromQuickSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassRemoveItemFromQuickSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
