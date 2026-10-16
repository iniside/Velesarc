// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "QuickBar/ArcMassQuickBarHandlers.h"

#include "ArcMassQuickBarTestStructs.generated.h"

USTRUCT(meta = (Hidden))
struct FArcQuickBarTestSlotHandler : public FArcMassQuickSlotHandler
{
	GENERATED_BODY()

	static int32 SelectedCount;
	static int32 DeselectedCount;
	static void Reset();

	virtual void OnSlotSelected(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const FArcItemData* ItemData, const FArcMassQuickBarEntry* BarConfig,
		const FArcMassQuickBarSlot* SlotConfig) const override;

	virtual void OnSlotDeselected(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const FArcItemData* ItemData, const FArcMassQuickBarEntry* BarConfig,
		const FArcMassQuickBarSlot* SlotConfig) const override;
};

USTRUCT(meta = (Hidden))
struct FArcQuickBarTestValidator : public FArcMassQuickBarSlotValidator
{
	GENERATED_BODY()

	static FGameplayTag BlockedSlotId;

	virtual bool IsValid(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const FArcMassQuickBarEntry& Bar, const FArcMassQuickBarSlot& Slot) const override;
};

USTRUCT(meta = (Hidden))
struct FArcQuickBarTestBarAction : public FArcMassQuickBarSelectedAction
{
	GENERATED_BODY()

	static int32 ActivatedCount;
	static int32 DeactivatedCount;
	static void Reset();

	virtual void QuickBarActivated(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const FArcMassQuickBarEntry& Bar) const override;

	virtual void QuickBarDeactivated(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const FArcMassQuickBarEntry& Bar) const override;
};
