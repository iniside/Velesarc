// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcReplicatedCommand.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "ArcMassAddItemToQuickSlotCommand.generated.h"

/**
 * Server-side command that assigns an existing item (already present in the target
 * Mass entity's item store fragment) to a specific QuickBar / QuickSlot pair.
 *
 * Behaviour mirrors FArcAddItemToQuickBarCommand but operates against a Mass entity
 * via the FArcMassItemStoreFragment + FArcMassQuickBarSharedFragment pipeline.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassAddItemToQuickSlotCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Target Mass entity owning the quick bar. */
	UPROPERTY()
	FMassEntityHandle Entity;

	/** Existing item already stored on the entity that should be slotted. */
	UPROPERTY(BlueprintReadWrite)
	FArcItemId ItemId;

	/** QuickBar BarId (matches FArcMassQuickBarEntry::BarId). */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickBar;

	/** QuickBar slot id (matches FArcMassQuickBarSlot::QuickBarSlotId). */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickSlot;

	FArcMassAddItemToQuickSlotCommand() = default;

	FArcMassAddItemToQuickSlotCommand(FMassEntityHandle InEntity,
		const FArcItemId& InItemId,
		const FGameplayTag& InQuickBar,
		const FGameplayTag& InQuickSlot)
		: Entity(InEntity)
		, ItemId(InItemId)
		, QuickBar(InQuickBar)
		, QuickSlot(InQuickSlot)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassAddItemToQuickSlotCommand::StaticStruct();
	}
	virtual ~FArcMassAddItemToQuickSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassAddItemToQuickSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassAddItemToQuickSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
