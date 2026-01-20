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

#include "Pawn/ArcCorePawn.h"

#include "AIController.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/CameraSystemEvaluator.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/GameplayCameraComponent.h"
#include "GameFramework/GameplayCamerasPlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "Net/UnrealNetwork.h"
#include "Pawn/ArcPawnExtensionComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Player/ArcCorePlayerController.h"
#include "Player/ArcCorePlayerState.h"
#include "Player/ArcPlayerStateExtensionComponent.h"
#include "Pawn/ArcPawnData.h"

AArcCorePawn::AArcCorePawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NavMoverComponent(nullptr)
{
	PrimaryActorTick.bCanEverTick = true;

	SetReplicatingMovement(false);	// disable Actor-level movement replication, since our Mover component will handle it

	auto IsImplementedInBlueprint = [](const UFunction* Func) -> bool
	{
		return Func && ensure(Func->GetOuter())
			&& Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	};

	static FName ProduceInputBPFuncName = FName(TEXT("OnProduceInputInBlueprint"));
	UFunction* ProduceInputFunction = GetClass()->FindFunctionByName(ProduceInputBPFuncName);
	bHasProduceInputinBpFunc = IsImplementedInBlueprint(ProduceInputFunction);
}

void AArcCorePawn::PostInitializeComponents()
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

// Called every frame
void AArcCorePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Clear all camera-related cached input
	CachedLookInput = FRotator::ZeroRotator;
}

void AArcCorePawn::BeginPlay()
{
	Super::BeginPlay();

	NavMoverComponent = FindComponentByClass<UNavMoverComponent>();
}

FVector AArcCorePawn::GetNavAgentLocation() const
{
	FVector AgentLocation = FNavigationSystem::InvalidLocation;
	const USceneComponent* UpdatedComponent = CharacterMotionComponent ? CharacterMotionComponent->GetUpdatedComponent() : nullptr;
	
	if (NavMoverComponent)
	{
		AgentLocation = NavMoverComponent->GetFeetLocation();
	}
	
	if (FNavigationSystem::IsValidLocation(AgentLocation) == false && UpdatedComponent != nullptr)
	{
		AgentLocation = UpdatedComponent->GetComponentLocation() - FVector(0,0,UpdatedComponent->Bounds.BoxExtent.Z);
	}

	return AgentLocation;
}

void AArcCorePawn::UpdateNavigationRelevance()
{
	if (!bNativeProduceInput)
	{
		return;
	}
	if (CharacterMotionComponent)
	{
		if (USceneComponent* UpdatedComponent = CharacterMotionComponent->GetUpdatedComponent())
		{
			UpdatedComponent->SetCanEverAffectNavigation(bCanAffectNavigationGeneration);
		}
	}
}

void AArcCorePawn::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	OnProduceInput((float)SimTimeMs, InputCmdResult);

	if (bHasProduceInputinBpFunc)
	{
		InputCmdResult = OnProduceInputInBlueprint((float)SimTimeMs, InputCmdResult);
	}
}


void AArcCorePawn::OnProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd)
{

	// Generate user commands. Called right before the Character movement simulation will tick (for a locally controlled pawn)
	// This isn't meant to be the best way of doing a camera system. It is just meant to show a couple of ways it may be done
	// and to make sure we can keep distinct the movement, rotation, and view angles.
	// Styles 1-3 are really meant to be used with a gamepad.
	//
	// Its worth calling out: the code that happens here is happening *outside* of the Character movement simulation. All we are doing
	// is generating the input being fed into that simulation. That said, this means that A) the code below does not run on the server
	// (and non controlling clients) and B) the code is not rerun during reconcile/resimulates. Use this information guide any
	// decisions about where something should go (such as aim assist, lock on targeting systems, etc): it is hard to give absolute
	// answers and will depend on the game and its specific needs. In general, at this time, I'd recommend aim assist and lock on 
	// targeting systems to happen /outside/ of the system, i.e, here. But I can think of scenarios where that may not be ideal too.

	FCharacterDefaultInputs& CharacterInputs = OutInputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	if (GetController() == nullptr)
	{
		if (GetLocalRole() == ENetRole::ROLE_Authority && GetRemoteRole() == ENetRole::ROLE_SimulatedProxy)
		{
			static const FCharacterDefaultInputs DoNothingInput;
			// If we get here, that means this pawn is not currently possessed and we're choosing to provide default do-nothing input
			CharacterInputs = DoNothingInput;
		}

		// We don't have a local controller so we can't run the code below. This is ok. Simulated proxies will just use previous input when extrapolating
		return;
	}

	CharacterInputs.ControlRotation = FRotator::ZeroRotator;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		CharacterInputs.ControlRotation = PC->GetControlRotation();
	}

	bool bRequestedNavMovement = false;
	if (NavMoverComponent)
	{
		bRequestedNavMovement = NavMoverComponent->ConsumeNavMovementData(CachedMoveInputIntent, CachedMoveInputVelocity);
	}
	
	// Favor velocity input 
	bool bUsingInputIntentForMove = CachedMoveInputVelocity.IsZero();

	if (bUsingInputIntentForMove)
	{
		const FVector FinalDirectionalIntent = CharacterInputs.ControlRotation.RotateVector(CachedMoveInputIntent);
		CharacterInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FinalDirectionalIntent);
	}
	else
	{
		CharacterInputs.SetMoveInput(EMoveInputType::Velocity, CachedMoveInputVelocity);
	}

	
	// Normally cached input is cleared by OnMoveCompleted input event but that won't be called if movement came from nav movement
	if (bRequestedNavMovement)
	{
		CachedMoveInputIntent = FVector::ZeroVector;
		CachedMoveInputVelocity = FVector::ZeroVector;
	}
	
	static float RotationMagMin(1e-3);

	const bool bHasAffirmativeMoveInput = (CharacterInputs.GetMoveInput().Size() >= RotationMagMin);
	
	// Figure out intended orientation
	CharacterInputs.OrientationIntent = FVector::ZeroVector;


	if (bHasAffirmativeMoveInput)
	{
		if (bOrientRotationToMovement)
		{
			// set the intent to the actors movement direction
			CharacterInputs.OrientationIntent = CharacterInputs.GetMoveInput().GetSafeNormal();
		}
		else
		{
			// set intent to the the control rotation - often a player's camera rotation
			CharacterInputs.OrientationIntent = CharacterInputs.ControlRotation.Vector().GetSafeNormal();
		}

		LastAffirmativeMoveInput = CharacterInputs.GetMoveInput();
	}
	else if (bMaintainLastInputOrientation)
	{
		// There is no movement intent, so use the last-known affirmative move input
		CharacterInputs.OrientationIntent = LastAffirmativeMoveInput;
	}
	
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (AI->GetFocusActor())
		{
			const FVector FocalPoint = AI->GetFocalPoint();
			const FVector Dir = (FocalPoint - GetActorLocation()).GetSafeNormal();
			CharacterInputs.OrientationIntent = Dir;	
		}
	}
	
	CharacterInputs.bIsJumpPressed = bIsJumpPressed;
	CharacterInputs.bIsJumpJustPressed = bIsJumpJustPressed;

	if (bShouldToggleFlying)
	{
		if (!bIsFlyingActive)
		{
			CharacterInputs.SuggestedMovementMode = DefaultModeNames::Flying;
		}
		else
		{
			CharacterInputs.SuggestedMovementMode = DefaultModeNames::Falling;
		}

		bIsFlyingActive = !bIsFlyingActive;
	}
	else
	{
		CharacterInputs.SuggestedMovementMode = NAME_None;
	}

	// Convert inputs to be relative to the current movement base (depending on options and state)
	CharacterInputs.bUsingMovementBase = false;

	if (bUseBaseRelativeMovement)
	{
		if (const UCharacterMoverComponent* MoverComp = GetComponentByClass<UCharacterMoverComponent>())
		{
			if (UPrimitiveComponent* MovementBase = MoverComp->GetMovementBase())
			{
				FName MovementBaseBoneName = MoverComp->GetMovementBaseBoneName();

				FVector RelativeMoveInput, RelativeOrientDir;

				UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.GetMoveInput(), RelativeMoveInput);
				UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.OrientationIntent, RelativeOrientDir);

				CharacterInputs.SetMoveInput(CharacterInputs.GetMoveInputType(), RelativeMoveInput);
				CharacterInputs.OrientationIntent = RelativeOrientDir;

				CharacterInputs.bUsingMovementBase = true;
				CharacterInputs.MovementBase = MovementBase;
				CharacterInputs.MovementBaseBoneName = MovementBaseBoneName;
			}
		}
	}

	// Clear/consume temporal movement inputs. We are not consuming others in the event that the game world is ticking at a lower rate than the Mover simulation. 
	// In that case, we want most input to carry over between simulation frames.
	{

		bIsJumpJustPressed = false;
		bShouldToggleFlying = false;
	}
}

AArcCorePawnAbilitySystem::AArcCorePawnAbilitySystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<UArcCoreAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void AArcCorePawnAbilitySystem::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySet && AbilitySystemComponent)
	{
		AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, nullptr, this);
	}
}

UAbilitySystemComponent* AArcCorePawnAbilitySystem::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AArcCorePawnAbilitySystem::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	return GetAbilitySystemComponent()->GetOwnedGameplayTags(TagContainer);
}

UAIPerceptionComponent* AArcCorePawnAbilitySystem::GetPerceptionComponent()
{
	return FindComponentByClass<UAIPerceptionComponent>();
}
//////////////////////////

AArcCorePlayerPawn::AArcCorePlayerPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{	
}

void AArcCorePlayerPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.RepNotifyCondition = ELifetimeRepNotifyCondition::REPNOTIFY_Always;
	SharedParams.Condition = COND_SkipOwner;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, LocalViewPoint, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, LocalViewRotation, SharedParams)
}

void AArcCorePlayerPawn::PossessedBy(AController* NewController)
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
		//UE_LOG(LogArcCoreCharacter, Log, TEXT("AArcCoreCharacter::PossessedBy CheckDataReady"))
		ArcPS->CheckDataReady();
	}

	AArcCorePlayerController* PC = GetController<AArcCorePlayerController>();
	if (PC)
	{
		GameplayCameraSystemHost = PC->GetGameplayCameraSystemHost();

		if (GetNetMode() == NM_Standalone)
		{
			AGameplayCamerasPlayerCameraManager* GCP = Cast<AGameplayCamerasPlayerCameraManager>(PC->PlayerCameraManager);
			
			GCP->ActivateGameplayCamera(GetGameplayCameraComponent());
			
			//GameplayCameraComponent->ActivateCameraForPlayerController(PC);
		}
	}
}

void AArcCorePlayerPawn::UnPossessed()
{
	Super::UnPossessed();
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandleControllerChanged();
	}
}

void AArcCorePlayerPawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this);
	if (PawnExtComp != nullptr)
	{
		PawnExtComp->HandleControllerChanged();

		if(AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>())
		{
			//UE_LOG(LogArcCoreCharacter, Log, TEXT("AArcCoreCharacter::OnRep_Controller CheckDataReady"))
			ArcPS->CheckDataReady(PawnExtComp->GetPawnData<UArcPawnData>());
		}

		AArcCorePlayerController* PC = GetController<AArcCorePlayerController>();
		if (PC)
		{
			//OnPlayerControllerReplicated(PC);
			GameplayCameraSystemHost = PC->GetGameplayCameraSystemHost();

			GetGameplayCameraComponent()->ActivateCameraForPlayerController(PC);
		}
	}
}

void AArcCorePlayerPawn::OnRep_PlayerState()
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
			
			//UE_LOG(LogArcCoreCharacter, Log, TEXT("AArcCoreCharacter::OnRep_PlayerState CheckDataReady [PS %s]")
			//	, *GetNameSafe(ArcPS))
		
			ArcPS->CheckDataReady(PawnExtComp->GetPawnData<UArcPawnData>());
		}
	}
}

void AArcCorePlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->SetupPlayerInputComponent();
	}
}

void AArcCorePlayerPawn::BeginPlay()
{
	Super::BeginPlay();
}

void AArcCorePlayerPawn::Tick(float DeltaTime)
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
				//ServerSetViewPointAndRotation(LocalViewPoint, LocalViewRotation);
			}	
		}
	}
}

void AArcCorePlayerPawn::GetActorEyesViewPoint(FVector& OutLocation
	, FRotator& OutRotation) const
{
	OutLocation = LocalViewPoint;
	OutRotation = LocalViewRotation;
}


UAbilitySystemComponent* AArcCorePlayerPawn::GetAbilitySystemComponent() const
{
	if (!AbilitySystemComponent2)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetPlayerState<AArcCorePlayerState>()))
		{
			AbilitySystemComponent2 = Cast<UArcCoreAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
		}	
	}
	
	return AbilitySystemComponent2;
}

void AArcCorePlayerPawn::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	return GetAbilitySystemComponent()->GetOwnedGameplayTags(TagContainer);
}

UAIPerceptionComponent* AArcCorePlayerPawn::GetPerceptionComponent()
{
	return FindComponentByClass<UAIPerceptionComponent>();
}

UCapsuleComponent* AArcCorePlayerPawn::GetCapsuleComponent() const
{
	if (CachedCapsuleComponent)
	{
		return CachedCapsuleComponent;
	}
	
	CachedCapsuleComponent = FindComponentByClass<UCapsuleComponent>();
	return CachedCapsuleComponent;
}

UGameplayCameraComponent* AArcCorePlayerPawn::GetGameplayCameraComponent() const
{
	if (CachedGameplayCameraComponent)
	{
		return CachedGameplayCameraComponent;
	}
	
	CachedGameplayCameraComponent = FindComponentByClass<UGameplayCameraComponent>();
	return CachedGameplayCameraComponent;
}
