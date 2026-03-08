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

#include "Animation/ArcANS_MeleeTraceWindow.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UArcANS_MeleeTraceWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
											UAnimSequenceBase* Animation,
											float TotalDuration,
											const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	FireEventOnASC(MeshComp, BeginEventTag);
}

void UArcANS_MeleeTraceWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
										  UAnimSequenceBase* Animation,
										  const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	FireEventOnASC(MeshComp, EndEventTag);
}

void UArcANS_MeleeTraceWindow::FireEventOnASC(USkeletalMeshComponent* MeshComp,
											   const FGameplayTag& EventTag) const
{
	if (!MeshComp || !EventTag.IsValid())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner))
	{
		if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			FGameplayEventData EventData;
			EventData.Instigator = Owner;
			ASC->HandleGameplayEvent(EventTag, &EventData);
		}
	}
}
