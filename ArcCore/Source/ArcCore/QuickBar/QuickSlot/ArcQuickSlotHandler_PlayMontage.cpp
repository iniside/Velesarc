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



#include "ArcQuickSlotHandler_PlayMontage.h"
#include "Animation/AnimMontage.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "QuickBar/ArcQuickBarComponent.h"

void FArcQuickSlotHandler_PlayMontage::OnSlotSelected(UArcCoreAbilitySystemComponent* InArcASC
													  , UArcQuickBarComponent* QuickBar
													  , const FArcItemData* InSlot
													  , const FArcQuickBar* InQuickBar
													  , const FArcQuickBarSlot* InQuickSlot) const
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

	if (SelectedMontageToPlay.IsNull() == true)
	{
		return;
	}
	
	C->GetMesh()->GetAnimInstance()->Montage_Play(SelectedMontageToPlay.LoadSynchronous());

	if (InArcASC->GetOwner()->HasAuthority() && InArcASC->GetNetMode() != NM_Standalone)
	{
		if (bReliable)
		{
			InArcASC->MulticastPlaySoftMontageReliable(SelectedMontageToPlay);
		}
		else
		{
			InArcASC->MulticastPlaySoftMontageUnreliable(SelectedMontageToPlay);
		}
	}
}

void FArcQuickSlotHandler_PlayMontage::OnSlotDeselected(UArcCoreAbilitySystemComponent* InArcASC
														, UArcQuickBarComponent* QuickBar
														, const FArcItemData* InSlot
														, const FArcQuickBar* InQuickBar
														, const FArcQuickBarSlot* InQuickSlot) const
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

	if (DeselectedMontageToPlay.IsNull() == true)
	{
		return;
	}
	
	C->GetMesh()->GetAnimInstance()->Montage_Play(DeselectedMontageToPlay.LoadSynchronous());
	
	if (InArcASC->GetOwner()->HasAuthority() && InArcASC->GetNetMode() != NM_Standalone)
	{
		if (bReliable)
		{
			InArcASC->MulticastPlaySoftMontageReliable(DeselectedMontageToPlay);
		}
		else
		{
			InArcASC->MulticastPlaySoftMontageUnreliable(DeselectedMontageToPlay);
		}
	}
}