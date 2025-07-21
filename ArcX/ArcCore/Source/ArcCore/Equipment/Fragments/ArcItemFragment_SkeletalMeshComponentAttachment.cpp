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



#include "ArcItemFragment_SkeletalMeshComponentAttachment.h"


#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"

#include "Equipment/ArcEquipmentUtils.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

#include "Items/ArcItemDefinition.h"

bool FArcAttachmentHandler_SkeletalMeshComponent::HandleItemAddedToSlot(UArcItemAttachmentComponent* InAttachmentComponent
															   , const FArcItemData* InItem
															   , const FArcItemData* InOwnerItem) const
{
	UArcItemDefinition* VisualItemDefinition = nullptr;
	
	// Items can be attached and/or grant attachment sockets.
	const FArcItemFragment_SkeletalMeshComponentAttachment* Fragment = ArcEquipmentUtils::GetAttachmentFragment<FArcItemFragment_SkeletalMeshComponentAttachment>(InItem, VisualItemDefinition);
	if (Fragment == nullptr && VisualItemDefinition == nullptr)
	{
		return false;
	}

	APlayerState* PS = Cast<APlayerState>(InAttachmentComponent->GetOwner());
	ACharacter* C = PS->GetPawn<ACharacter>();
	
	if (C == nullptr)
	{
		return false;
	}
	
	FArcItemAttachment Attached = MakeItemAttachment(InAttachmentComponent, InItem, InOwnerItem);
	
	return true;
}

void FArcAttachmentHandler_SkeletalMeshComponent::HandleItemAttach(UArcItemAttachmentComponent* InAttachmentComponent
	, const FArcItemId InItemId
	, const FArcItemId InOwnerItem) const
{
	const FArcItemAttachment* ItemAttachment = InAttachmentComponent->GetAttachment(InItemId);
	
	UObject* AttachedObject = InAttachmentComponent->FindFirstAttachedObject(ItemAttachment->ItemDefinition);
	if (AttachedObject)
	{
		return;
	}
	
	UObject* ExistingAttachment = InAttachmentComponent->FindFirstAttachedObject(ItemAttachment->ItemDefinition);
	if (ExistingAttachment != nullptr)
	{
		return;
	}

	const FArcItemFragment_SkeletalMeshComponentAttachment* Fragment = FindAttachmentFragment<FArcItemFragment_SkeletalMeshComponentAttachment>(*ItemAttachment);
	if (!Fragment)
	{
		return;
	}
	
	TSubclassOf<USkeletalMeshComponent> SkeletalMeshComponent = Fragment->SkeletalMeshComponentClass.Get();
	if (!SkeletalMeshComponent)
	{
		return;
	}
	
	FArcItemAttachment* LocalAttachment = const_cast<FArcItemAttachment*>(InAttachmentComponent->GetAttachment(InItemId));
	
	ACharacter* OwnerCharacter = InAttachmentComponent->FindCharacter();

	
	
	USkeletalMeshComponent* SpawnedComponent = SpawnComponent<USkeletalMeshComponent>(OwnerCharacter, InAttachmentComponent, ItemAttachment, SkeletalMeshComponent);
	
	if (SpawnedComponent == nullptr)
	{
		return;
	}

	USceneComponent* ParentComponent = FindAttachmentComponent(InAttachmentComponent, ItemAttachment);
	
	if (TSubclassOf<UAnimInstance> AnimInstance = Fragment->SkeletalMeshAnimInstance.Get())
	{
		SpawnedComponent->SetAnimInstanceClass(AnimInstance);
	}

	if (Fragment->bUseLeaderPose)
	{
		if (USkeletalMeshComponent* MasterPoseComp = Cast<USkeletalMeshComponent>(ParentComponent))
		{
			SpawnedComponent->SetLeaderPoseComponent(MasterPoseComp);	
		}
	}
	
	InAttachmentComponent->AddAttachmentForItem(ItemAttachment->ItemDefinition, SpawnedComponent);
}
