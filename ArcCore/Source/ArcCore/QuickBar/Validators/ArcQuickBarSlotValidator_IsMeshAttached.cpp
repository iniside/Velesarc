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



#include "ArcQuickBarSlotValidator_IsMeshAttached.h"

#include "GameFramework/Actor.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "Items/ArcItemData.h"
#include "QuickBar/ArcQuickBarComponent.h"

bool FArcQuickBarSlotValidator_IsMeshAttached::IsValid(UArcQuickBarComponent* InQuickBarComp
													   , const FArcQuickBar& InQuickBar
													   , const FArcQuickBarSlot& InSlot) const
{
	ENetMode NM = InQuickBarComp->GetNetMode();

	if (NM == ENetMode::NM_DedicatedServer)
	{
		return true;
	}

	UArcItemAttachmentComponent* IAC = InQuickBarComp->GetOwner()->FindComponentByClass<UArcItemAttachmentComponent>();

	if (IAC == nullptr)
	{
		return true;
	}

	const FArcItemData* Slot = InQuickBarComp->FindQuickSlotItem(InQuickBar.BarId, InSlot.QuickBarSlotId);
	if (Slot == nullptr)
	{
		return true;
	}
	bool bIsAttached = IAC->DoesSlotHaveAttachedActor(Slot->GetSlotId());

	return bIsAttached;
}
