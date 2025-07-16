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

#include "ArcAnimNotify_Unholster.h"

#include "ArcGunStateComponent.h"
#include "Components/SkeletalMeshComponent.h"


#include "Equipment/ArcItemAttachmentComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"

void UArcAnimNotify_Unholster::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	
	UArcGunStateComponent* ArcWC = UArcGunStateComponent::FindGunStateComponent(MeshComp->GetOwner());
	if (ArcWC == nullptr)
	{
		ACharacter* C = Cast<ACharacter>(MeshComp->GetOwner());
		if (C)
		{
			ArcWC = UArcGunStateComponent::FindGunStateComponent(C->GetPlayerState());
		}
	}
	
	if (ArcWC == nullptr)
	{
		return;
	}
	
	const FArcItemId& ItemId = ArcWC->GetEquippedWeaponItemId();

	if (ItemId.IsNone())
	{
		return;
	}
	
	const UArcItemDefinition* WeaponItemDef = ArcWC->GetWeaponItemDef();
	UArcItemAttachmentComponent* IAC = UArcItemAttachmentComponent::FindItemAttachmentComponent(ArcWC->GetOwner());

	//AActor* WeaponActor = IAC->GetAttachedActor(ItemData->GetItemId());
	//if(WeaponActor == nullptr)
	//{
	//	return;
	//}
	
	const FArcItemFragment_GunUnholster* WeaponAttachment = WeaponItemDef->FindFragment<FArcItemFragment_GunUnholster>();
	if(WeaponAttachment == nullptr)
	{
		//UE_LOG(LogArcWeaponComponent, Log, TEXT("AttachUnholster - Weapon attachment is invalid"));
		return;
	}

	IAC->AttachItemToSocket(ItemId, WeaponAttachment->UnholsterSocket, NAME_None, WeaponAttachment->Transform);
	IAC->LinkAnimLayer(ItemId);
}
