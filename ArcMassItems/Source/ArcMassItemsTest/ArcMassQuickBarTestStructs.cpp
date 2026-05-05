// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassQuickBarTestStructs.h"
#include "QuickBar/ArcMassQuickBarTypes.h"

int32 FArcQuickBarTestSlotHandler::SelectedCount = 0;
int32 FArcQuickBarTestSlotHandler::DeselectedCount = 0;

void FArcQuickBarTestSlotHandler::Reset()
{
	SelectedCount = 0;
	DeselectedCount = 0;
}

void FArcQuickBarTestSlotHandler::OnSlotSelected(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
	const FArcItemData* ItemData, const FArcMassQuickBarEntry* BarConfig,
	const FArcMassQuickBarSlot* SlotConfig) const
{
	++SelectedCount;
}

void FArcQuickBarTestSlotHandler::OnSlotDeselected(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
	const FArcItemData* ItemData, const FArcMassQuickBarEntry* BarConfig,
	const FArcMassQuickBarSlot* SlotConfig) const
{
	++DeselectedCount;
}

FGameplayTag FArcQuickBarTestValidator::BlockedSlotId = FGameplayTag::EmptyTag;

bool FArcQuickBarTestValidator::IsValid(FMassEntityManager& /*EntityManager*/, FMassEntityHandle /*Entity*/,
	const FArcMassQuickBarEntry& /*Bar*/, const FArcMassQuickBarSlot& Slot) const
{
	return Slot.QuickBarSlotId != BlockedSlotId;
}

int32 FArcQuickBarTestBarAction::ActivatedCount = 0;
int32 FArcQuickBarTestBarAction::DeactivatedCount = 0;

void FArcQuickBarTestBarAction::Reset()
{
	ActivatedCount = 0;
	DeactivatedCount = 0;
}

void FArcQuickBarTestBarAction::QuickBarActivated(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
	const FArcMassQuickBarEntry& Bar) const
{
	++ActivatedCount;
}

void FArcQuickBarTestBarAction::QuickBarDeactivated(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
	const FArcMassQuickBarEntry& Bar) const
{
	++DeactivatedCount;
}
