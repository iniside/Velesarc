// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcChaosFalling.h"

#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "Mover/ArcChaosMovementModifiers.h"
#include "Mover/ArcMoverTypes.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcChaosFalling)

UArcChaosAirSource::UArcChaosAirSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AirControlPercentage = 0.4f;
	FallingDeceleration = 200.0f;
	FallingLateralFriction = 0.0f;
	OverTerminalSpeedFallingDeceleration = 800.0f;
	TerminalMovementPlaneSpeed = 1500.0f;
	VerticalFallingDeceleration = 4000.0f;
	TerminalVerticalSpeed = 2000.0f;
	bShouldClampTerminalVerticalSpeed = true;
}

void UArcChaosAirSource::GenerateMove_Async(
	const FMoverTickStartData& StartState,
	const FMoverTimeStep& TimeStep,
	UMoverBlackboard* SimBlackboard,
	FProposedMove& OutProposedMove)
{
	{
		TGuardValue<float> SpeedScaleGuard(StepSpeedScale, ArcMover::ComputeSpeedScale(StartState.SyncState.MovementModifiers));
		Super::GenerateMove_Async(StartState, TimeStep, SimBlackboard, OutProposedMove);
	}

	if (!Simulation || !bShouldApplyAngularVelocityToTarget)
	{
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!DefaultSimInputs || !CharacterInputs || !StartingSyncState)
	{
		return;
	}

	const FVector IntentDir = FVector::VectorPlaneProject(CharacterInputs->GetOrientationIntentDir_WorldSpace(), DefaultSimInputs->UpDir);
	if (IntentDir.IsNearlyZero())
	{
		return;
	}

	FRotator TargetOrientation = IntentDir.ToOrientationRotator();
	if (const FArcMoverCustomInputs* CustomInputs = StartState.InputCmd.InputCollection.FindDataByType<FArcMoverCustomInputs>())
	{
		TargetOrientation.Yaw += CustomInputs->RotationOffset;
	}

	// Fixed physics step, not world delta: the angular velocity is integrated over exactly this sim step.
	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	OutProposedMove.AngularVelocityDegrees = UMovementUtils::ComputeAngularVelocityDegrees(
		StartingSyncState->GetOrientation_WorldSpace(), TargetOrientation, DeltaSeconds, AirTurningRateLimit);
}

float UArcChaosAirSource::GetMaxSpeed() const
{
	return Super::GetMaxSpeed() * StepSpeedScale;
}

UArcChaosFallingMode::UArcChaosFallingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UArcChaosAirSource>(TEXT("AirSource")))
{
}
