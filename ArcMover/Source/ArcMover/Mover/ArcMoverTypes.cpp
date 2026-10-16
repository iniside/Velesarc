// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMoverTypes.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "DefaultMovementSet/Settings/StanceSettings.h"

#include "GameFramework/Actor.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Mover_Gait_Walk, "Mover.Gait.Walk");
UE_DEFINE_GAMEPLAY_TAG(TAG_Mover_Gait_Run, "Mover.Gait.Run");
UE_DEFINE_GAMEPLAY_TAG(TAG_Mover_Gait_Sprint, "Mover.Gait.Sprint");

UE_DEFINE_GAMEPLAY_TAG(TAG_Mover_BlockAllInput, "Mover.BlockAllInput");

FArcMovementModifier_MaxVelocity::FArcMovementModifier_MaxVelocity()
{
	DurationMs = -1.0f;
}

void FArcMovementModifier_MaxVelocity::OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState
	, const FMoverAuxStateContext& AuxState)
{
	if (UCommonLegacyMovementSettings* MovementSettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>())
	{
		OriginalMaxSpeed = MovementSettings->MaxSpeed;
		MovementSettings->MaxSpeed = MaxVelocity;
	}

	// UArcMoverInputProducerComponent retired with Bucket B / C cluster; gait override
	// will be reimplemented against UArcMassMoverInputProducerComponent later.
}

void FArcMovementModifier_MaxVelocity::OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState
	, const FMoverAuxStateContext& AuxState)
{
	if (UCommonLegacyMovementSettings* MovementSettings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>())
	{
		MovementSettings->MaxSpeed = OriginalMaxSpeed;
	}

	// UArcMoverInputProducerComponent retired with Bucket B / C cluster; gait reset
	// will be reimplemented against UArcMassMoverInputProducerComponent later.
}

FMovementModifierBase* FArcMovementModifier_MaxVelocity::Clone() const
{
	return new FArcMovementModifier_MaxVelocity(*this);
}

void FArcMovementModifier_MaxVelocity::NetSerialize(FArchive& Ar)
{
	
}

UScriptStruct* FArcMovementModifier_MaxVelocity::GetScriptStruct() const
{
	return FArcMovementModifier_MaxVelocity::StaticStruct();  
}

FMovementModifierHandle UArcMoverFunctions::ApplyMaxVelocity(AActor* Target, float MaxVelocity, EArcMoverGaitType Gait)
{
	if (UMoverComponent* Mover = Target->FindComponentByClass<UMoverComponent>())
	{
		TSharedPtr<FArcMovementModifier_MaxVelocity> Modifier = MakeShareable(new FArcMovementModifier_MaxVelocity());
		Modifier->MaxVelocity = MaxVelocity;
		Modifier->Gait = Gait;
		FMovementModifierHandle Handle = Mover->QueueMovementModifier(Modifier);
		
		return Handle;
	}
	
	return FMovementModifierHandle(MODIFIER_INVALID_HANDLE);
}

UArcSmoothWalkingMode::UArcSmoothWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SharedSettingsClasses.Add(UStanceSettings::StaticClass());
	bSmoothFacingWithDoubleSpring = false;
}

void UArcSmoothWalkingMode::Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState
	, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState)
{
	Super::Activate(Context, PrevModeName, SimContext, StartState, OutSyncState, OutAuxState);

	// The BP used a retriggerable 0.2 s latent delay; the window is tracked in sim time so it follows the simulation clock.
	if (PrevModeName == DefaultModeNames::Falling)
	{
		JustLandedUntilSimTimeMs = Context.EventTimeMs + GetGaitSettings().JustLandedDurationMs;
	}
}

void UArcSmoothWalkingMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep
	, FProposedMove& OutProposedMove) const
{
	CurrentSimTimeMs = TimeStep.BaseSimTimeMs;
	Super::GenerateMove_Implementation(SimContext, StartState, TimeStep, OutProposedMove);
}

FArcWalkGaitSettings UArcSmoothWalkingMode::GetGaitSettings() const
{
	FArcWalkGaitSettings Settings;
	Settings.WalkSpeed = WalkSpeed;
	Settings.WalkAcceleration = WalkAcceleration;
	Settings.RunSpeed = RunSpeed;
	Settings.RunAcceleration = RunAcceleration;
	Settings.SprintSpeed = SprintSpeed;
	Settings.SprintAcceleration = SprintAcceleration;
	Settings.CrouchSpeed = CrouchSpeed;
	Settings.WalkRunTurnStrength = WalkRunTurnStrength;
	Settings.SprintTurnStrength = SprintTurnStrength;
	Settings.GaitChangeDeceleration = GaitChangeDeceleration;
	Settings.StoppingDeceleration = StoppingDeceleration;
	Settings.WalkRunFacingTime = WalkRunFacingTime;
	Settings.SprintFacingTime = SprintFacingTime;
	Settings.IdleFacingTime = IdleFacingTime;
	return Settings;
}

float UArcSmoothWalkingMode::GetBaseMaxSpeed() const
{
	return CommonLegacySettings ? CommonLegacySettings->MaxSpeed : 0.0f;
}

float UArcSmoothWalkingMode::GetDecelerationSmoothingTime() const
{
	return DecelerationSmoothingTime;
}

float UArcSmoothWalkingMode::GetBrakingLag() const
{
	return ArcWalkGait::GetBrakingLag(DecelerationSmoothingTime, DecelerationSmoothingCompensation);
}

void UArcSmoothWalkingMode::GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds, const FMoverSimContext& SimContext, const FVector& DesiredVelocity,
									 const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity)
{
	const FCharacterDefaultInputs DefaultInputsFallback;
	const FArcMoverCustomInputs CustomInputsFallback;
	const FCharacterDefaultInputs* DefaultInputsPtr = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FArcMoverCustomInputs* CustomInputsPtr = StartState.InputCmd.InputCollection.FindDataByType<FArcMoverCustomInputs>();
	const FCharacterDefaultInputs& DefaultInputs = DefaultInputsPtr ? *DefaultInputsPtr : DefaultInputsFallback;
	const FArcMoverCustomInputs& CustomInputs = CustomInputsPtr ? *CustomInputsPtr : CustomInputsFallback;

	const UCharacterMoverComponent* CharacterMover = Cast<UCharacterMoverComponent>(GetMoverComponent());
	const bool bCrouching = CharacterMover && CharacterMover->IsCrouching();
	const bool bJustLanded = CurrentSimTimeMs < JustLandedUntilSimTimeMs;

	const FArcWalkTuning Tuning = ArcWalkGait::ComputeWalkTuning(GetGaitSettings(), DefaultInputs, CustomInputs, DesiredFacing, CurrentFacing
		, InOutVelocity.Size2D(), bCrouching, bJustLanded, GetBaseMaxSpeed(), DecelerationSmoothingTime);
	MaxSpeedOverride = Tuning.MaxSpeed;
	Acceleration = Tuning.Acceleration;
	Deceleration = Tuning.Deceleration;
	TurningStrength = Tuning.TurningStrength;
	FacingSmoothingTime = Tuning.FacingSmoothingTime;

	Super::GenerateWalkMove_Implementation(StartState, DeltaSeconds, SimContext, DesiredVelocity, Tuning.DesiredFacing, CurrentFacing, InOutAngularVelocityDegrees, InOutVelocity);

	if (CommonLegacySettings)
	{
		InOutVelocity = ArcWalkGait::ClampWalkVelocity(InOutVelocity, CommonLegacySettings->MaxSpeed);
	}

	ArcWalkGait::ClampUnderRotation(Tuning.CurrentOffset, CustomInputs.ControlRotationRate, InOutAngularVelocityDegrees);
}
