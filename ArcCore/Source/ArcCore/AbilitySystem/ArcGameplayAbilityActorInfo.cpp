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

#include "ArcGameplayAbilityActorInfo.h"

#include "ArcCoreUtils.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystem/ArcAbilityTargetingComponent.h"
#include "Equipment/ArcEquipmentComponent.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/Pawn.h"
#include "QuickBar/ArcQuickBarComponent.h"

void FArcGameplayAbilityActorInfo::InitFromActor(AActor* InOwnerActor
												 , AActor* InAvatarActor
												 , UAbilitySystemComponent* InAbilitySystemComponent)
{
	Super::InitFromActor(InOwnerActor
		, InAvatarActor
		, InAbilitySystemComponent);

	if (!ItemAttachmentComponent && InOwnerActor)
	{
		ItemAttachmentComponent = Arcx::Utils::GetComponent<UArcItemAttachmentComponent>(InOwnerActor, UArcItemAttachmentComponent::StaticClass());
	}
	if (!ItemAttachmentComponent && InAvatarActor)
	{
		ItemAttachmentComponent = Arcx::Utils::GetComponent<UArcItemAttachmentComponent>(InOwnerActor, UArcItemAttachmentComponent::StaticClass());
	}

	if (!EquipmentComponent && InOwnerActor)
	{
		EquipmentComponent = Arcx::Utils::GetComponent<UArcEquipmentComponent>(InOwnerActor, UArcEquipmentComponent::StaticClass());
	}
	if (!EquipmentComponent && InAvatarActor)
	{
		EquipmentComponent = Arcx::Utils::GetComponent<UArcEquipmentComponent>(InOwnerActor, UArcEquipmentComponent::StaticClass());
	}

	if (!QuickBarComponent && InOwnerActor)
	{
		QuickBarComponent = Arcx::Utils::GetComponent<UArcQuickBarComponent>(InOwnerActor, UArcQuickBarComponent::StaticClass());
	}
	if (!QuickBarComponent && InAvatarActor)
	{
		QuickBarComponent = Arcx::Utils::GetComponent<UArcQuickBarComponent>(InOwnerActor, UArcQuickBarComponent::StaticClass());
	}
	
	OwnerPawn = Cast<APawn>(InAvatarActor);
}
