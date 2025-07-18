/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */



#pragma once


#include "UObject/ObjectMacros.h"

#include "ArcQuickSlotHandler.generated.h"
class UArcCoreAbilitySystemComponent;
class UArcQuickBarComponent;
struct FArcItemData;
struct FArcQuickBar;
struct FArcQuickBarSlot;
/**
 * QuickSlotHandler is small object called when Slot changes it's state.
 * When slot is somehow selected, by one of QuickBarComponent functions,
 * OnSlotSelected is called.
 * When Item is Removed/QuickSlot deselected OnSlotDeselected is called.
 *
 * Be wary, what Handlers you add to what slots. Not all handler will make sense for every slot.
 * For example if you play animation when slot is selected, it does only makes sense
 * to put such handler on bar where only one slot can be selected at time.
 *
 * Handlers should be executed on both client and server, so make sure to check where you are
 * if something should happen only on Authority.
 */
USTRUCT()
struct ARCCORE_API FArcQuickSlotHandler
{
	GENERATED_BODY()

public:
	virtual void OnSlotSelected(UArcCoreAbilitySystemComponent* InArcASC
								, UArcQuickBarComponent* QuickBar
								, const FArcItemData* InSlot
								, const FArcQuickBar* InQuickBar
								, const FArcQuickBarSlot* InQuickSlot) const
	{
	};

	virtual void OnSlotDeselected(UArcCoreAbilitySystemComponent* InArcASC
								  , UArcQuickBarComponent* QuickBar
								  , const FArcItemData* InSlot
								  , const FArcQuickBar* InQuickBar
								  , const FArcQuickBarSlot* InQuickSlot) const
	{
	};

	virtual ~FArcQuickSlotHandler() = default;
};
