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



#include "ArcAN_SendGameplayEvent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"

UArcAN_SendGameplayEvent::UArcAN_SendGameplayEvent(const FObjectInitializer& O)
	: Super(O)
{
};

FString UArcAN_SendGameplayEvent::GetNotifyName_Implementation() const
{
	return "Arc Send Gameplay Event";
}

void UArcAN_SendGameplayEvent::Notify(USkeletalMeshComponent* MeshComp
									  , UAnimSequenceBase* Animation
									  , const FAnimNotifyEventReference& EventReference)
{
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(MeshComp->GetOwner());
	if (ASI && ASI->GetAbilitySystemComponent())
	{
		FGameplayEventData Payload;
		ASI->GetAbilitySystemComponent()->HandleGameplayEvent(EventTag
			, &Payload);
	}
}

void UArcAN_SendGameplayEvent::BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(BranchingPointPayload.SkelMeshComponent->GetOwner());
	if (ASI && ASI->GetAbilitySystemComponent())
	{
		FGameplayEventData Payload;
		ASI->GetAbilitySystemComponent()->HandleGameplayEvent(EventTag
			, &Payload);
	}
}

#if WITH_EDITOR
void UArcAN_SendGameplayEvent::ValidateAssociatedAssets()
{
	Super::ValidateAssociatedAssets();
}
#endif

UArcAnimNotify_MarkGameplayEvent::UArcAnimNotify_MarkGameplayEvent(const FObjectInitializer& O)
	: Super(O)
{
};


FString UArcAnimNotify_MarkGameplayEvent::GetNotifyName_Implementation() const
{
	return "Mark Gameplay Event";
}

void UArcAnimNotify_MarkGameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation
											  , const FAnimNotifyEventReference& EventReference)
{
	// just do nothing.
}

void UArcAnimNotify_MarkGameplayEvent::BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotify(BranchingPointPayload);
}
#if WITH_EDITOR
void UArcAnimNotify_MarkGameplayEvent::ValidateAssociatedAssets()
{
	Super::ValidateAssociatedAssets();
}

UArcAnimNotify_SetPlayRate::UArcAnimNotify_SetPlayRate(const FObjectInitializer& O)
	: Super(O)
{
};

FString UArcAnimNotify_SetPlayRate::GetNotifyName_Implementation() const
{
	return "Set Play Rate";
}

void UArcAnimNotify_SetPlayRate::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation
	, const FAnimNotifyEventReference& EventReference)
{
	
}

void UArcAnimNotify_SetPlayRate::BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotify(BranchingPointPayload);
}

void UArcAnimNotify_SetPlayRate::ValidateAssociatedAssets()
{
	Super::ValidateAssociatedAssets();
}
#endif
