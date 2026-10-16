// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcReplicatedCommand.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "ArcMassAddNewItemToQuickSlotCommand.generated.h"

/**
 * Server-side command that creates a new item from an item record
 * (referenced by FPrimaryAssetId) on the target Mass entity's item store
 * fragment AND assigns the freshly-created item to a specific QuickBar /
 * QuickSlot pair, all in a single round-trip.
 *
 * Definition variant of FArcMassAddItemToQuickSlotCommand.
 */
USTRUCT(BlueprintType)
struct ARCMASSITEMSRUNTIME_API FArcMassAddNewItemToQuickSlotCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Target Mass entity owning the quick bar. */
	UPROPERTY()
	FMassEntityHandle Entity;

	/** Id of the item record to instantiate on the server. */
	UPROPERTY(BlueprintReadWrite)
	FPrimaryAssetId ItemDefinitionId;

	/** QuickBar BarId (matches FArcMassQuickBarEntry::BarId). */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickBar;

	/** QuickBar slot id (matches FArcMassQuickBarSlot::QuickBarSlotId). */
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag QuickSlot;

	FArcMassAddNewItemToQuickSlotCommand() = default;

	FArcMassAddNewItemToQuickSlotCommand(FMassEntityHandle InEntity,
		const FPrimaryAssetId& InItemDefinitionId,
		const FGameplayTag& InQuickBar,
		const FGameplayTag& InQuickSlot)
		: Entity(InEntity)
		, ItemDefinitionId(InItemDefinitionId)
		, QuickBar(InQuickBar)
		, QuickSlot(InQuickSlot)
	{}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcMassAddNewItemToQuickSlotCommand::StaticStruct();
	}
	virtual ~FArcMassAddNewItemToQuickSlotCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcMassAddNewItemToQuickSlotCommand>
	: public TStructOpsTypeTraitsBase2<FArcMassAddNewItemToQuickSlotCommand>
{
	enum
	{
		WithCopy = true
	};
};
