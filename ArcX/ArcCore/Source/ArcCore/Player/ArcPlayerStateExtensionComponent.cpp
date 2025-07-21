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

#include "ArcPlayerStateExtensionComponent.h"
#include "GameFramework/Pawn.h"

#include "ArcCoreGameplayTags.h"
#include "Player/ArcCorePlayerState.h"
#include "Components/GameFrameworkComponentManager.h"

const FName UArcPlayerStateExtensionComponent::NAME_PlayerStateInitialized("PlayerStateInitialized");

// Sets default values for this component's properties
UArcPlayerStateExtensionComponent::UArcPlayerStateExtensionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every
	// frame.  You can turn these features off to improve performance if you don't need
	// them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

bool UArcPlayerStateExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager
													, FGameplayTag CurrentState
													, FGameplayTag DesiredState) const
{
	check(Manager);

	APawn* Pawn = GetPawn<APawn>();
	APlayerController* PC = GetPlayerState<APlayerState>()->GetPlayerController();
	const FArcCoreGameplayTags& InitTags = FArcCoreGameplayTags::Get();

	if (!CurrentState.IsValid() && DesiredState == InitTags.InitState_Spawned)
	{
		// As long as we are on a valid pawn, we count as spawned
		return true;
	}
	else if (CurrentState == InitTags.InitState_Spawned && DesiredState == InitTags.InitState_DataAvailable)
	{
		return true;
	}
	else if (CurrentState == InitTags.InitState_DataAvailable && DesiredState == InitTags.InitState_DataInitialized)
	{
		return true;
	}
	else if (CurrentState == InitTags.InitState_DataInitialized && DesiredState == InitTags.InitState_GameplayReady)
	{
		if (Pawn && PC)
    	{
    		return true;
    	}
	}
	
	return false;
}

void UArcPlayerStateExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager
													   , FGameplayTag CurrentState
													   , FGameplayTag DesiredState)
{
	//if (DesiredState == FArcCoreGameplayTags::Get().InitState_GameplayReady)
	//{
	//	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(GetPlayerState<APlayerState>(), NAME_PlayerStateInitialized);
	//}
}

void UArcPlayerStateExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureState == FArcCoreGameplayTags::Get().InitState_GameplayReady)
	{
		UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(GetPlayerState<APlayerState>(), NAME_PlayerStateInitialized);
	}
}

void UArcPlayerStateExtensionComponent::CheckDefaultInitialization()
{
	// Before checking our progress, try progressing any other features we might depend on
	CheckDefaultInitializationForImplementers();

	const FArcCoreGameplayTags& InitTags = FArcCoreGameplayTags::Get();
	static const TArray<FGameplayTag> StateChain = {
		InitTags.InitState_Spawned
		, InitTags.InitState_DataAvailable
		, InitTags.InitState_DataInitialized
		, InitTags.InitState_GameplayReady
	};

	// This will try to progress from spawned (which is only set in BeginPlay) through the
	// data initialization stages until it gets to gameplay ready
	ContinueInitStateChain(StateChain);
}


// Called when the game starts
void UArcPlayerStateExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Listen for changes to all features
	BindOnActorInitStateChanged(NAME_None
		, FGameplayTag()
		, false);

	// Notifies state manager that we have spawned, then try rest of default
	// initialization
	TryToChangeInitState(FArcCoreGameplayTags::Get().InitState_Spawned);
	CheckDefaultInitialization();
	
	// ...
}

void UArcPlayerStateExtensionComponent::PlayerStateInitialized()
{
	OnPlayerStateReadyDelegate.Broadcast();
	//HandlePlayerStateReady();
}

void UArcPlayerStateExtensionComponent::PlayerPawnInitialized(APawn* InPawn)
{
	OnPawnReadyDelegate.Broadcast(InPawn);
}

void UArcPlayerStateExtensionComponent::HandlePlayerStateReady()
{
	if(bPlayerStateInitialized == true)
	{
		return;
	}
	bPlayerStateInitialized = true;
}

void UArcPlayerStateExtensionComponent::HandlePlayerStateDataReplicated()
{
	HandlePlayerStateReady();
}
