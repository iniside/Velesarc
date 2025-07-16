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

#include "Pawn/ArcPawnData.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"

#include "AbilitySystem/ArcAbilitySet.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Player/ArcCoreCharacter.h"
#include "Player/ArcCorePlayerState.h"
#include "Items/ArcItemsComponent.h"
#include "ArcCoreUtils.h"
#include "ArcLogs.h"
#include "AbilitySystem/ArcAbilityTargetingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"

#include "AbilitySystem/Targeting/ArcTargetingObject.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemsStoreComponent.h"
#include "QuickBar/ArcQuickBarComponent.h"

void FArcPawnInitializationFragment_SetSkeletalMesh::Initialize(APawn* InCharacter
																, AArcCorePlayerState* InPlayerState
																, AArcCorePlayerController* InPlayerController) const
{
	if (USkeletalMeshComponent* SMC = InCharacter->FindComponentByClass<USkeletalMeshComponent>())
	{
		USkeletalMesh* SM = SkeletalMesh.LoadSynchronous();
		SMC->SetSkeletalMesh(SM);

		//LOG_ARC_NET(
		//	LogPawnExtenstionComponent
		//	, "UArcHeroComponentBase::HandleChangeInitState [CurrentState %s] [DesiredState %s] Activate Skeletal "
		//	"Mesh"
		//	, *CurrentState.ToString()
		//	, *DesiredState.ToString())

		SMC->SetActive(true);
	}
}

void FArcPawnInitializationFragment_SetAnimBlueprint::Initialize(APawn* InCharacter
																 , AArcCorePlayerState* InPlayerState
																 , AArcCorePlayerController* InPlayerController) const
{
	UClass* AnimClass = AnimBlueprint.LoadSynchronous();
	USkeletalMeshComponent* SMC = InCharacter->FindComponentByClass<USkeletalMeshComponent>();
	if(SMC == nullptr)
	{
		return;
	}

	SMC->SetAnimInstanceClass(AnimClass);
	for (const TSoftClassPtr<UAnimInstance>& AL : AnimLayer)
	{
		TSubclassOf<UAnimInstance> AnimLayerLink = AL.LoadSynchronous();

		SMC->LinkAnimClassLayers(AnimLayerLink);
	}
}

void FArcPawnDataFragment_AbilitySets::GiveFragment(APawn* InCharacter
													, AArcCorePlayerState* InPlayerState) const
{
	UArcCoreAbilitySystemComponent* ASC = InPlayerState->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	if (ASC == nullptr)
	{
		ASC = InCharacter->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	}
	if (ASC)
	{
		for (const TSoftObjectPtr<UArcAbilitySet>& AbilitySet : AbilitySets)
		{
			if (AbilitySet.IsNull() == false)
			{
				AbilitySet.LoadSynchronous()->GiveToAbilitySystem(ASC
					, nullptr);
			}
		}
	}
}

void FArcPawnDataFragment_DefaultItems::GiveFragment(APawn* InCharacter
													 , AArcCorePlayerState* InPlayerState) const
{
	if (InPlayerState->HasAuthority() == false)
	{
		return;
	}
	UArcItemsStoreComponent* IC = Cast<UArcItemsStoreComponent>(InPlayerState->GetComponentByClass(Component));
	if (IC == nullptr)
	{
		IC = Cast<UArcItemsStoreComponent>(InCharacter->GetComponentByClass(Component));
		if (IC == nullptr)
		{
			return;
		}
	}

	if (IC)
	{
		for (const FArcDefaultItem& DSI : ItemsToAdd)
		{
			FArcItemSpec Spec;
			FPrimaryAssetId DefinitionId;
			DefinitionId = DSI.ItemDefinitionId;

			if (bUseItemDefinitionId)
			{
				FString StringGuid = DefinitionId.PrimaryAssetName.ToString();
				Spec.ItemId= FArcItemId(FGuid(StringGuid));
			}
			
			Spec.SetItemId(FArcItemId::Generate()).SetItemDefinition(DefinitionId).SetAmount(DSI.Amount);

			FArcItemId AddedItemId = IC->AddItem(Spec, FArcItemId());
		}
	}
}

void FArcPawnDataFragment_DefaultItemsOnSlot::GiveFragment(APawn* InCharacter
														   , AArcCorePlayerState* InPlayerState) const
{
	if (InPlayerState->HasAuthority() == false)
	{
		return;
	}
	UArcItemsStoreComponent* IC = Arcx::Utils::GetComponent(InPlayerState
		, ItemComponent);
	if (IC)
	{
		for (const FArcDefaultSlotItem& DSI : ItemsToAdd)
		{
			if (IC)
			{
				FPrimaryAssetId DefinitionId;
				DefinitionId = DSI.ItemDefinitionId;
				
				FArcItemSpec Spec;
				Spec.SetItemId(FArcItemId::Generate()).SetItemDefinition(DefinitionId).SetAmount(1);
				FArcItemId AddedItemId = IC->AddItem(Spec, FArcItemId());
				if (AddedItemId.IsValid() && DSI.Slot.IsValid())
				{
					IC->AddItemToSlot(AddedItemId, DSI.Slot);
				}
			}
		}
		return;
	}
}

void FArcPawnDataFragment_AddItemsToQuickbar::GiveFragment(class APawn* InCharacter
	, class AArcCorePlayerState* InPlayerState) const
{
	UArcQuickBarComponent* QBC = Arcx::Utils::GetComponent(InPlayerState, QuickBarComponent);
	for (const FArcDefaultQuickSlotItem& DSI : ItemsToAdd)
	{
		UArcItemsStoreComponent* IC = QBC->GetItemStoreComponent(DSI.BarId);
		if (!IC || !QBC)
		{
			continue;
		}
		
		FPrimaryAssetId DefinitionId;
		DefinitionId = DSI.ItemDefinitionId;
		
		FArcItemSpec Spec;
		Spec.SetItemId(FArcItemId::Generate()).SetItemDefinition(DefinitionId).SetAmount(1);
		FArcItemId AddedItemId = IC->AddItem(Spec, FArcItemId());
		if (AddedItemId.IsValid() && DSI.ItemSlot.IsValid())
		{
			IC->AddItemToSlot(AddedItemId, DSI.ItemSlot);
		}

		QBC->AddItemToBarOrRegisterDelegate(DSI.BarId, DSI.QuickSlot, AddedItemId);
	}
}

void FArcAddPawnComponent_AddSingle::GiveComponent(class APawn* InPawn) const
{
	/**
	 * If anyone wonder, spawning on server and replicating component
	 * will not cause to replicate entire object. Just Replicate it's NetId
	 * And then dynamically spawn if component does not exists.
	 *
	 * NetAdressable in this case causes issues, because we spawn components way to late to be of any use
	 * for stable naming.
	 */
	if (bSpawnOnServer == true)
	{
		if (InPawn->HasAuthority() == true)
		{
			UActorComponent* Comp = NewObject<UActorComponent>(InPawn
				, ComponentClass
				, ComponentClass->GetFName());
			Comp->RegisterComponentWithWorld(InPawn->GetWorld());
			Comp->SetIsReplicated(bReplicate);
		}
	}

	if (bSpawnOnClient == true)
	{
		if (InPawn->HasAuthority() == false)
		{
			UActorComponent* Comp = NewObject<UActorComponent>(InPawn
				, ComponentClass
				, ComponentClass->GetFName());
			Comp->RegisterComponentWithWorld(InPawn->GetWorld());
			Comp->SetIsReplicated(bReplicate);
		}
	}
}

void UArcPawnData::GiveDataFragments(APawn* InCharacter
									 , AArcCorePlayerState* InPlayerState) const
{
	for (const FInstancedStruct& IS : Fragments)
	{
		IS.GetPtr<FArcPawnDataFragment>()->GiveFragment(InCharacter
			, InPlayerState);
	}
}

void UArcPawnData::GiveComponents(APawn* InCharacter) const
{
	for (const FInstancedStruct& IS : Components)
	{
		IS.GetPtr<FArcAddPawnComponent>()->GiveComponent(InCharacter);
	}
}

void UArcPawnData::RunInitializationFragments(class APawn* InCharacter
											  , class AArcCorePlayerState* InPlayerState
											  , class AArcCorePlayerController* InPlayerController) const
{
	for (const FInstancedStruct& IS : InitializationFragments)
	{
		IS.GetPtr<FArcPawnInitializationFragment>()->Initialize(InCharacter
			, InPlayerState
			, InPlayerController);
	}
}
