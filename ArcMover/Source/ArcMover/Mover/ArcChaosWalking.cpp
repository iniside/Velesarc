// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcChaosWalking.h"

#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "ChaosMover/Character/Settings/SharedChaosCharacterMovementSettings.h"
#include "HAL/IConsoleManager.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "Mover/ArcChaosMovementModifiers.h"
#include "Mover/ArcChaosSlide.h"
#include "Mover/ArcChaosWalkingExecutor.h"
#include "Mover/ArcMoverTypes.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcChaosWalking)

namespace ArcChaosWalkingPrivate
{
	TArray<const UScriptStruct*> AppendWalkStateType(TConstArrayView<const UScriptStruct*> BaseTypes)
	{
		TArray<const UScriptStruct*> Types;
		Types.Append(BaseTypes.GetData(), BaseTypes.Num());
		Types.Add(FArcChaosWalkState::StaticStruct());
		Types.Add(FArcChaosSpringFeedState::StaticStruct());
		return Types;
	}

	TAutoConsoleVariable<float> CVarOwnVelocityFeedThreshold(
		TEXT("arc.ChaosMover.OwnVelocityFeedThreshold"),
		-1.0f,
		TEXT("Deviation (cm/s) from the previous proposed velocity ignored when feeding the Arc Chaos walking/sliding spring. -1 uses the source's OwnVelocityFeedThreshold property, >=0 overrides it."),
		ECVF_Default);
}

FVector ArcChaosMover::GetWalkMovementNormal(const UMoverBlackboard* Blackboard, const FVector& UpDir, bool bMaintainHorizontalGroundVelocity)
{
	FFloorCheckResult LastFloorResult;
	if (bMaintainHorizontalGroundVelocity || !Blackboard || !Blackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult) || !LastFloorResult.IsWalkableFloor())
	{
		return UpDir;
	}
	return LastFloorResult.HitResult.ImpactNormal;
}

void ArcChaosMover::FeedSpringOwnVelocity(UChaosMoverSourceBase& Source, const FVector& MovementNormal, float Threshold, FVector& InOutVelocity)
{
	const float CVarThreshold = ArcChaosWalkingPrivate::CVarOwnVelocityFeedThreshold.GetValueOnAnyThread();
	const float EffectiveThreshold = CVarThreshold >= 0.0f ? CVarThreshold : Threshold;
	if (EffectiveThreshold < 0.0f)
	{
		return;
	}

	const FArcChaosSpringFeedState* FeedState = Source.PersistentState.FindDataByType<FArcChaosSpringFeedState>();
	if (!FeedState || !FeedState->bValid)
	{
		return;
	}

	const FVector LastVelocity = FVector::VectorPlaneProject(FeedState->LastProposedVelocity, MovementNormal);
	const FVector Deviation = InOutVelocity - LastVelocity;
	InOutVelocity = LastVelocity + Deviation.GetSafeNormal() * FMath::Max(0.0, Deviation.Size() - EffectiveThreshold);
}

void ArcChaosMover::RecordSpringOwnVelocity(UChaosMoverSourceBase& Source, const FVector& ProposedVelocity)
{
	FArcChaosSpringFeedState& FeedState = Source.PersistentState.FindOrAddMutableDataByType<FArcChaosSpringFeedState>();
	FeedState.LastProposedVelocity = ProposedVelocity;
	FeedState.bValid = true;
}

void ArcChaosMover::CarryPersistentStateIntoStartState(const UChaosMoverSourceBase* Source, const FMoverTickStartData& StartState)
{
	if (!Source)
	{
		return;
	}

	// UChaosMoverSimulation::PredictTrajectory carries only StartState between steps (ChaosMoverSimulation.cpp:388-401).
	FMoverDataCollection& StartSyncCollection = const_cast<FMoverTickStartData&>(StartState).SyncState.SyncStateCollection;
	for (const UScriptStruct* Type : Source->GetPersistentSyncStateTypes())
	{
		if (const FMoverDataStructBase* Entry = Source->PersistentState.FindDataByType(Type))
		{
			StartSyncCollection.AddDataByCopy(Entry);
		}
	}
}

UScriptStruct* FArcChaosSpringFeedState::GetScriptStruct() const
{
	return StaticStruct();
}

FMoverDataStructBase* FArcChaosSpringFeedState::Clone() const
{
	return new FArcChaosSpringFeedState(*this);
}

bool FArcChaosSpringFeedState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	const bool bSuccess = Super::NetSerialize(Ar, Map, bOutSuccess);
	Ar << LastProposedVelocity;
	Ar << bValid;
	return bSuccess;
}

bool FArcChaosSpringFeedState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	return false;
}

void FArcChaosSpringFeedState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FArcChaosSpringFeedState* ToState = static_cast<const FArcChaosSpringFeedState*>(&To);
	LastProposedVelocity = ToState->LastProposedVelocity;
	bValid = ToState->bValid;
}

UScriptStruct* FArcChaosWalkState::GetScriptStruct() const
{
	return StaticStruct();
}

FMoverDataStructBase* FArcChaosWalkState::Clone() const
{
	return new FArcChaosWalkState(*this);
}

bool FArcChaosWalkState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	const bool bSuccess = Super::NetSerialize(Ar, Map, bOutSuccess);
	Ar << JustLandedUntilSimTimeMs;
	Ar << LastComputedMaxSpeed;
	return bSuccess;
}

bool FArcChaosWalkState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FArcChaosWalkState* Authority = static_cast<const FArcChaosWalkState*>(&AuthorityState);
	return JustLandedUntilSimTimeMs != Authority->JustLandedUntilSimTimeMs
		|| !FMath::IsNearlyEqual(LastComputedMaxSpeed, Authority->LastComputedMaxSpeed);
}

void FArcChaosWalkState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FArcChaosWalkState* ToState = static_cast<const FArcChaosWalkState*>(&To);
	JustLandedUntilSimTimeMs = ToState->JustLandedUntilSimTimeMs;
	LastComputedMaxSpeed = ToState->LastComputedMaxSpeed;
}

UArcChaosSmoothWalkingSource::UArcChaosSmoothWalkingSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSmoothFacingWithDoubleSpring = false;
}

void UArcChaosSmoothWalkingSource::GenerateMove_Async(
	const FMoverTickStartData& StartState,
	const FMoverTimeStep& TimeStep,
	UMoverBlackboard* SimBlackboard,
	FProposedMove& OutProposedMove)
{
	CurrentSimTimeMs = TimeStep.BaseSimTimeMs;
	const FArcChaosWalkState* WalkState = PersistentState.FindDataByType<FArcChaosWalkState>();
	OverrideMaxSpeed(WalkState && WalkState->LastComputedMaxSpeed >= 0.0f
		? WalkState->LastComputedMaxSpeed
		: GetBaseMaxSpeed() * ArcMover::ComputeSpeedScale(StartState.SyncState.MovementModifiers));
	Super::GenerateMove_Async(StartState, TimeStep, SimBlackboard, OutProposedMove);
	ArcChaosMover::RecordSpringOwnVelocity(*this, OutProposedMove.LinearVelocity);
}

TConstArrayView<const UScriptStruct*> UArcChaosSmoothWalkingSource::GetPersistentSyncStateTypes() const
{
	static const TArray<const UScriptStruct*> Types = ArcChaosWalkingPrivate::AppendWalkStateType(Super::GetPersistentSyncStateTypes());
	return Types;
}

void UArcChaosSmoothWalkingSource::ApplyWalkMove(
	FMoverTickStartData& StartState,
	float DeltaSeconds,
	const FVector& DesiredVelocity,
	const FQuat& DesiredFacing,
	const FQuat& CurrentFacing,
	FVector& InOutAngularVelocityDegrees,
	FVector& InOutVelocity)
{
	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation ? Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>() : nullptr;
	if (DefaultSimInputs)
	{
		const FVector MovementNormal = ArcChaosMover::GetWalkMovementNormal(Simulation->GetBlackboard(), DefaultSimInputs->UpDir, bMaintainHorizontalGroundVelocity);
		ArcChaosMover::FeedSpringOwnVelocity(*this, MovementNormal, OwnVelocityFeedThreshold, InOutVelocity);
	}

	const FCharacterDefaultInputs DefaultInputsFallback;
	const FArcMoverCustomInputs CustomInputsFallback;
	const FCharacterDefaultInputs* DefaultInputsPtr = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FArcMoverCustomInputs* CustomInputsPtr = StartState.InputCmd.InputCollection.FindDataByType<FArcMoverCustomInputs>();
	const FCharacterDefaultInputs& DefaultInputs = DefaultInputsPtr ? *DefaultInputsPtr : DefaultInputsFallback;
	const FArcMoverCustomInputs& CustomInputs = CustomInputsPtr ? *CustomInputsPtr : CustomInputsFallback;

	const bool bCrouching = Simulation && Simulation->HasGameplayTag(Mover_IsCrouching, true);
	FArcChaosWalkState& WalkState = PersistentState.FindOrAddMutableDataByType<FArcChaosWalkState>();
	const bool bJustLanded = CurrentSimTimeMs < WalkState.JustLandedUntilSimTimeMs;
	const float BaseMaxSpeed = GetBaseMaxSpeed() * ArcMover::ComputeSpeedScale(StartState.SyncState.MovementModifiers);

	const FArcWalkTuning Tuning = ArcWalkGait::ComputeWalkTuning(Gait, DefaultInputs, CustomInputs, DesiredFacing, CurrentFacing
		, InOutVelocity.Size2D(), bCrouching, bJustLanded, BaseMaxSpeed, DecelerationSmoothingTime);
	WalkState.LastComputedMaxSpeed = Tuning.MaxSpeed;
	Acceleration = Tuning.Acceleration;
	Deceleration = Tuning.Deceleration;
	TurningStrength = Tuning.TurningStrength;
	FacingSmoothingTime = Tuning.FacingSmoothingTime;

	Super::ApplyWalkMove(StartState, DeltaSeconds, DesiredVelocity, Tuning.DesiredFacing, CurrentFacing, InOutAngularVelocityDegrees, InOutVelocity);

	InOutVelocity = ArcWalkGait::ClampWalkVelocity(InOutVelocity, BaseMaxSpeed);
	ArcWalkGait::ClampUnderRotation(Tuning.CurrentOffset, CustomInputs.ControlRotationRate, InOutAngularVelocityDegrees);
}

FArcWalkGaitSettings UArcChaosSmoothWalkingSource::GetGaitSettings() const
{
	return Gait;
}

float UArcChaosSmoothWalkingSource::GetBaseMaxSpeed() const
{
	if (TStrongObjectPtr<const USharedChaosCharacterMovementSettings> SharedSettings = SharedSettingsWeakPtr.Pin())
	{
		return SharedSettings->MaxSpeed;
	}
	return 0.0f;
}

float UArcChaosSmoothWalkingSource::GetDecelerationSmoothingTime() const
{
	return DecelerationSmoothingTime;
}

float UArcChaosSmoothWalkingSource::GetBrakingLag() const
{
	return ArcWalkGait::GetBrakingLag(DecelerationSmoothingTime, DecelerationSmoothingCompensation);
}

UArcChaosSmoothWalkingMode::UArcChaosSmoothWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UArcChaosWalkingExecutor>(TEXT("WalkingExecutor")))
{
	MoveSource = CreateDefaultSubobject<UArcChaosSmoothWalkingSource>(TEXT("ArcSmoothWalkingSource"));
}

void UArcChaosSmoothWalkingMode::Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState
	, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState)
{
	Super::Activate(Context, PrevModeName, SimContext, StartState, OutSyncState, OutAuxState);

	if (!OutSyncState)
	{
		return;
	}

	if (!OutSyncState->SyncStateCollection.FindDataByType<FArcChaosWalkState>())
	{
		if (const FArcChaosWalkState* PreviousWalkState = StartState.SyncState.SyncStateCollection.FindDataByType<FArcChaosWalkState>())
		{
			OutSyncState->SyncStateCollection.AddDataByCopy(PreviousWalkState);
		}
	}

	const UArcChaosSmoothWalkingSource* WalkingSource = Cast<UArcChaosSmoothWalkingSource>(MoveSource);
	if (WalkingSource && PrevModeName != DefaultModeNames::Walking && PrevModeName != ArcMoverModeNames::Sliding)
	{
		for (const UScriptStruct* SpringStateType : WalkingSource->UChaosCharacterSmoothWalkingSource::GetPersistentSyncStateTypes())
		{
			OutSyncState->SyncStateCollection.RemoveDataByType(SpringStateType);
		}
		OutSyncState->SyncStateCollection.RemoveDataByType(FArcChaosSpringFeedState::StaticStruct());
	}

	FArcChaosWalkState& WalkState = OutSyncState->SyncStateCollection.FindOrAddMutableDataByType<FArcChaosWalkState>();
	if (PrevModeName == DefaultModeNames::Falling)
	{
		const double JustLandedDurationMs = WalkingSource ? WalkingSource->Gait.JustLandedDurationMs : FArcWalkGaitSettings().JustLandedDurationMs;
		WalkState.JustLandedUntilSimTimeMs = Context.EventTimeMs + JustLandedDurationMs;
	}
}

void UArcChaosSmoothWalkingMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	Super::GenerateMove_Implementation(SimContext, StartState, TimeStep, OutProposedMove);
	ArcChaosMover::CarryPersistentStateIntoStartState(MoveSource, StartState);
}
