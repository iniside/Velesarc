// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcChaosCharacterMoverComponent.h"

#include "ArcMover.h"

#include "ChaosMover/Character/Executors/ChaosCharacterAirExecutor.h"
#include "ChaosMover/Character/Executors/ChaosCharacterWalkingExecutor.h"
#include "ChaosMover/Character/Modes/ChaosFallingMode.h"
#include "ChaosMover/Character/Modes/ChaosWalkingMode.h"
#include "ChaosMover/Character/Transitions/ChaosCharacterCrouchCheck.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ChaosMover/Character/Transitions/ChaosCharacterJumpCheck.h"
#include "MovementMode.h"
#include "MovementModeTransition.h"
#include "Mover/ArcChaosFalling.h"
#include "Mover/ArcChaosMovementModifiers.h"
#include "Mover/ArcChaosSlide.h"
#include "Mover/ArcChaosWalking.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcChaosCharacterMoverComponent)

namespace ArcChaosCharacterMoverComponentPrivate
{
	void ConfigureJumpCheck(UChaosCharacterJumpCheck& JumpCheck)
	{
		JumpCheck.JumpUpwardsSpeed = 500.0f;
		JumpCheck.TransitionToMode = DefaultModeNames::Falling;
	}

	void ConfigureCrouchCheck(UChaosCharacterCrouchCheck& CrouchCheck)
	{
		CrouchCheck.CapsuleHalfHeight = 55.0f;
	}

	const UCapsuleComponent* FindCapsule(const UMoverComponent& MoverComponent)
	{
		if (const UCapsuleComponent* UpdatedCapsule = Cast<UCapsuleComponent>(MoverComponent.GetUpdatedComponent()))
		{
			return UpdatedCapsule;
		}

		const AActor* Owner = MoverComponent.GetOwner();
		return Owner ? Cast<UCapsuleComponent>(Owner->GetRootComponent()) : nullptr;
	}

	void ApplyCapsuleFloorHeight(UMoverComponent& MoverComponent)
	{
		const UCapsuleComponent* Capsule = FindCapsule(MoverComponent);
		if (!Capsule)
		{
			UE_LOGF(LogArcMover, Warning, "%ls: no capsule updated component; Chaos executors keep their engine floor height defaults.", *GetPathNameSafe(&MoverComponent));
			return;
		}

		const float TargetHeight = Capsule->GetScaledCapsuleHalfHeight();
		const float QueryRadius = FMath::Max(Capsule->GetScaledCapsuleRadius() - 5.0f, 0.0f);

		for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& Element : MoverComponent.MovementModes)
		{
			if (const UChaosWalkingMode* WalkingMode = Cast<UChaosWalkingMode>(Element.Value.Get()))
			{
				if (UChaosCharacterWalkingExecutor* WalkingExecutor = WalkingMode->GetWalkingExecutor())
				{
					WalkingExecutor->TargetHeightOverride = TargetHeight;
					WalkingExecutor->QueryRadiusOverride = QueryRadius;
				}
			}
			else if (const UChaosFallingMode* FallingMode = Cast<UChaosFallingMode>(Element.Value.Get()))
			{
				if (UChaosCharacterAirExecutor* AirExecutor = FallingMode->GetAirExecutor())
				{
					AirExecutor->TargetHeightOverride = TargetHeight;
					AirExecutor->QueryRadiusOverride = QueryRadius;
				}
			}
		}
	}

	void UseArcMovementSettings(TArray<TSubclassOf<UObject>>& SharedSettingsClasses)
	{
		for (TSubclassOf<UObject>& SettingsClass : SharedSettingsClasses)
		{
			if (SettingsClass.Get() == USharedChaosCharacterMovementSettings::StaticClass())
			{
				SettingsClass = UArcChaosCharacterMovementSettings::StaticClass();
			}
		}
	}
}

UArcChaosCharacterMovementSettings::UArcChaosCharacterMovementSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxSpeed = 800.0f;
	Acceleration = 500.0f;
	Deceleration = 0.0f;
	GroundFriction = 0.0f;
	BrakingFrictionFactor = 0.0f;
	bUseSeparateBrakingFriction = false;
	BrakingFriction = 8.0f;
	TurningRate = 300.0f;
	TurningBoost = 8.0f;
	MaxStepHeight = 40.0f;
	MaxWalkableSlopeAngle = FMath::RadiansToDegrees(FMath::Acos(0.71f));
}

UArcChaosCharacterMoverComponent::UArcChaosCharacterMoverComponent()
{
	UArcChaosSmoothWalkingMode* WalkingMode = CreateDefaultSubobject<UArcChaosSmoothWalkingMode>(TEXT("ArcChaosWalkingMode"));
	UArcChaosFallingMode* FallingMode = CreateDefaultSubobject<UArcChaosFallingMode>(TEXT("ArcChaosFallingMode"));
	UArcChaosSlideMode* SlideMode = CreateDefaultSubobject<UArcChaosSlideMode>(TEXT("ArcChaosSlideMode"));

	UChaosCharacterJumpCheck* WalkingJumpCheck = CreateDefaultSubobject<UChaosCharacterJumpCheck>(TEXT("WalkingJumpCheck"));
	ArcChaosCharacterMoverComponentPrivate::ConfigureJumpCheck(*WalkingJumpCheck);
	UChaosCharacterCrouchCheck* WalkingCrouchCheck = CreateDefaultSubobject<UChaosCharacterCrouchCheck>(TEXT("WalkingCrouchCheck"));
	ArcChaosCharacterMoverComponentPrivate::ConfigureCrouchCheck(*WalkingCrouchCheck);

	WalkingMode->Transitions.Add(WalkingJumpCheck);
	WalkingMode->Transitions.Add(WalkingCrouchCheck);
	WalkingMode->Transitions.Add(CreateDefaultSubobject<UArcChaosToSlideTransition>(TEXT("WalkingToSlideTransition")));

	UChaosCharacterJumpCheck* SlideJumpCheck = CreateDefaultSubobject<UChaosCharacterJumpCheck>(TEXT("SlideJumpCheck"));
	ArcChaosCharacterMoverComponentPrivate::ConfigureJumpCheck(*SlideJumpCheck);
	UChaosCharacterCrouchCheck* SlideCrouchCheck = CreateDefaultSubobject<UChaosCharacterCrouchCheck>(TEXT("SlideCrouchCheck"));
	ArcChaosCharacterMoverComponentPrivate::ConfigureCrouchCheck(*SlideCrouchCheck);

	SlideMode->Transitions.Add(SlideJumpCheck);
	SlideMode->Transitions.Add(SlideCrouchCheck);
	SlideMode->Transitions.Add(CreateDefaultSubobject<UArcChaosFromSlideTransition>(TEXT("SlideFromSlideTransition")));

	MovementModes.Add(DefaultModeNames::Walking, WalkingMode);
	MovementModes.Add(DefaultModeNames::Falling, FallingMode);
	MovementModes.Add(ArcMoverModeNames::Sliding, SlideMode);

	for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& Element : MovementModes)
	{
		UBaseMovementMode* Mode = Element.Value.Get();
		if (!Mode)
		{
			continue;
		}

		ArcChaosCharacterMoverComponentPrivate::UseArcMovementSettings(Mode->SharedSettingsClasses);
		for (const TObjectPtr<UBaseMovementModeTransition>& Transition : Mode->Transitions)
		{
			if (Transition)
			{
				ArcChaosCharacterMoverComponentPrivate::UseArcMovementSettings(Transition->SharedSettingsClasses);
			}
		}
	}
}

void UArcChaosCharacterMoverComponent::InitializeComponent()
{
	Super::InitializeComponent();

	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		ArcChaosCharacterMoverComponentPrivate::ApplyCapsuleFloorHeight(*this);
	}
}

FMovementModifierHandle UArcChaosCharacterMoverComponent::QueueSpeedScaleMovementModifier(const FGuid& SourceId, float SpeedPercent)
{
	if (!SourceId.IsValid() || !FMath::IsFinite(SpeedPercent) || SpeedPercent < 0.0f)
	{
		return FMovementModifierHandle();
	}

	TSharedPtr<FArcChaosMovementModifier_SpeedScale> Modifier = MakeShared<FArcChaosMovementModifier_SpeedScale>();
	Modifier->SourceId = SourceId;
	Modifier->SpeedPercent = SpeedPercent;
	return QueueModifier(FQueueModifierParams(Modifier));
}
