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

#include "ArcCore/Interaction/ArcInteractionAdvertiserComponent.h"
#include "ArcCore/Interaction/ArcInteractionReceiverComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

#include "AbilitySystemComponent.h"
#include "ArcCoreInteractionSubsystem.h"
#include "ArcInteractionStateChange.h"
#include "ArcInteractionStateChangePreset.h"
#include "SmartObjectComponent.h"
#include "SmartObjectSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "UObject/ObjectSaveContext.h"

UArcInteractionAdvertiserComponent::UArcInteractionAdvertiserComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UArcInteractionAdvertiserComponent::OnRegister()
{
	Super::OnRegister();
	
	if (bSetOwnerDormant)
	{
		GetOwner()->SetNetDormancy(ENetDormancy::DORM_DormantAll);	
	}
	
}

void UArcInteractionAdvertiserComponent::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);
	if (GetComponentLevel())
	{
		if (InteractionId.IsValid() == false)
		{
			InteractionId = FGuid::NewGuid();
		}
	}
}

void UArcInteractionAdvertiserComponent::SetInteractionInstigator(APawn* InPawn)
{
	CurrentInteractingPawn = InPawn;

	if (CurrentInteractingPawn != nullptr)
	{
		bIsClaimed = false;
	}
	else
	{
		bIsClaimed = true;	
	}
	
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bIsClaimed, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, CurrentInteractingPawn, this);
	
	GetOwner()->FlushNetDormancy();
	OnInteractionInstigatorSet(InPawn);
}

bool UArcInteractionAdvertiserComponent::IsClaimed(APawn* InPawnTrying) const
{
	if (bAllowClaim == false)
	{
		return false;
	}
	
	// If we have SmartObject rely on it's slots state.
	if (SmartObjectComponent.IsValid() == true)
	{
		return bIsClaimed;	
	}

	if (CurrentInteractingPawn == nullptr)
	{
		return true;
	}

	if (CurrentInteractingPawn == InPawnTrying)
	{
		return true;
	}

	return false;
}

void UArcInteractionAdvertiserComponent::OnSelected(APawn* InPawn
	, AController* InController
	, UArcInteractionReceiverComponent* InReceiver) const
{
	//for (const FInstancedStruct& IS : InteractionStatePreset->StateChangeActions)
	//{
	//	const FArcInteractionStateChange* Ptr = IS.GetPtr<FArcInteractionStateChange>();
	//	Ptr->OnSelected(InPawn, InController, InReceiver, this);
	//}
}

void UArcInteractionAdvertiserComponent::OnDeselected(APawn* InPawn
	, AController* InController
	, UArcInteractionReceiverComponent* InReceiver) const
{
	//for (const FInstancedStruct& IS : InteractionStatePreset->StateChangeActions)
	//{
	//	const FArcInteractionStateChange* Ptr = IS.GetPtr<FArcInteractionStateChange>();
	//	Ptr->OnDeselected(InPawn, InController, InReceiver, this);
	//}
}

void UArcInteractionAdvertiserComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAllowClaim)
	{
		USmartObjectComponent* SOC = GetOwner()->FindComponentByClass<USmartObjectComponent>();
		if (SOC == nullptr)
		{
			//UE_LOG(LogTemp, Log, TEXT("Actor % does not have SmartObjectComponent"), *GetNameSafe(GetOwner()));
			return;
		}

		SmartObjectComponent = SOC;

	
		SmartObjectComponent->GetOnSmartObjectEventNative().AddUObject(this, &UArcInteractionAdvertiserComponent::HandleOnSmartObjectChanged);
	}
	
	if (GetOwner()->HasAuthority())
	{
		if (InteractionCustomData.IsValid() && LevelActor.IsValid())
		{
			void* Allocated = FMemory::Malloc(InteractionCustomData.GetScriptStruct()->GetCppStructOps()->GetSize()
				, InteractionCustomData.GetScriptStruct()->GetCppStructOps()->GetAlignment());
			InteractionCustomData.GetScriptStruct()->GetCppStructOps()->Construct(Allocated);
			InteractionCustomData.GetScriptStruct()->CopyScriptStruct(Allocated, InteractionCustomData.GetMemory());
			FArcCoreInteractionCustomData* Ptr = static_cast<FArcCoreInteractionCustomData*>(Allocated);

			void* Allocated_LevelActor = FMemory::Malloc(LevelActor.GetScriptStruct()->GetCppStructOps()->GetSize()
				, LevelActor.GetScriptStruct()->GetCppStructOps()->GetAlignment());
			LevelActor.GetScriptStruct()->GetCppStructOps()->Construct(Allocated_LevelActor);
			LevelActor.GetScriptStruct()->CopyScriptStruct(Allocated_LevelActor, LevelActor.GetMemory());
			FArcInteractionData_LevelActor* Ptr_LevelActor = static_cast<FArcInteractionData_LevelActor*>(Allocated_LevelActor);
			
			AActor* OwnerOwner = GetOwner()->GetOwner();
			Ptr_LevelActor->LevelActor = GetOwner();
			
			if (OwnerOwner == nullptr)
			{
				GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>()->AddInteraction(GetInteractionId(), {Ptr, Ptr_LevelActor});
			}
		}
		else
		{
			AActor* OwnerOwner = GetOwner()->GetOwner();
			if (OwnerOwner == nullptr)
			{
				GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>()->AddInteraction(GetInteractionId(), {});
			}
		}
	}
}

void UArcInteractionAdvertiserComponent::HandleOnSmartObjectChanged(const FSmartObjectEventData& EventData
	, const AActor* Interactor)
{
	if (EventData.Reason == ESmartObjectChangeReason::OnClaimed
		|| EventData.Reason == ESmartObjectChangeReason::OnReleased)
	{
		USmartObjectSubsystem* SOS = GetWorld()->GetSubsystem<USmartObjectSubsystem>();
		TArray<FSmartObjectSlotHandle> OutSlots;
		SOS->GetAllSlots(EventData.SmartObjectHandle, OutSlots);

		bool bCanBeClaimed = false;
		for (const FSmartObjectSlotHandle& SlotHandle : OutSlots)
		{
			// Early out since we found at least one valid slot which can be claimed.
			if (SOS->CanBeClaimed(SlotHandle))
			{
				bCanBeClaimed = true;
				break;
			}
		}

		bIsClaimed = bCanBeClaimed;
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bIsClaimed, this);
		GetOwner()->FlushNetDormancy();
	}
}

void UArcInteractionAdvertiserComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass
		, CurrentInteractingPawn
		, Params);

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass
		, bIsClaimed
		, Params);
	
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}


