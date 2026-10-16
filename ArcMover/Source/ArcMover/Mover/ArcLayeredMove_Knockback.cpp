// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcLayeredMove_Knockback.h"

#include "MoverComponent.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcLayeredMove_Knockback)

namespace ArcMover
{
	bool IsKnockbackDurationRepresentable(float DurationSeconds)
	{
		const double NativeDurationMs = static_cast<double>(DurationSeconds) * 1000.0;
		return FMath::IsFinite(DurationSeconds) && DurationSeconds > 0.0f
			&& FMath::IsFinite(NativeDurationMs) && NativeDurationMs <= static_cast<double>(TNumericLimits<float>::Max());
	}

	bool HasValidKnockbackParameters(const FVector& Direction, float DistanceCm, float DurationSeconds, const FVector& UpDirection)
	{
		const bool bDirectionIsFinite = FMath::IsFinite(Direction.X) && FMath::IsFinite(Direction.Y) && FMath::IsFinite(Direction.Z);
		const bool bUpDirectionIsFinite = FMath::IsFinite(UpDirection.X) && FMath::IsFinite(UpDirection.Y) && FMath::IsFinite(UpDirection.Z);
		if (!bDirectionIsFinite || !bUpDirectionIsFinite || !FMath::IsFinite(DistanceCm) || DistanceCm < 0.0f
			|| !IsKnockbackDurationRepresentable(DurationSeconds))
		{
			return false;
		}

		const FVector NormalizedUpDirection = UpDirection.GetSafeNormal();
		return !NormalizedUpDirection.IsNearlyZero()
			&& !FVector::VectorPlaneProject(Direction, NormalizedUpDirection).IsNearlyZero();
	}
}

FArcLayeredMove_Knockback::FArcLayeredMove_Knockback()
{
	MixMode = EMoveMixMode::OverrideVelocity;
	FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
	SyncNativeDuration();
}

bool FArcLayeredMove_Knockback::HasValidParameters(const FVector& UpDirection) const
{
	return ArcMover::HasValidKnockbackParameters(Direction, DistanceCm, DurationSeconds, UpDirection);
}

void FArcLayeredMove_Knockback::OnActivated(const FLayeredMoveOnActivatedParams& Params)
{
	const UMoverComponent* MoverComponent = Cast<UMoverComponent>(Params.SimContext.SimulationOwner);
	MixMode = EMoveMixMode::OverrideVelocity;
	FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
	bTerminalCleanupGenerated = false;
	if (MoverComponent && HasValidParameters(MoverComponent->GetUpDirection()))
	{
		SyncNativeDuration();
	}
	else
	{
		DurationMs = 0.0f;
		bTerminalCleanupGenerated = true;
	}

	Super::OnActivated(Params);
}

bool FArcLayeredMove_Knockback::GenerateMove(const FLayeredMoveGenerateMoveParams& Params, FProposedMove& OutProposedMove)
{
	const UMoverComponent* MoverComponent = Cast<UMoverComponent>(Params.SimContext.SimulationOwner);
	if (!MoverComponent || !HasValidParameters(MoverComponent->GetUpDirection()))
	{
		DurationMs = 0.0f;
		bTerminalCleanupGenerated = true;
		return false;
	}

	if (!FMath::IsFinite(Params.TimeStep.BaseSimTimeMs) || !FMath::IsFinite(Params.TimeStep.StepMs)
		|| Params.TimeStep.StepMs <= 0.0f || !FMath::IsFinite(StartSimTimeMs))
	{
		return false;
	}

	const double RemainingDurationMs = static_cast<double>(DurationMs)
		- FMath::Max(0.0, Params.TimeStep.BaseSimTimeMs - StartSimTimeMs);
	if (RemainingDurationMs <= 0.0)
	{
		OutProposedMove.LinearVelocity = FVector::ZeroVector;
		OutProposedMove.MixMode = EMoveMixMode::OverrideAllExceptVerticalVelocity;
		// Commit zero planar velocity with the movement mode's current vertical result before expiring.
		bTerminalCleanupGenerated = true;
		return true;
	}

	const FVector UpDirection = MoverComponent->GetUpDirection().GetSafeNormal();
	const FVector PlanarDirection = FVector::VectorPlaneProject(Direction, UpDirection).GetSafeNormal();
	const double ActiveStepMs = FMath::Min(static_cast<double>(Params.TimeStep.StepMs), RemainingDurationMs);
	const double StepFraction = ActiveStepMs / static_cast<double>(Params.TimeStep.StepMs);
	const double SpeedCmPerSecond = static_cast<double>(DistanceCm) / static_cast<double>(DurationSeconds);

	OutProposedMove.LinearVelocity = PlanarDirection * (SpeedCmPerSecond * StepFraction);
	// Keep the native finish rail on OverrideVelocity while preserving movement-mode vertical velocity per substep.
	OutProposedMove.MixMode = EMoveMixMode::OverrideAllExceptVerticalVelocity;
	return true;
}

bool FArcLayeredMove_Knockback::IsFinished(double) const
{
	return bTerminalCleanupGenerated || DurationMs == 0.0f;
}

FLayeredMoveBase* FArcLayeredMove_Knockback::Clone() const
{
	FArcLayeredMove_Knockback* Copy = new FArcLayeredMove_Knockback(*this);
	Copy->SyncNativeDuration();
	return Copy;
}

void FArcLayeredMove_Knockback::NetSerialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		SyncNativeDuration();
	}

	Super::NetSerialize(Ar);
	Ar << Direction;
	Ar << DistanceCm;
	Ar << DurationSeconds;
	uint8 bSerializedTerminalCleanupGenerated = bTerminalCleanupGenerated ? 1 : 0;
	Ar.SerializeBits(&bSerializedTerminalCleanupGenerated, 1);
	bTerminalCleanupGenerated = bSerializedTerminalCleanupGenerated != 0;

	if (Ar.IsLoading())
	{
		SyncNativeDuration();
	}
}

UScriptStruct* FArcLayeredMove_Knockback::GetScriptStruct() const
{
	return FArcLayeredMove_Knockback::StaticStruct();
}

bool FArcLayeredMove_Knockback::CanRepresentNativeDuration() const
{
	return ArcMover::IsKnockbackDurationRepresentable(DurationSeconds);
}

void FArcLayeredMove_Knockback::SyncNativeDuration()
{
	DurationMs = CanRepresentNativeDuration()
		? static_cast<float>(static_cast<double>(DurationSeconds) * 1000.0)
		: 0.0f;
}
