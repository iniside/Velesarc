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

#include "Player/ArcMassPlayerControllerExtensionComponent.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Core/ArcMassGameFrameworkTags.h"
#include "GameFramework/PlayerController.h"

const FName UArcMassPlayerControllerExtensionComponent::NAME_ActorFeatureName(TEXT("PlayerControllerExtension"));

UArcMassPlayerControllerExtensionComponent::UArcMassPlayerControllerExtensionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UArcMassPlayerControllerExtensionComponent::OnRegister()
{
	Super::OnRegister();

	RegisterInitStateFeature();
}

void UArcMassPlayerControllerExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Listen for changes to all features on this actor.
	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);

	// Notify state manager that we have spawned, then try to advance the rest of the chain.
	ensure(TryToChangeInitState(FArcMassGameFrameworkTags::Get().InitState_Spawned));
	CheckDefaultInitialization();
}

void UArcMassPlayerControllerExtensionComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	UnregisterInitStateFeature();
	Super::EndPlay(EndPlayReason);
}

bool UArcMassPlayerControllerExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager
																	, FGameplayTag CurrentState
																	, FGameplayTag DesiredState) const
{
	check(Manager);

	const FArcMassGameFrameworkTags& Tags = FArcMassGameFrameworkTags::Get();

	const APlayerController* PC = GetOwner<APlayerController>();

	if (!CurrentState.IsValid() && DesiredState == Tags.InitState_Spawned)
	{
		// Valid as long as we have a PlayerController owner.
		return (PC != nullptr);
	}
	else if (CurrentState == Tags.InitState_Spawned && DesiredState == Tags.InitState_DataAvailable)
	{
		// Gate on the PC being valid and either having authority or being locally controlled.
		if (PC == nullptr)
		{
			return false;
		}

		return (PC->HasAuthority() || PC->IsLocalController());
	}
	else if (CurrentState == Tags.InitState_DataAvailable && DesiredState == Tags.InitState_DataInitialized)
	{
		// All sibling features on the PC must have reached DataAvailable.
		return Manager->HaveAllFeaturesReachedInitState(GetOwner<APlayerController>(), Tags.InitState_DataAvailable);
	}
	else if (CurrentState == Tags.InitState_DataInitialized && DesiredState == Tags.InitState_GameplayReady)
	{
		// All sibling features on the PC must have reached DataInitialized.
		return Manager->HaveAllFeaturesReachedInitState(GetOwner<APlayerController>(), Tags.InitState_DataInitialized);
	}

	return false;
}

void UArcMassPlayerControllerExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager
																	   , FGameplayTag CurrentState
																	   , FGameplayTag DesiredState)
{
	// No active work needed at this step.
}

void UArcMassPlayerControllerExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	// When any sibling feature reaches DataAvailable or DataInitialized, re-attempt
	// advancing our own chain.
	if (Params.FeatureName != NAME_ActorFeatureName)
	{
		const FArcMassGameFrameworkTags& Tags = FArcMassGameFrameworkTags::Get();
		if (Params.FeatureState == Tags.InitState_DataAvailable
			|| Params.FeatureState == Tags.InitState_DataInitialized)
		{
			CheckDefaultInitialization();
		}
	}
}

void UArcMassPlayerControllerExtensionComponent::CheckDefaultInitialization()
{
	const TArray<FGameplayTag> StateChain = {
		FArcMassGameFrameworkTags::Get().InitState_Spawned
		, FArcMassGameFrameworkTags::Get().InitState_DataAvailable
		, FArcMassGameFrameworkTags::Get().InitState_DataInitialized
		, FArcMassGameFrameworkTags::Get().InitState_GameplayReady
	};

	ContinueInitStateChain(StateChain);
}
