// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcLayeredMove_Lift.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "GameFramework/Actor.h"

FArcLayeredMove_Lift::FArcLayeredMove_Lift()
	: StartLocation(ForceInitToZero)
	, TargetLocation(ForceInitToZero)
	, bRestrictSpeedToExpected(false)
	, PathOffsetCurve(nullptr)
	, TimeMappingCurve(nullptr)
{
	DurationMs = 1000.f;
	MixMode = EMoveMixMode::OverrideVelocity;
}

FVector FArcLayeredMove_Lift::GetPathOffsetInWorldSpace(const float MoveFraction) const
{
	if (PathOffsetCurve)
	{
		float MinCurveTime(0.f);
		float MaxCurveTime(1.f);

		PathOffsetCurve->GetTimeRange(MinCurveTime, MaxCurveTime);
		
		// Calculate path offset
		const FVector PathOffsetInFacingSpace = PathOffsetCurve->GetVectorValue(FMath::GetRangeValue(FVector2f(MinCurveTime, MaxCurveTime), MoveFraction));;
		FRotator FacingRotation((TargetLocation-StartLocation).Rotation());
		FacingRotation.Pitch = 0.f; // By default we don't include pitch in the offset, but an option could be added if necessary
		return FacingRotation.RotateVector(PathOffsetInFacingSpace);
	}

	return FVector::ZeroVector;
}

float FArcLayeredMove_Lift::EvaluateFloatCurveAtFraction(const UCurveFloat& Curve, const float Fraction) const
{
	float MinCurveTime(0.f);
	float MaxCurveTime(1.f);

	Curve.GetTimeRange(MinCurveTime, MaxCurveTime);
	return Curve.GetFloatValue(FMath::GetRangeValue(FVector2f(MinCurveTime, MaxCurveTime), Fraction));
}

bool FArcLayeredMove_Lift::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
#if 0
	OutProposedMove.MixMode = MixMode;

	const float DeltaSeconds = TimeStep.StepMs / 1000.f;
	
	const float RealDurationMs = DurationMs - LiftDurationMs;
	float MoveFraction = (TimeStep.BaseSimTimeMs - StartSimTimeMs) / RealDurationMs;

	if (TimeMappingCurve)
	{
		MoveFraction = EvaluateFloatCurveAtFraction(*TimeMappingCurve, MoveFraction);
	}
	
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	
	FVector CurrentLocation = FVector::ZeroVector;
	if (MoverComp)
	{
		const AActor* MoverActor = MoverComp->GetOwner();
		CurrentLocation = MoverActor->GetActorLocation();	
	}
	else if (StartingSyncState)
	{
		CurrentLocation = StartingSyncState->GetLocation_WorldSpace();
	}
	
	FVector CurrentTargetLocation = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, MoveFraction);
	FVector PathOffset = GetPathOffsetInWorldSpace(MoveFraction);
	CurrentTargetLocation += PathOffset;

	FVector Velocity = (CurrentTargetLocation - CurrentLocation) / DeltaSeconds;

	if (bRestrictSpeedToExpected && !Velocity.IsNearlyZero(UE_KINDA_SMALL_NUMBER))
	{
		// Calculate expected current location (if we didn't have collision and moved exactly where our velocity should have taken us)
		const float PreviousMoveFraction = (TimeStep.BaseSimTimeMs - StartSimTimeMs - TimeStep.StepMs) / DurationMs;
		FVector CurrentExpectedLocation = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, PreviousMoveFraction);
		CurrentExpectedLocation += GetPathOffsetInWorldSpace(PreviousMoveFraction);

		// Restrict speed to the expected speed, allowing some small amount of error
		const FVector ExpectedForce = (CurrentTargetLocation - CurrentExpectedLocation) / DeltaSeconds;
		const float ExpectedSpeed = ExpectedForce.Size();
		const float CurrentSpeedSqr = Velocity.SizeSquared();

		const float ErrorAllowance = 0.5f; // in cm/s
		if (CurrentSpeedSqr > FMath::Square(ExpectedSpeed + ErrorAllowance))
		{
			Velocity.Normalize();
			Velocity *= ExpectedSpeed;
		}
	}
	
	if (CurrentLocation.Equals(TargetLocation, 1.f))
	{
		Velocity = FVector::ZeroVector;
	}
	OutProposedMove.LinearVelocity = Velocity;
	
	return true;
#endif

	OutProposedMove.MixMode = MixMode;

	const float DeltaSeconds = FMath::Max(TimeStep.StepMs / 1000.f, KINDA_SMALL_NUMBER);
	const float AscendDurationMs = FMath::Max(DurationMs - LiftDurationMs, KINDA_SMALL_NUMBER);
	const float OrbitDurationMs = FMath::Max(LiftDurationMs, 0.f);

	const auto SampleDesiredLocation = [this, AscendDurationMs, OrbitDurationMs] (const float SampledSimTimeMs) -> FVector
		{
			const float ElapsedMs = SampledSimTimeMs - StartSimTimeMs;
			if (ElapsedMs <= 0.f)
			{
				return StartLocation;
			}

			const bool bInOrbitPhase = (OrbitDurationMs > 0.f) && (ElapsedMs > AscendDurationMs);
			if (!bInOrbitPhase)
			{
				float LiftFraction = FMath::Clamp(ElapsedMs / AscendDurationMs, 0.f, 1.f);
				if (TimeMappingCurve)
				{
					LiftFraction = EvaluateFloatCurveAtFraction(*TimeMappingCurve, LiftFraction);
				}

				FVector Desired = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, LiftFraction);
				Desired += GetPathOffsetInWorldSpace(LiftFraction);
				return Desired;
			}

			const float OrbitElapsedMs = FMath::Clamp(ElapsedMs - AscendDurationMs, 0.f, OrbitDurationMs);
			const float DirectionFlipIntervalMs = FMath::Max(OrbitDurationMs * 0.25f, 500.f);
			const int32 OrbitSegmentIndex = FMath::FloorToInt(OrbitElapsedMs / DirectionFlipIntervalMs);
			const float DirectionSign = (OrbitSegmentIndex & 1) ?
					-1.f :
					1.f;

			const float AngularSpeedRadPerSec = FMath::DegreesToRadians(60.f);
			const float Angle = DirectionSign * AngularSpeedRadPerSec * (OrbitElapsedMs * 0.001f);

			const float BaseRadius = FMath::Max(50.f, (TargetLocation - StartLocation).Size2D() * 0.35f);
			const float RadiusVariation = 1.f + 0.15f * FMath::Sin(OrbitElapsedMs * 0.0008f);
			const float RadiusX = BaseRadius * RadiusVariation;
			const float RadiusY = RadiusX * 0.85f;

			const float VerticalBob = FMath::Sin(OrbitElapsedMs * 0.0025f) * 15.f;
			const FVector OrbitCenter(StartLocation.X, StartLocation.Y, TargetLocation.Z);

			return OrbitCenter + FVector(
					   RadiusX * FMath::Cos(Angle),
					   RadiusY * FMath::Sin(Angle),
					   VerticalBob);
		};

	const FVector DesiredLocation = SampleDesiredLocation(TimeStep.BaseSimTimeMs);

	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();

	FVector CurrentLocation = FVector::ZeroVector;
	if (MoverComp)
	{
		const AActor* MoverActor = MoverComp->GetOwner();
		CurrentLocation = MoverActor->GetActorLocation();
	}
	else if (StartingSyncState)
	{
		CurrentLocation = StartingSyncState->GetLocation_WorldSpace();
	}

	FVector Velocity = (DesiredLocation - CurrentLocation) / DeltaSeconds;

	if (bRestrictSpeedToExpected && !Velocity.IsNearlyZero(UE_KINDA_SMALL_NUMBER))
	{
		const FVector ExpectedLocation = SampleDesiredLocation(TimeStep.BaseSimTimeMs - TimeStep.StepMs);
		const FVector ExpectedForce = (DesiredLocation - ExpectedLocation) / DeltaSeconds;
		const float ExpectedSpeed = ExpectedForce.Size();
		const float CurrentSpeedSqr = Velocity.SizeSquared();

		const float ErrorAllowance = 0.5f;
		if (CurrentSpeedSqr > FMath::Square(ExpectedSpeed + ErrorAllowance))
		{
			Velocity.Normalize();
			Velocity *= ExpectedSpeed;
		}
	}

	if (CurrentLocation.Equals(DesiredLocation, 1.f))
	{
		Velocity = FVector::ZeroVector;
	}

	OutProposedMove.LinearVelocity = Velocity;
	return true;
}

bool FArcLayeredMove_Lift::GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep
	, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	return GenerateMove(StartState, TimeStep, nullptr, SimBlackboard, OutProposedMove);
}

FLayeredMoveBase* FArcLayeredMove_Lift::Clone() const
{
	FArcLayeredMove_Lift* CopyPtr = new FArcLayeredMove_Lift(*this);
	return CopyPtr;
}

void FArcLayeredMove_Lift::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << LiftDurationMs;
	Ar << StartLocation;
	Ar << TargetLocation;
	Ar << bRestrictSpeedToExpected;
	Ar << PathOffsetCurve;
	Ar << TimeMappingCurve;
}

UScriptStruct* FArcLayeredMove_Lift::GetScriptStruct() const
{
	return FArcLayeredMove_Lift::StaticStruct();
}

FString FArcLayeredMove_Lift::ToSimpleString() const
{
	return FString::Printf(TEXT("Move To"));
}

void FArcLayeredMove_Lift::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}