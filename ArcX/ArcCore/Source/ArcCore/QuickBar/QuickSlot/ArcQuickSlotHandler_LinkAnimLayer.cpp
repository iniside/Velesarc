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



#include "ArcQuickSlotHandler_LinkAnimLayer.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"

#include "Animation/AnimInstance.h"
#include "Items/ArcItemData.h"

void FArcQuickSlotHandler_LinkAnimLayer::OnSlotSelected(UArcCoreAbilitySystemComponent* InArcASC
														, class UArcQuickBarComponent* QuickBar
														, const FArcItemData* InSlot
														, const struct FArcQuickBar* InQuickBar
														, const struct FArcQuickBarSlot* InQuickSlot) const
{
	APlayerState* PS = Cast<APlayerState>(InArcASC->GetOwner());
	if (PS == nullptr)
	{
		return;
	}

	ACharacter* C = PS->GetPawn<ACharacter>();
	if (C == nullptr)
	{
		return;
	}

	UArcItemAttachmentComponent* IAC = UArcItemAttachmentComponent::FindItemAttachmentComponent(PS);
	if (IAC == nullptr)
	{
		return;
	}

	IAC->LinkAnimLayer(InSlot->GetSlotId());
}

void FArcQuickSlotHandler_LinkAnimLayer::OnSlotDeselected(UArcCoreAbilitySystemComponent* InArcASC
														  , class UArcQuickBarComponent* QuickBar
														  , const FArcItemData* InSlot
														  , const struct FArcQuickBar* InQuickBar
								, const struct FArcQuickBarSlot* InQuickSlot) const
{
	APlayerState* PS = Cast<APlayerState>(InArcASC->GetOwner());
	if (PS == nullptr)
	{
		return;
	}

	ACharacter* C = PS->GetPawn<ACharacter>();
	if (C == nullptr)
	{
		return;
	}

	UArcItemAttachmentComponent* IAC = UArcItemAttachmentComponent::FindItemAttachmentComponent(PS);
	if (IAC == nullptr)
	{
		return;
	}

	IAC->UnlinkAnimLayer(InSlot->GetSlotId());
}