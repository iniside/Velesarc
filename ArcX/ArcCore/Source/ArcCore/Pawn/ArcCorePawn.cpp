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

#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "GameFramework/PlayerController.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "Pawn/ArcPawnExtensionComponent.h"

// Sets default values
AArcCorePawn::AArcCorePawn()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve
	// performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SetReplicatingMovement(false);
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


void AArcCorePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandleControllerChanged();
	}
}

void AArcCorePawn::UnPossessed()
{
	Super::UnPossessed();
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandleControllerChanged();
	}
}

void AArcCorePawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandleControllerChanged();
	}
}

void AArcCorePawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->HandlePlayerStateReplicated();
	}
	// disable Ticking on camera on remote proxies, since we will update it from server
	// regardless.
}

// Called when the game starts or when spawned
void AArcCorePawn::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AArcCorePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AArcCorePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(this))
	{
		PawnExtComp->SetupPlayerInputComponent();
	}
}

void AArcCorePawn::AddMovementInput(FVector WorldDirection, float ScaleValue, bool bForce)
{
	//Super::AddMovementInput(WorldDirection, ScaleValue, bForce);
	CachedMoveInputIntent = WorldDirection * ScaleValue;
}

void AArcCorePawn::AddControllerYawInput(float Val)
{
	CachedLookInput.Yaw = Val;
}

void AArcCorePawn::AddControllerPitchInput(float Val)
{
	CachedLookInput.Pitch = Val;
}

void AArcCorePawn::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	OnProduceInput((float)SimTimeMs, InputCmdResult);
}

void AArcCorePawn::OnProduceInput(float DeltaMs, FMoverInputCmdContext& OutInputCmd)
{
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
#if 1
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
	
	CharacterInputs.bIsJumpPressed = false; //bIsJumpPressed;
	CharacterInputs.bIsJumpJustPressed = false; //bIsJumpJustPressed;

	//if (bShouldToggleFlying)
	//{
	//	if (!bIsFlyingActive)
	//	{
	//		CharacterInputs.SuggestedMovementMode = DefaultModeNames::Flying;
	//	}
	//	else
	//	{
	//		CharacterInputs.SuggestedMovementMode = DefaultModeNames::Falling;
	//	}
	//
	//	bIsFlyingActive = !bIsFlyingActive;
	//}
	//else
	{
		CharacterInputs.SuggestedMovementMode = NAME_None;
	}

	// Convert inputs to be relative to the current movement base (depending on options and state)
	CharacterInputs.bUsingMovementBase = false;

	//if (bUseBaseRelativeMovement)
	//{
	//	if (const UCharacterMoverComponent* MoverComp = GetComponentByClass<UCharacterMoverComponent>())
	//	{
	//		if (UPrimitiveComponent* MovementBase = MoverComp->GetMovementBase())
	//		{
	//			FName MovementBaseBoneName = MoverComp->GetMovementBaseBoneName();
	//
	//			FVector RelativeMoveInput, RelativeOrientDir;
	//
	//			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.GetMoveInput(), RelativeMoveInput);
	//			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterInputs.OrientationIntent, RelativeOrientDir);
	//
	//			CharacterInputs.SetMoveInput(CharacterInputs.GetMoveInputType(), RelativeMoveInput);
	//			CharacterInputs.OrientationIntent = RelativeOrientDir;
	//
	//			CharacterInputs.bUsingMovementBase = true;
	//			CharacterInputs.MovementBase = MovementBase;
	//			CharacterInputs.MovementBaseBoneName = MovementBaseBoneName;
	//		}
	//	}
	//}

	// Clear/consume temporal movement inputs. We are not consuming others in the event that the game world is ticking at a lower rate than the Mover simulation. 
	// In that case, we want most input to carry over between simulation frames.
	//{
//
//		bIsJumpJustPressed = false;
//		bShouldToggleFlying = false;
//	}
#endif
}

AArcCorePawnAbilitySystem::AArcCorePawnAbilitySystem()
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
