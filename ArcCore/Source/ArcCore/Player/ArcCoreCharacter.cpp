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

#include "Player/ArcCoreCharacter.h"

#include "ArcCorePlayerController.h"
#include "ArcCorePlayerState.h"
#include "ArcPlayerStateExtensionComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"

#include "Pawn/ArcPawnExtensionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/ArcCoreAssetManager.h"
#include "Core/CameraSystemEvaluator.h"
#include "Engine/AssetManager.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "GameFramework/GameplayCameraSystemComponent.h"

#include "Net/UnrealNetwork.h"
#include "Pawn/ArcPawnData.h"

DEFINE_LOG_CATEGORY(LogArcCoreCharacter);

void AArcCoreCharacter::ServerSetViewPointAndRotation_Implementation(FVector NewViewPoint
	, FRotator NewViewRotation)
{
	LocalViewPoint = NewViewPoint;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, LocalViewPoint, this);
	
	LocalViewRotation = NewViewRotation;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, LocalViewRotation, this);
}

void AArcCoreCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.RepNotifyCondition = ELifetimeRepNotifyCondition::REPNOTIFY_Always;
	SharedParams.Condition = COND_SkipOwner;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass
		, LocalViewPoint
		, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass
		, LocalViewRotation
		, SharedParams)
}


AArcCoreCharacter::AArcCoreCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Make mesh component to do not activate automatically.
	// it will be manually activated inside ArcHeroComponent.
	GetMesh()->bAutoActivate = false;
	GetCapsuleComponent()->PrimaryComponentTick.bCanEverTick = false;
	GetCapsuleComponent()->PrimaryComponentTick.bStartWithTickEnabled = false;

	GameplayCameraComponent = CreateDefaultSubobject<UGameplayCameraComponent>(TEXT("GameplayCameraComponent"));
	GameplayCameraComponent->SetupAttachment(RootComponent);
}

void AArcCoreCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandleControllerChanged();
	}

	if (APlayerState* PS = GetPlayerState<APlayerState>())
	{
		if (UArcPlayerStateExtensionComponent* PSExt = UArcPlayerStateExtensionComponent::Get(PS))
		{
			PSExt->CheckDefaultInitialization();
		}	
	}
	
	if (AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>())
	{
		UE_LOG(LogArcCoreCharacter, Log, TEXT("AArcCoreCharacter::PossessedBy CheckDataReady"))
		ArcPS->CheckDataReady();
	}

	AArcCorePlayerController* PC = GetController<AArcCorePlayerController>();
	if (PC)
	{
		GameplayCameraSystemHost = PC->GetGameplayCameraSystemHost();

		if (GetNetMode() == NM_Standalone)
		{
			GameplayCameraComponent->ActivateCameraForPlayerController(PC);
		}
	}
}

void AArcCoreCharacter::UnPossessed()
{
	Super::UnPossessed();
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandleControllerChanged();
	}
}

void AArcCoreCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this);
	if (PawnExtComp != nullptr)
	{
		PawnExtComp->HandleControllerChanged();

		if(AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>())
		{
			UE_LOG(LogArcCoreCharacter, Log, TEXT("AArcCoreCharacter::OnRep_Controller CheckDataReady"))
			ArcPS->CheckDataReady(PawnExtComp->GetPawnData<UArcPawnData>());
		}

		AArcCorePlayerController* PC = GetController<AArcCorePlayerController>();
		if (PC)
		{
			OnPlayerControllerReplicated(PC);
			GameplayCameraSystemHost = PC->GetGameplayCameraSystemHost();

			GameplayCameraComponent->ActivateCameraForPlayerController(PC);
		}
	}
}

void AArcCoreCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandlePlayerStateReplicated();

		if(AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>())
		{
			if (UArcPlayerStateExtensionComponent* PSExt = UArcPlayerStateExtensionComponent::Get(ArcPS))
			{
				PSExt->CheckDefaultInitialization();
			}
			UE_LOG(LogArcCoreCharacter, Log, TEXT("AArcCoreCharacter::OnRep_PlayerState CheckDataReady [PS %s]")
				, *GetNameSafe(ArcPS))
		
			ArcPS->CheckDataReady(PawnExtComp->GetPawnData<UArcPawnData>());
		}
	}
}

void AArcCoreCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->SetupPlayerInputComponent();
	}
}

void AArcCoreCharacter::BeginPlay()
{
	Super::BeginPlay();

}

void AArcCoreCharacter::BecomeViewTarget(APlayerController* PC)
{
	Super::BecomeViewTarget(PC);
}

void AArcCoreCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetNetMode() == NM_Standalone || GetNetMode() == NM_Client)
	{
		if (!GameplayCameraSystemHost)
		{
			GameplayCameraSystemHost = IGameplayCameraSystemHost::FindActiveHost(GetLocalViewingPlayerController());	
		}
		
		if (GameplayCameraSystemHost)
		{
			FMinimalViewInfo ViewInfo;
			GameplayCameraSystemHost->GetCameraSystemEvaluator()->GetEvaluatedCameraView(ViewInfo);
			LocalViewPoint = ViewInfo.Location;//FMath::VInterpConstantTo(LocalViewPoint, ViewInfo.Location, DeltaTime, 8.f);
			//LocalViewPoint = FMath::VInterpNormalRotationTo(LocalViewPoint, ViewInfo.Location, DeltaTime, 8.f);
			//LocalViewPoint = ViewInfo.Location;
			LocalViewRotation = GetViewRotation(); //OutResult.Rotation;	
			
			if (GetNetMode() == NM_Client)
			{
				ServerSetViewPointAndRotation(LocalViewPoint, LocalViewRotation);
			}	
		}
	}
}

void AArcCoreCharacter::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

void AArcCoreCharacter::GetActorEyesViewPoint(FVector& OutLocation
	, FRotator& OutRotation) const
{
	OutLocation = LocalViewPoint;
	OutRotation = LocalViewRotation;
}

UArcCoreAbilitySystemComponent* AArcCoreCharacter::GetArcAbilitySystemComponent() const
{
	if(AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>())
	{
		return ArcPS->GetArcAbilitySystemComponent();
	}
	
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		return PawnExtComp->GetArcAbilitySystemComponent();
	}

	return nullptr;
}

void AArcCoreCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (GetArcAbilitySystemComponent())
	{
		GetArcAbilitySystemComponent()->GetOwnedGameplayTags(TagContainer);
	}
}
