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

#include "AbilitySystem/Combo/ArcAnimNotifyState_ComboWindow.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

#include "AbilitySystem/Combo/ArcComboComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UArcAnimNotify_ComboEvent::Notify(USkeletalMeshComponent* MeshComp
									   , UAnimSequenceBase* Animation
									   , const FAnimNotifyEventReference& EventReference)
{
	if (APawn* C = Cast<APawn>(MeshComp->GetOwner()))
	{
		if (APlayerState* PS = C->GetPlayerState())
		{
			UArcComboComponent* ArcCombo = PS->FindComponentByClass<UArcComboComponent>();
			if (ArcCombo)
			{
				ArcCombo->OnComboEventDelegate.Broadcast(EventTag);
			}
		}
	}
	// if(IAbilitySystemInterface* ASI =
	// Cast<IAbilitySystemInterface>(MeshComp->GetOwner()))
	//{
	//	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	//	FGameplayEventData EventData;
	//	EventData.Instigator = MeshComp->GetOwner();
	//	ASC->GetOwnedGameplayTags(EventData.InstigatorTags);
	//
	//	ASC->HandleGameplayEvent(EventTag, &EventData);
	// }
}

void UArcAnimNotify_ComboWindowStart::Notify(USkeletalMeshComponent* MeshComp
											 , UAnimSequenceBase* Animation
											 , const FAnimNotifyEventReference& EventReference)
{
	if (APawn* C = Cast<APawn>(MeshComp->GetOwner()))
	{
		if (APlayerState* PS = C->GetPlayerState())
		{
			UArcComboComponent* ArcCombo = PS->FindComponentByClass<UArcComboComponent>();
			if (ArcCombo)
			{
				ArcCombo->OnComboWindowDelegate.Broadcast(true);
				ArcCombo->BroadcastComboWindowStart();
			}
		}
	}
}

void UArcAnimNotify_ComboWindowEnd::Notify(USkeletalMeshComponent* MeshComp
										   , UAnimSequenceBase* Animation
										   , const FAnimNotifyEventReference& EventReference)
{
	if (APawn* C = Cast<APawn>(MeshComp->GetOwner()))
	{
		if (APlayerState* PS = C->GetPlayerState())
		{
			UArcComboComponent* ArcCombo = PS->FindComponentByClass<UArcComboComponent>();
			if (ArcCombo)
			{
				ArcCombo->OnComboWindowDelegate.Broadcast(false);
				ArcCombo->BroadcastComboWindowEnd();
			}
		}
	}
}
