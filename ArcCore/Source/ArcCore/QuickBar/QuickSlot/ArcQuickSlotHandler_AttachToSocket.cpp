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



#include "ArcQuickSlotHandler_AttachToSocket.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemData.h"

void FArcQuickSlotHandler_AttachToSocket::OnSlotSelected(UArcCoreAbilitySystemComponent* InArcASC
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

	if (PS->GetPawn()->GetLocalRole() < ENetRole::ROLE_AutonomousProxy)
	{
		return;
	}

	UArcItemAttachmentComponent* IAC = UArcItemAttachmentComponent::FindItemAttachmentComponent(PS);
	if (IAC == nullptr)
	{
		return;
	}

	IAC->AttachItemToSocket(InSlot->GetItemId()
		,SocketName , ComponentTag);
}

void FArcQuickSlotHandler_AttachToSocket::OnSlotDeselected(UArcCoreAbilitySystemComponent* InArcASC
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

	if (PS->GetPawn()->GetLocalRole() < ENetRole::ROLE_AutonomousProxy)
	{
		return;
	}

	UArcItemAttachmentComponent* IAC = UArcItemAttachmentComponent::FindItemAttachmentComponent(PS);
	if (IAC == nullptr)
	{
		return;
	}

	IAC->DetachItemFromSocket(InSlot->GetItemId());
}