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

#include "Pawn/ArcMassPawn.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Core/CameraSystemEvaluator.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "GameFramework/GameplayCamerasPlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Pawn/ArcMassPawnData.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "Player/ArcMassPlayerState.h"
#include "Player/ArcMassPlayerStateExtensionComponent.h"

AArcMassPawn::AArcMassPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NavMoverComponent(nullptr)
{
	PrimaryActorTick.bCanEverTick = true;

	// Disable Actor-level movement replication; the Mover component handles it.
	SetReplicatingMovement(false);
}

void AArcMassPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	CharacterMotionComponent = FindComponentByClass<UCharacterMoverComponent>();

	if (CharacterMotionComponent)
	{
		if (USceneComponent* UpdatedComponent = CharacterMotionComponent->GetUpdatedComponent())
		{
			UpdatedComponent->SetCanEverAffectNavigation(bCanAffectNavigationGeneration);
		}
	}
}

void AArcMassPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CachedLookInput = FRotator::ZeroRotator;
}

void AArcMassPawn::BeginPlay()
{
	Super::BeginPlay();

	NavMoverComponent = FindComponentByClass<UNavMoverComponent>();
}

FVector AArcMassPawn::GetNavAgentLocation() const
{
	FVector AgentLocation = FNavigationSystem::InvalidLocation;
	const USceneComponent* UpdatedComponent = CharacterMotionComponent ? CharacterMotionComponent->GetUpdatedComponent() : nullptr;

	if (NavMoverComponent)
	{
		AgentLocation = NavMoverComponent->GetFeetLocation();
	}

	if (FNavigationSystem::IsValidLocation(AgentLocation) == false && UpdatedComponent != nullptr)
	{
		AgentLocation = UpdatedComponent->GetComponentLocation() - FVector(0, 0, UpdatedComponent->Bounds.BoxExtent.Z);
	}

	return AgentLocation;
}

void AArcMassPawn::UpdateNavigationRelevance()
{
	if (CharacterMotionComponent)
	{
		if (USceneComponent* UpdatedComponent = CharacterMotionComponent->GetUpdatedComponent())
		{
			UpdatedComponent->SetCanEverAffectNavigation(bCanAffectNavigationGeneration);
		}
	}
}

//////////////////////////

AArcMassPlayerPawn::AArcMassPlayerPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AArcMassPlayerPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (UArcMassPawnExtensionComponent* PawnExtComp = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->CheckDefaultInitialization();
	}

	if (AArcMassPlayerState* ArcPS = GetPlayerState<AArcMassPlayerState>())
	{
		ArcPS->CheckDataReady();
	}

	GameplayCameraSystemHost = IGameplayCameraSystemHost::FindActiveHost(GetLocalViewingPlayerController());

	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
		if (GetNetMode() == NM_Standalone)
		{
			if (AGameplayCamerasPlayerCameraManager* GCP = Cast<AGameplayCamerasPlayerCameraManager>(PC->PlayerCameraManager))
			{
				GCP->ActivateGameplayCamera(GetGameplayCameraComponent());
			}
		}
	}
}

void AArcMassPlayerPawn::UnPossessed()
{
	Super::UnPossessed();

	if (UArcMassPawnExtensionComponent* PawnExtComp = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->CheckDefaultInitialization();
	}
}

void AArcMassPlayerPawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	UArcMassPawnExtensionComponent* PawnExtComp = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(this);
	if (PawnExtComp != nullptr)
	{
		PawnExtComp->CheckDefaultInitialization();

		if (AArcMassPlayerState* ArcPS = GetPlayerState<AArcMassPlayerState>())
		{
			ArcPS->CheckDataReady(PawnExtComp->GetPawnData<UArcMassPawnData>());
		}

		GameplayCameraSystemHost = IGameplayCameraSystemHost::FindActiveHost(GetLocalViewingPlayerController());

		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			GetGameplayCameraComponent()->ActivateCameraForPlayerController(PC);
		}
	}
}

void AArcMassPlayerPawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (UArcMassPawnExtensionComponent* PawnExtComp = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->CheckDefaultInitialization();
	}
}

void AArcMassPlayerPawn::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	Super::SetupPlayerInputComponent(InInputComponent);

	// Gate(3) of UArcMassPawnExtensionComponent::CanChangeInitState[Spawned->DataAvailable] reads
	// Pawn->InputComponent, which APawn populates only inside this call (via PawnClientRestart).
	// PossessedBy / OnRep_Controller pumps fire before this, so they always see InputComponent==null;
	// without a pump here the init-state chain stalls forever.
	if (UArcMassPawnExtensionComponent* PawnExtComp = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->CheckDefaultInitialization();
	}
}

UCapsuleComponent* AArcMassPlayerPawn::GetCapsuleComponent() const
{
	if (CachedCapsuleComponent)
	{
		return CachedCapsuleComponent;
	}

	CachedCapsuleComponent = FindComponentByClass<UCapsuleComponent>();
	return CachedCapsuleComponent;
}

void AArcMassPlayerPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.Condition = COND_SkipOwner;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, LocalViewPoint, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, LocalViewRotation, SharedParams);
}

void AArcMassPlayerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetNetMode() == NM_Standalone || GetNetMode() == NM_Client)
	{
		if (!GameplayCameraSystemHost)
		{
			GameplayCameraSystemHost = IGameplayCameraSystemHost::FindActiveHost(GetLocalViewingPlayerController());
		}

		if (GameplayCameraSystemHost && GameplayCameraSystemHost->GetCameraSystemEvaluator())
		{
			FMinimalViewInfo ViewInfo;
			GameplayCameraSystemHost->GetCameraSystemEvaluator()->GetEvaluatedCameraView(ViewInfo);
			LocalViewPoint = ViewInfo.Location;
			LocalViewRotation = GetViewRotation();

			if (GetNetMode() == NM_Client)
			{
				ServerSetViewPointAndRotation(LocalViewPoint, LocalViewRotation);
			}
		}
	}
}

void AArcMassPlayerPawn::ServerSetViewPointAndRotation_Implementation(FVector_NetQuantize InViewPoint, FRotator InViewRotation)
{
	LocalViewPoint = InViewPoint;
	LocalViewRotation = InViewRotation;
}

void AArcMassPlayerPawn::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = LocalViewPoint;
	OutRotation = LocalViewRotation;
}

UGameplayCameraComponent* AArcMassPlayerPawn::GetGameplayCameraComponent() const
{
	if (CachedGameplayCameraComponent)
	{
		return CachedGameplayCameraComponent;
	}

	CachedGameplayCameraComponent = FindComponentByClass<UGameplayCameraComponent>();
	if (!CachedGameplayCameraComponent)
	{
		// Last-resort runtime creation if neither BP nor PawnData provided one.
		AArcMassPlayerPawn* MutableThis = const_cast<AArcMassPlayerPawn*>(this);
		CachedGameplayCameraComponent = NewObject<UGameplayCameraComponent>(MutableThis, UGameplayCameraComponent::StaticClass(), TEXT("GameplayCameraComponent"));
		CachedGameplayCameraComponent->RegisterComponent();
	}
	return CachedGameplayCameraComponent;
}
