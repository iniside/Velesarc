// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcChaosSlide.h"

#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "Kismet/KismetMathLibrary.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "Mover/ArcChaosMovementModifiers.h"
#include "Mover/ArcChaosWalking.h"
#include "Mover/ArcChaosWalkingExecutor.h"
#include "Mover/ArcMoverTypes.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcChaosSlide)

namespace ArcMoverModeNames
{
	const FName Sliding = TEXT("Sliding");
}

namespace ArcChaosSlidePrivate
{
	TArray<const UScriptStruct*> BuildPersistentTypes(TConstArrayView<const UScriptStruct*> BaseTypes)
	{
		TArray<const UScriptStruct*> Types;
		Types.Append(BaseTypes.GetData(), BaseTypes.Num());
		Types.Add(FArcChaosSlideState::StaticStruct());
		Types.Add(FArcChaosSpringFeedState::StaticStruct());
		return Types;
	}
}

UScriptStruct* FArcChaosSlideState::GetScriptStruct() const
{
	return StaticStruct();
}

FMoverDataStructBase* FArcChaosSlideState::Clone() const
{
	return new FArcChaosSlideState(*this);
}

bool FArcChaosSlideState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	const bool bSuccess = Super::NetSerialize(Ar, Map, bOutSuccess);
	Ar << ActivatedSimTimeMs;
	Ar << LastComputedMaxSpeed;
	return bSuccess;
}

bool FArcChaosSlideState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FArcChaosSlideState& Authority = static_cast<const FArcChaosSlideState&>(AuthorityState);
	return ActivatedSimTimeMs != Authority.ActivatedSimTimeMs
		|| !FMath::IsNearlyEqual(LastComputedMaxSpeed, Authority.LastComputedMaxSpeed);
}

void FArcChaosSlideState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FArcChaosSlideState& ToState = static_cast<const FArcChaosSlideState&>(To);
	ActivatedSimTimeMs = ToState.ActivatedSimTimeMs;
	LastComputedMaxSpeed = ToState.LastComputedMaxSpeed;
}

UArcChaosSlideSource::UArcChaosSlideSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Acceleration = 2000.0f;
	Deceleration = 200.0f;
	TurningStrength = 1.0f;
	AccelerationSmoothingTime = 0.0f;
	DecelerationSmoothingTime = 0.0f;
	FacingSmoothingTime = 0.15f;
	bSmoothFacingWithDoubleSpring = false;
	VelocityDeadzoneThreshold = 0.01f;
	AccelerationDeadzoneThreshold = 0.001f;
	FacingDeadzoneThreshold = 0.1f;
	AngularVelocityDeadzoneThreshold = 0.01f;
	OutsideInfluenceSmoothingTime = 0.05f;
}

void UArcChaosSlideSource::GenerateMove_Async(
	const FMoverTickStartData& StartState,
	const FMoverTimeStep& TimeStep,
	UMoverBlackboard* SimBlackboard,
	FProposedMove& OutProposedMove)
{
	CurrentStepSimTimeMs = TimeStep.BaseSimTimeMs;
	const FArcChaosSlideState* SlideState = PersistentState.FindDataByType<FArcChaosSlideState>();
	OverrideMaxSpeed(SlideState ? SlideState->LastComputedMaxSpeed : InitialBoostSpeed * ArcMover::ComputeSpeedScale(StartState.SyncState.MovementModifiers));
	Super::GenerateMove_Async(StartState, TimeStep, SimBlackboard, OutProposedMove);
	ArcChaosMover::RecordSpringOwnVelocity(*this, OutProposedMove.LinearVelocity);
}

TConstArrayView<const UScriptStruct*> UArcChaosSlideSource::GetPersistentSyncStateTypes() const
{
	static const TArray<const UScriptStruct*> Types = ArcChaosSlidePrivate::BuildPersistentTypes(Super::GetPersistentSyncStateTypes());
	return Types;
}

void UArcChaosSlideSource::ApplyWalkMove(
	FMoverTickStartData& StartState,
	float DeltaSeconds,
	const FVector& DesiredVelocity,
	const FQuat& DesiredFacing,
	const FQuat& CurrentFacing,
	FVector& InOutAngularVelocityDegrees,
	FVector& InOutVelocity)
{
	check(Simulation);
	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	check(DefaultSimInputs);

	const FVector MovementNormal = ArcChaosMover::GetWalkMovementNormal(Simulation->GetBlackboard(), DefaultSimInputs->UpDir, bMaintainHorizontalGroundVelocity);
	ArcChaosMover::FeedSpringOwnVelocity(*this, MovementNormal, OwnVelocityFeedThreshold, InOutVelocity);

	const FArcMoverCustomInputs* CustomInputs = StartState.InputCmd.InputCollection.FindDataByType<FArcMoverCustomInputs>();
	const double RotationOffset = CustomInputs ? CustomInputs->RotationOffset : 0.0;

	const FRotator DesiredRotator = DesiredFacing.Rotator();
	const double CurrentOffset = UKismetMathLibrary::NormalizedDeltaRotator(CurrentFacing.Rotator(), DesiredRotator).Yaw;
	const double ClampedOffset = FMath::Clamp(RotationOffset, CurrentOffset - 179.0, CurrentOffset + 179.0);
	const FQuat OverriddenDesiredFacing = UKismetMathLibrary::ComposeRotators(DesiredRotator, FRotator(0.0, ClampedOffset, 0.0)).Quaternion();

	FFloorCheckResult LastFloorResult;
	if (const UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard())
	{
		SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult);
	}

	FVector HorizontalDir = FVector::ZeroVector;
	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (CharacterInputs
		&& (CharacterInputs->GetMoveInputType() == EMoveInputType::DirectionalIntent || CharacterInputs->GetMoveInputType() == EMoveInputType::Velocity))
	{
		HorizontalDir = FVector::VectorPlaneProject(CharacterInputs->GetMoveInput_WorldSpace(), DefaultSimInputs->UpDir).GetSafeNormal();
	}
	const double FloorDot = FMath::Clamp(FVector::DotProduct(LastFloorResult.HitResult.Normal, HorizontalDir), -1.0, 1.0);
	const double SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(FloorDot)) - 90.0;

	FArcChaosSlideState* SlideState = PersistentState.FindMutableDataByType<FArcChaosSlideState>();
	const bool bBoosting = SlideState && (CurrentStepSimTimeMs - SlideState->ActivatedSimTimeMs) < InitialBoostTime * 1000.0;

	double NewMaxSpeed = InitialBoostSpeed;
	if (!bBoosting)
	{
		NewMaxSpeed = SlopeAngle > -ShallowSlopeAngle
			? FlatGroundSpeed
			: UKismetMathLibrary::MapRangeClamped(SlopeAngle, -ShallowSlopeAngle, -SteepSlopeAngle, ShallowSlopeSpeed, SteepSlopeSpeed);
	}
	NewMaxSpeed *= ArcMover::ComputeSpeedScale(StartState.SyncState.MovementModifiers);
	if (SlideState)
	{
		SlideState->LastComputedMaxSpeed = static_cast<float>(NewMaxSpeed);
	}
	OverrideAcceleration(bBoosting ? InitialBoostAcceleration : AfterBoostAcceleration);

	const double NewDeceleration = UKismetMathLibrary::MapRangeClamped(SlopeAngle, ShallowSlopeAngle, SteepSlopeAngle, FlatGroundDeceleration, SteepSlopeDeceleration);
	TGuardValue<float> DecelerationGuard(Deceleration, static_cast<float>(NewDeceleration));

	Super::ApplyWalkMove(StartState, DeltaSeconds, DesiredVelocity, OverriddenDesiredFacing, CurrentFacing, InOutAngularVelocityDegrees, InOutVelocity);
}

UArcChaosSlideMode::UArcChaosSlideMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UArcChaosWalkingExecutor>(TEXT("WalkingExecutor")))
{
	MoveSource = CreateDefaultSubobject<UArcChaosSlideSource>(TEXT("SlideSource"));
}

void UArcChaosSlideMode::Activate(
	const FMoverEventContext& Context,
	FName PrevModeName,
	const FMoverSimContext& SimContext,
	const FMoverTickStartData& StartState,
	FMoverSyncState* OutSyncState,
	FMoverAuxStateContext* OutAuxState)
{
	Super::Activate(Context, PrevModeName, SimContext, StartState, OutSyncState, OutAuxState);

	if (!OutSyncState)
	{
		return;
	}

	if (PrevModeName != DefaultModeNames::Walking && PrevModeName != ArcMoverModeNames::Sliding)
	{
		OutSyncState->SyncStateCollection.RemoveDataByType(FArcChaosSpringFeedState::StaticStruct());
	}

	FArcChaosSlideState& SlideState = OutSyncState->SyncStateCollection.FindOrAddMutableDataByType<FArcChaosSlideState>();
	if (PrevModeName != ArcMoverModeNames::Sliding)
	{
		SlideState.ActivatedSimTimeMs = Context.EventTimeMs;
		if (const UArcChaosSlideSource* SlideSource = Cast<UArcChaosSlideSource>(MoveSource))
		{
			SlideState.LastComputedMaxSpeed = SlideSource->InitialBoostSpeed * ArcMover::ComputeSpeedScale(StartState.SyncState.MovementModifiers);
		}
	}
}

void UArcChaosSlideMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	Super::GenerateMove_Implementation(SimContext, StartState, TimeStep, OutProposedMove);
	ArcChaosMover::CarryPersistentStateIntoStartState(MoveSource, StartState);
}

UArcChaosToSlideTransition::UArcChaosToSlideTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
	bFirstSubStepOnly = false;
	bAllowModeReentry = false;
}

FTransitionEvalResult UArcChaosToSlideTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult;
	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation ? Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>() : nullptr;
	if (!DefaultSimInputs)
	{
		return EvalResult;
	}

	const double PlanarSpeed = FVector::VectorPlaneProject(Params.ProposedMove.LinearVelocity, DefaultSimInputs->UpDir).Size();
	if (PlanarSpeed > EnterSpeed && Simulation->HasGameplayTag(Mover_IsCrouching, true))
	{
		EvalResult.NextMode = ArcMoverModeNames::Sliding;
	}

	return EvalResult;
}

UArcChaosFromSlideTransition::UArcChaosFromSlideTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
	bFirstSubStepOnly = false;
	bAllowModeReentry = false;
}

FTransitionEvalResult UArcChaosFromSlideTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult;
	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation ? Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>() : nullptr;
	if (!DefaultSimInputs)
	{
		return EvalResult;
	}

	const double PlanarSpeed = FVector::VectorPlaneProject(Params.ProposedMove.LinearVelocity, DefaultSimInputs->UpDir).Size();
	if (!Simulation->HasGameplayTag(Mover_IsCrouching, true) || PlanarSpeed <= ExitSpeed)
	{
		EvalResult.NextMode = DefaultModeNames::Walking;
	}

	return EvalResult;
}
