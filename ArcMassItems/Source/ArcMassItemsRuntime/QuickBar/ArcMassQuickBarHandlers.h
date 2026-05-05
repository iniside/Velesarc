// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"

#include "ArcMassQuickBarHandlers.generated.h"

struct FArcMassQuickBarSlot;
struct FArcMassQuickBarEntry;
struct FArcItemData;

/**
 * Base class for slot-level handlers called when a quickbar slot is selected or deselected.
 * Handlers are configured per-slot via FArcMassQuickBarSlot::SelectedHandlers (FInstancedStruct).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Arc Mass Quick Slot Handler"))
struct ARCMASSITEMSRUNTIME_API FArcMassQuickSlotHandler
{
	GENERATED_BODY()

public:
	virtual void OnSlotSelected(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const FArcItemData* ItemData, const FArcMassQuickBarEntry* BarConfig,
		const FArcMassQuickBarSlot* SlotConfig) const
	{
	}

	virtual void OnSlotDeselected(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const FArcItemData* ItemData, const FArcMassQuickBarEntry* BarConfig,
		const FArcMassQuickBarSlot* SlotConfig) const
	{
	}

	virtual ~FArcMassQuickSlotHandler() = default;
};

/**
 * Base class for slot validators used during bar cycling.
 * If a validator returns false, the slot is skipped during cycling.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Arc Mass Quick Bar Slot Validator"))
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarSlotValidator
{
	GENERATED_BODY()

public:
	virtual bool IsValid(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const FArcMassQuickBarEntry& Bar, const FArcMassQuickBarSlot& Slot) const
	{
		return true;
	}

	virtual ~FArcMassQuickBarSlotValidator() = default;
};

/**
 * Base class for bar-level actions called when a bar is activated or deactivated.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Arc Mass Quick Bar Selected Action"))
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarSelectedAction
{
	GENERATED_BODY()

public:
	virtual void QuickBarActivated(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const FArcMassQuickBarEntry& Bar) const
	{
	}

	virtual void QuickBarDeactivated(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const FArcMassQuickBarEntry& Bar) const
	{
	}

	virtual ~FArcMassQuickBarSelectedAction() = default;
};
