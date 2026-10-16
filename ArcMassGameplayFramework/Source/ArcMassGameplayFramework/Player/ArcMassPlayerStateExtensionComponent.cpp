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

#include "Player/ArcMassPlayerStateExtensionComponent.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Core/ArcMassGameFrameworkTags.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Pawn/ArcMassPawnData.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "Player/ArcMassPlayerState.h"

const FName UArcMassPlayerStateExtensionComponent::NAME_ActorFeatureName(TEXT("PlayerStateExtension"));

UArcMassPlayerStateExtensionComponent::UArcMassPlayerStateExtensionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UArcMassPlayerStateExtensionComponent::OnRegister()
{
	Super::OnRegister();

	RegisterInitStateFeature();
}

void UArcMassPlayerStateExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Listen for changes to all features on this actor.
	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);

	// Bind to the owning PS's pawn lifecycle so we can gate DataInitialized on
	// the pawn (and its PawnExtension feature) being ready.
	if (APlayerState* PS = GetOwner<APlayerState>())
	{
		PS->OnPawnSet.AddDynamic(this, &UArcMassPlayerStateExtensionComponent::HandleOwnerPawnSet);

		if (APawn* ExistingPawn = PS->GetPawn())
		{
			BindToPawn(ExistingPawn);
		}
	}

	// Notify state manager that we have spawned, then try to advance the rest of the chain.
	ensure(TryToChangeInitState(FArcMassGameFrameworkTags::Get().InitState_Spawned));
	CheckDefaultInitialization();
}

void UArcMassPlayerStateExtensionComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (APlayerState* PS = GetOwner<APlayerState>())
	{
		PS->OnPawnSet.RemoveDynamic(this, &UArcMassPlayerStateExtensionComponent::HandleOwnerPawnSet);
	}
	UnbindFromPawn();

	UnregisterInitStateFeature();
	Super::EndPlay(EndPlayReason);
}

bool UArcMassPlayerStateExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager
															   , FGameplayTag CurrentState
															   , FGameplayTag DesiredState) const
{
	check(Manager);

	const FArcMassGameFrameworkTags& Tags = FArcMassGameFrameworkTags::Get();

	const APlayerState* PS = GetOwner<APlayerState>();

	if (!CurrentState.IsValid() && DesiredState == Tags.InitState_Spawned)
	{
		// Valid as long as we have a PlayerState owner.
		return (PS != nullptr);
	}
	else if (CurrentState == Tags.InitState_Spawned && DesiredState == Tags.InitState_DataAvailable)
	{
		// Gate on PawnData being assigned on the owning PlayerState.
		// AArcMassPlayerState::CheckDataReady drives this via CheckDefaultInitialization()
		// on the PS extension once PawnData is non-null.
		if (PS == nullptr)
		{
			return false;
		}

		const AArcMassPlayerState* ArcPS = Cast<AArcMassPlayerState>(PS);
		if (ArcPS == nullptr || ArcPS->GetPawnData() == nullptr)
		{
			return false;
		}

		return true;
	}
	else if (CurrentState == Tags.InitState_DataAvailable && DesiredState == Tags.InitState_DataInitialized)
	{
		// All sibling features on the PS must have reached DataAvailable.
		if (!Manager->HaveAllFeaturesReachedInitState(GetOwner<APlayerState>(), Tags.InitState_DataAvailable))
		{
			return false;
		}

		// The owning PS must have a pawn, and that pawn's PawnExtension feature
		// must have reached DataInitialized. PS-side readiness implies pawn-side
		// data is ready for any sibling feature that consumes the pawn.
		APawn* Pawn = (PS != nullptr) ? PS->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			return false;
		}

		return Manager->HasFeatureReachedInitState(
			Pawn, UArcMassPawnExtensionComponent::NAME_ActorFeatureName, Tags.InitState_DataInitialized);
	}
	else if (CurrentState == Tags.InitState_DataInitialized && DesiredState == Tags.InitState_GameplayReady)
	{
		// All sibling features on the PS must have reached DataInitialized.
		return Manager->HaveAllFeaturesReachedInitState(GetOwner<APlayerState>(), Tags.InitState_DataInitialized);
	}

	return false;
}

void UArcMassPlayerStateExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager
																   , FGameplayTag CurrentState
																   , FGameplayTag DesiredState)
{
	// No per-transition work needed; InitState transitions themselves are the
	// readiness signal that sibling features bind via BindOnActorInitStateChanged.
}

void UArcMassPlayerStateExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
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

void UArcMassPlayerStateExtensionComponent::CheckDefaultInitialization()
{
	const TArray<FGameplayTag> StateChain = {
		FArcMassGameFrameworkTags::Get().InitState_Spawned
		, FArcMassGameFrameworkTags::Get().InitState_DataAvailable
		, FArcMassGameFrameworkTags::Get().InitState_DataInitialized
		, FArcMassGameFrameworkTags::Get().InitState_GameplayReady
	};

	ContinueInitStateChain(StateChain);
}

void UArcMassPlayerStateExtensionComponent::HandleOwnerPawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
	if (OldPawn != BoundPawn.Get() || NewPawn != BoundPawn.Get())
	{
		UnbindFromPawn();
	}

	if (NewPawn != nullptr)
	{
		BindToPawn(NewPawn);
	}

	CheckDefaultInitialization();
}

void UArcMassPlayerStateExtensionComponent::HandlePawnInitStateChanged(const FActorInitStateChangedParams& Params)
{
	CheckDefaultInitialization();
}

void UArcMassPlayerStateExtensionComponent::BindToPawn(APawn* Pawn)
{
	if (Pawn == nullptr || BoundPawn.Get() == Pawn)
	{
		return;
	}

	UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(Pawn);
	if (Manager == nullptr)
	{
		return;
	}

	BoundPawn = Pawn;
	PawnInitStateHandle = Manager->RegisterAndCallForActorInitState(
		Pawn,
		UArcMassPawnExtensionComponent::NAME_ActorFeatureName,
		FArcMassGameFrameworkTags::Get().InitState_DataInitialized,
		FActorInitStateChangedDelegate::CreateUObject(this, &UArcMassPlayerStateExtensionComponent::HandlePawnInitStateChanged),
		/*bCallImmediately*/ true);
}

void UArcMassPlayerStateExtensionComponent::UnbindFromPawn()
{
	APawn* Pawn = BoundPawn.Get();
	if (Pawn != nullptr && PawnInitStateHandle.IsValid())
	{
		if (UGameFrameworkComponentManager* Manager = UGameFrameworkComponentManager::GetForActor(Pawn))
		{
			Manager->UnregisterActorInitStateDelegate(Pawn, PawnInitStateHandle);
		}
	}
	PawnInitStateHandle.Reset();
	BoundPawn.Reset();
}

void UArcMassPlayerStateExtensionComponent::BroadcastOnPlayerStateReady()
{
	if (bPlayerStateReadyBroadcasted)
	{
		return;
	}
	bPlayerStateReadyBroadcasted = true;

	OnPlayerStateReadyDelegate.Broadcast();
}
