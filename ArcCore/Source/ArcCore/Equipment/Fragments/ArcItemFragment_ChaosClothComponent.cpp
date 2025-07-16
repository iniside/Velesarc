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



#include "ArcItemFragment_ChaosClothComponent.h"

#include "ComponentReregisterContext.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothAsset.h"

#include "Components/SkeletalMeshComponent.h"
#include "Dataflow/DataflowSimulationManager.h"
#include "Equipment/ArcEquipmentUtils.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsHelpers.h"

FArcItemFragment_ChaosClothComponent::FArcItemFragment_ChaosClothComponent()
{
	ClothComponentClass = UChaosClothComponent::StaticClass();
}


bool FArcAttachmentHandler_ChaosCloth::HandleItemAddedToSlot(UArcItemAttachmentComponent* InAttachmentComponent
															   , const FArcItemData* InItem
															   , const FArcItemData* InOwnerItem) const
{
	UArcItemDefinition* VisualItemDefinition = nullptr;
	
	// Items can be attached and/or grant attachment sockets.
	const FArcItemFragment_ChaosClothComponent* Fragment = ArcEquipmentUtils::GetAttachmentFragment<FArcItemFragment_ChaosClothComponent>(InItem, VisualItemDefinition);
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
	
	InAttachmentComponent->AddAttachedItem(Attached);
	
	return true;
}

void FArcAttachmentHandler_ChaosCloth::HandleItemRemovedFromSlot(UArcItemAttachmentComponent* InAttachmentComponent
	, const FArcItemData* InItem
	, const FGameplayTag& SlotId
	, const FArcItemData* InOwnerItem) const
{
	InAttachmentComponent->RemoveAttachment(InItem->GetItemId());
}

void FArcAttachmentHandler_ChaosCloth::HandleItemAttach(UArcItemAttachmentComponent* InAttachmentComponent
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

	const FArcItemFragment_ChaosClothComponent* Fragment = FindAttachmentFragment<FArcItemFragment_ChaosClothComponent>(*ItemAttachment);

	if (Fragment == nullptr)
	{
		return;
	}
	
	TSubclassOf<UChaosClothComponent> ClothComponentClass = Fragment->ClothComponentClass.LoadSynchronous();
	if (!ClothComponentClass)
	{
		return;
	}
	
	ACharacter* OwnerCharacter = InAttachmentComponent->FindCharacter();

	UChaosClothComponent* SpawnedComponent = OwnerCharacter->FindComponentByClass<UChaosClothComponent>();
		
	if (SpawnedComponent == nullptr)
	{
		SpawnedComponent = SpawnComponent<UChaosClothComponent>(OwnerCharacter, InAttachmentComponent, ItemAttachment, ClothComponentClass);
	}

	if (SpawnedComponent == nullptr)
	{
		return;
	}
	
	SpawnedComponent->SetWorldScale3D(FVector(WorldScale));
	if (UChaosClothAsset* Loaded = Fragment->ClothAsset.LoadSynchronous())
	{
		Loaded->CreateDataflowContent();
		SpawnedComponent->SetAsset(Loaded);
		//SpawnedComponent->ResetConfigProperties();
		//SpawnedComponent->RecreateClothSimulationProxy();

		const FComponentReregisterContext Context(SpawnedComponent);
		
		//USkeletalMeshComponent* c = Cast<USkeletalMeshComponent>(OwnerCharacter->GetMesh());
		//c->RecreateClothingActors();
		//USkeletalMeshComponent* p = Cast<USkeletalMeshComponent>(SpawnedComponent->GetAttachParent());
		//p->RecreateClothingActors();
	}
	
	InAttachmentComponent->AddAttachmentForItem(ItemAttachment->ItemDefinition, SpawnedComponent);
}

void FArcAttachmentHandler_ChaosCloth::HandleItemDetach(UArcItemAttachmentComponent* InAttachmentComponent
	, const FArcItemId InItemId
	, const FArcItemId InOwnerItem) const
{
	if (InAttachmentComponent->GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		const FArcItemAttachment* Attachment = InAttachmentComponent->GetAttachment(InItemId);
		if (Attachment == nullptr)
		{
			return;
		}

		const FArcItemFragment_ChaosClothComponent* Fragment =  Attachment->ItemDefinition->FindFragment<FArcItemFragment_ChaosClothComponent>();
		if (!Fragment)
		{
			return;
		}
		TArray<UObject*> AttachedObjects = InAttachmentComponent->FindAttachedObjectsOfClass(Attachment->ItemDefinition, Fragment->ClothComponentClass.Get());
		
		for (UObject* AttachedObject : AttachedObjects)
		{
			if (UChaosClothComponent* CCC = Cast<UChaosClothComponent>(AttachedObject))
			{
				CCC->SetHiddenInGame(true);
				CCC->DetachFromParent();
				CCC->DestroyComponent();		
			}
		}
	}
}
