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

#include "Interaction/ArcInteractionReceiverComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "ArcInteractionLevelPlacedComponent.h"

#include "ArcInteractionRepresentation.h"
#include "ArcInteractionSearch.h"
#include "ArcInteractionStateChange.h"
#include "ArcInteractionStateChangePreset.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "Engine/LocalPlayer.h"

#include "CommonUserWidget.h"
#include "PrimaryGameLayout.h"
#include "Net/UnrealNetwork.h"

void UArcInteractionReceiverComponent::OnRep_CurrentTarget(AActor* OldTarget)
{
}

UArcInteractionReceiverComponent::UArcInteractionReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UArcInteractionReceiverComponent::BeginPlay()
{
	Super::BeginPlay();

	InstigatorPawn = Cast<APawn>(GetOwner());

	if (SearchType.IsValid())
	{
		SearchType.Get<FArcInteractionSearch>().BeginSearch(this);	
	}
}

void UArcInteractionReceiverComponent::TickComponent(float DeltaTime
	, ELevelTick TickType
	, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn == nullptr)
	{
		return;
	}

	if (CurrentInteractionInterface)
	{
		const FVector::FReal Distance = FVector::Distance(Pawn->GetActorLocation(), CurrentInteractionLocation);
		if (Distance > 200)
		{
			EndInteraction();
		}
	}
}

void UArcInteractionReceiverComponent::InitializeOptionInstanceData(const UScriptStruct* InInstanceDataType)
{
	OptionInstancedData.Reset();

	OptionInstancedData.InitializeAs(InInstanceDataType);
}

void UArcInteractionReceiverComponent::StartInteraction(TScriptInterface<IArcInteractionObjectInterface> InteractionManager
	, const FHitResult& InInteractionHit)
{
	FGuid NewInteractionId = InteractionManager->GetInteractionId(InInteractionHit);
	FGuid OldTarget = CurrentInteractionId;
	TScriptInterface<IArcInteractionObjectInterface> OldCurrentInteractionInterface = CurrentInteractionInterface;
	if (OldTarget == NewInteractionId)
	{
		return;
	}
	
	CurrentInteractionId = NewInteractionId;
	CurrentInteractionInterface = InteractionManager;
	CurrentInteractionLocation = CurrentInteractionInterface->GetInteractionLocation(InInteractionHit);
	
	APawn* Pawn = Cast<APawn>(GetOwner());
	// if we have old target, we should also have interaction for it.
	APlayerController* PC = Pawn->GetController<APlayerController>();

	if (PC == nullptr)
	{
		return;
	}

	if (OldCurrentInteractionInterface)
	{
		UArcInteractionStateChangePreset* State = OldCurrentInteractionInterface->GetInteractionStatePreset();
		for (const FInstancedStruct& IS : State->StateChangeActions)
        {
        	const FArcInteractionStateChange* Ptr = IS.GetPtr<FArcInteractionStateChange>();
        	Ptr->OnDeselected(Pawn, PC, this, CurrentInteractionInterface, CurrentInteractionId);
        }
	}

	if (CurrentInteractionInterface != nullptr)
	{
		UArcInteractionStateChangePreset* State = CurrentInteractionInterface->GetInteractionStatePreset();
		for (const FInstancedStruct& IS : State->StateChangeActions)
		{
			const FArcInteractionStateChange* Ptr = IS.GetPtr<FArcInteractionStateChange>();
			Ptr->OnSelected(Pawn, PC, this, CurrentInteractionInterface, CurrentInteractionId);
		}
	}
}

void UArcInteractionReceiverComponent::EndInteraction()
{
	if (CurrentInteractionInterface == nullptr)
	{
		return;
	}

	if (CurrentInteractionId.IsValid() == false)
	{
		return;
	}
	
	APawn* Pawn = Cast<APawn>(GetOwner());
	// if we have old target, we should also have interaction for it.
	APlayerController* PC = Pawn->GetController<APlayerController>();
	
	if (CurrentInteractionInterface)
	{
		UArcInteractionStateChangePreset* State = CurrentInteractionInterface->GetInteractionStatePreset();
		for (const FInstancedStruct& IS : State->StateChangeActions)
		{
			const FArcInteractionStateChange* Ptr = IS.GetPtr<FArcInteractionStateChange>();
			Ptr->OnDeselected(Pawn, PC, this, CurrentInteractionInterface, CurrentInteractionId);
		}
		
		CurrentInteractionComponent = nullptr;
		CurrentInteractionId.Invalidate();
	}
}
