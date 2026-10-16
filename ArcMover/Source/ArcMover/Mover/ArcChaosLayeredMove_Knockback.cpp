// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcChaosLayeredMove_Knockback.h"

#include "Mover/ArcLayeredMove_Knockback.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcChaosLayeredMove_Knockback)

FArcChaosLayeredMove_Knockback::FArcChaosLayeredMove_Knockback()
{
	MixMode = EMoveMixMode::OverrideVelocity;
	FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
	SyncNativeDuration();
}

bool FArcChaosLayeredMove_Knockback::HasValidParameters() const
{
	return ArcMover::HasValidKnockbackParameters(Direction, DistanceCm, DurationSeconds, UpDirection);
}

void FArcChaosLayeredMove_Knockback::ResetForActivation()
{
	MixMode = EMoveMixMode::OverrideVelocity;
	FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
	bTerminalCleanupGenerated = false;
	if (HasValidParameters())
	{
		SyncNativeDuration();
	}
	else
	{
		DurationMs = 0.0f;
		bTerminalCleanupGenerated = true;
	}
}

void FArcChaosLayeredMove_Knockback::OnActivated(const FLayeredMoveOnActivatedParams& Params)
{
	ResetForActivation();
	Super::OnActivated(Params);
}

bool FArcChaosLayeredMove_Knockback::GenerateMove(const FLayeredMoveGenerateMoveParams& Params, FProposedMove& OutProposedMove)
{
	return GenerateMove_Async(Params.StartState, Params.TimeStep, Params.SimBlackboard, OutProposedMove);
}

void FArcChaosLayeredMove_Knockback::OnStart_Async(UMoverBlackboard* SimBlackboard, const FMoverTime& SimTime)
{
	ResetForActivation();
}

bool FArcChaosLayeredMove_Knockback::GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	if (!HasValidParameters())
	{
		DurationMs = 0.0f;
		bTerminalCleanupGenerated = true;
		return false;
	}

	if (!FMath::IsFinite(TimeStep.BaseSimTimeMs) || !FMath::IsFinite(TimeStep.StepMs)
		|| TimeStep.StepMs <= 0.0f || !FMath::IsFinite(StartSimTimeMs))
	{
		return false;
	}

	const double RemainingDurationMs = static_cast<double>(DurationMs)
		- FMath::Max(0.0, TimeStep.BaseSimTimeMs - StartSimTimeMs);
	if (RemainingDurationMs <= 0.0)
	{
		OutProposedMove.LinearVelocity = FVector::ZeroVector;
		OutProposedMove.MixMode = EMoveMixMode::OverrideAllExceptVerticalVelocity;
		bTerminalCleanupGenerated = true;
		return true;
	}

	const FVector NormalizedUpDirection = UpDirection.GetSafeNormal();
	const FVector PlanarDirection = FVector::VectorPlaneProject(Direction, NormalizedUpDirection).GetSafeNormal();
	const double ActiveStepMs = FMath::Min(static_cast<double>(TimeStep.StepMs), RemainingDurationMs);
	const double StepFraction = ActiveStepMs / static_cast<double>(TimeStep.StepMs);
	const double SpeedCmPerSecond = static_cast<double>(DistanceCm) / static_cast<double>(DurationSeconds);

	OutProposedMove.LinearVelocity = PlanarDirection * (SpeedCmPerSecond * StepFraction);
	OutProposedMove.MixMode = EMoveMixMode::OverrideAllExceptVerticalVelocity;
	return true;
}

bool FArcChaosLayeredMove_Knockback::IsFinished(double) const
{
	return bTerminalCleanupGenerated || DurationMs == 0.0f;
}

FLayeredMoveBase* FArcChaosLayeredMove_Knockback::Clone() const
{
	FArcChaosLayeredMove_Knockback* Copy = new FArcChaosLayeredMove_Knockback(*this);
	Copy->SyncNativeDuration();
	return Copy;
}

void FArcChaosLayeredMove_Knockback::NetSerialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		SyncNativeDuration();
	}

	Super::NetSerialize(Ar);
	Ar << Direction;
	Ar << DistanceCm;
	Ar << DurationSeconds;
	Ar << UpDirection;
	uint8 bSerializedTerminalCleanupGenerated = bTerminalCleanupGenerated ? 1 : 0;
	Ar.SerializeBits(&bSerializedTerminalCleanupGenerated, 1);
	bTerminalCleanupGenerated = bSerializedTerminalCleanupGenerated != 0;

	if (Ar.IsLoading())
	{
		SyncNativeDuration();
	}
}

UScriptStruct* FArcChaosLayeredMove_Knockback::GetScriptStruct() const
{
	return FArcChaosLayeredMove_Knockback::StaticStruct();
}

FString FArcChaosLayeredMove_Knockback::ToSimpleString() const
{
	return TEXT("ArcChaosKnockback");
}

void FArcChaosLayeredMove_Knockback::SyncNativeDuration()
{
	DurationMs = ArcMover::IsKnockbackDurationRepresentable(DurationSeconds)
		? static_cast<float>(static_cast<double>(DurationSeconds) * 1000.0)
		: 0.0f;
}
