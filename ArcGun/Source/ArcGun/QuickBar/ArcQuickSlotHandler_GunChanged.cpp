/**
 * This file is part of ArcX.
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



#include "ArcQuickSlotHandler_GunChanged.h"

#include "GameFramework/Actor.h"
#include "ArcCoreUtils.h"
#include "ArcGunStateComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Player/ArcHeroComponentBase.h"
#include "QuickBar/ArcQuickBarComponent.h"

void FArcQuickSlotHandler_GunChanged::OnSlotSelected(UArcCoreAbilitySystemComponent* InArcASC
													 , UArcQuickBarComponent* QuickBar
													 , const FArcItemData* InSlot
													 , const FArcQuickBar* InQuickBar
													 , const FArcQuickBarSlot* InQuickSlot) const
{
	TSubclassOf<UArcGunStateComponent> ComponentClass = UArcGunStateComponent::StaticClass();
	UArcGunStateComponent* ArcGunComponent = Arcx::Utils::GetComponent(InArcASC->GetOwner(), ComponentClass);

	if(ArcGunComponent == nullptr)
	{
		return;
	}

	ArcGunComponent->HandleQuickSlotSelected(InQuickBar->BarId, InQuickSlot->QuickBarSlotId);
}

void FArcQuickSlotHandler_GunChanged::OnSlotDeselected(UArcCoreAbilitySystemComponent* InArcASC
	, UArcQuickBarComponent* QuickBar
	, const FArcItemData* InSlot
	, const FArcQuickBar* InQuickBar
	, const FArcQuickBarSlot* InQuickSlot) const
{
	TSubclassOf<UArcGunStateComponent> ComponentClass = UArcGunStateComponent::StaticClass();
	UArcGunStateComponent* ArcGunComponent = Arcx::Utils::GetComponent(InArcASC->GetOwner(), ComponentClass);

	if(ArcGunComponent == nullptr)
	{
		return;
	}

	ArcGunComponent->HandleQuickSlotDeselected(InQuickBar->BarId, InQuickSlot->QuickBarSlotId);
}
