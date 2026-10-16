// Copyright Lukasz Baran. All Rights Reserved.

#include "Movement/ArcMassSteerToMoveTargetProcessor.h"

#include "Mass/CommandBuffer.h"
#include "Mass/DebugLogging.h"
#include "Mass/Debugger.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassCommonUtils.h"
#include "MassDebuggerWorld.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavigationTypes.h"
#include "MassSimulationLOD.h"
#include "Math/UnrealMathUtility.h"
#include "Movement/ArcMassMoverMovementFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "VisualLogger/VisualLogger.h"
#if WITH_MASSGAMEPLAY_DEBUG
#include "MassNavigationDebug.h"
#endif // WITH_MASSGAMEPLAY_DEBUG

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassSteerToMoveTargetProcessor)

namespace ArcMassSteerToMoveTarget
{
	FVector::FReal CalcDirectionalSpeedScale(const FVector ForwardDirection, const FVector SteerDirection)
	{
		constexpr FVector::FReal ForwardSpeedScale = 1.;
		constexpr FVector::FReal BackwardSpeedScale = 0.25;
		constexpr FVector::FReal SideSpeedScale = 0.5;

		const FVector LeftDirection = FVector::CrossProduct(ForwardDirection, FVector::UpVector);
		const FVector::FReal DirX = FVector::DotProduct(LeftDirection, SteerDirection);
		const FVector::FReal DirY = FVector::DotProduct(ForwardDirection, SteerDirection);

		constexpr FVector::FReal SideA = SideSpeedScale;
		const FVector::FReal SideB = DirY > 0. ? ForwardSpeedScale : BackwardSpeedScale;
		const FVector::FReal Disc = FMath::Square(SideA) * FMath::Square(DirY) + FMath::Square(SideB) * FMath::Square(DirX);
		const FVector::FReal Speed = (Disc > SMALL_NUMBER) ? (SideA * SideB / FMath::Sqrt(Disc)) : 0.;

		return Speed;
	}

	FVector::FReal ArrivalSpeedEnvelope(const FVector::FReal NormalizedDistance)
	{
		return FMath::Sqrt(NormalizedDistance);
	}
} // ArcMassSteerToMoveTarget

UArcMassSteerToMoveTargetProcessor::UArcMassSteerToMoveTargetProcessor()
{
	ExecutionFlags = int32(EProcessorExecutionFlags::AllNetModes);
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks);
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Avoidance);
}

void UArcMassSteerToMoveTargetProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassStandingSteeringFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassGhostLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcMassMovementLimitsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcMassStandBrakeFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassMovingSteeringParameters>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassStandingSteeringParameters>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassSteerToMoveTargetPreventSlowdownTag>(EMassFragmentPresence::Optional);

#if WITH_MASSGAMEPLAY_DEBUG
	EntityQuery.DebugEnableEntityOwnerLogging();
#endif

	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassCodeDrivenMovementTag>(EMassFragmentPresence::Optional);
}

void UArcMassSteerToMoveTargetProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* DebugDrawWorld = UE::Mass::Debug::GetDebugDrawWorld(Context);

	EntityQuery.ForEachEntityChunk(Context, [FallbackLogOwner = this, DebugDrawWorld](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassVelocityFragment> VelocityList = Context.GetFragmentView<FMassVelocityFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassDesiredMovementFragment> MovementList = Context.GetMutableFragmentView<FMassDesiredMovementFragment>();
		const TArrayView<FArcMassForceFragment> ForceList = Context.GetMutableFragmentView<FArcMassForceFragment>();
		const TConstArrayView<FArcMassMovementLimitsFragment> LimitsList = Context.GetFragmentView<FArcMassMovementLimitsFragment>();
		const TArrayView<FMassSteeringFragment> SteeringList = Context.GetMutableFragmentView<FMassSteeringFragment>();
		const TArrayView<FMassStandingSteeringFragment> StandingSteeringList = Context.GetMutableFragmentView<FMassStandingSteeringFragment>();
		const TArrayView<FMassGhostLocationFragment> GhostList = Context.GetMutableFragmentView<FMassGhostLocationFragment>();
		const TArrayView<FArcMassStandBrakeFragment> StandBrakeList = Context.GetMutableFragmentView<FArcMassStandBrakeFragment>();
		const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();
		const FMassMovingSteeringParameters& MovingSteeringParams = Context.GetConstSharedFragment<FMassMovingSteeringParameters>();
		const FMassStandingSteeringParameters& StandingSteeringParams = Context.GetConstSharedFragment<FMassStandingSteeringParameters>();
		const bool bIsCodeDriven = Context.DoesArchetypeHaveTag<FMassCodeDrivenMovementTag>();
		const bool bPreventSteeringSlowdown = Context.DoesArchetypeHaveTag<FMassSteerToMoveTargetPreventSlowdownTag>();

		const FVector::FReal SteerK = 1. / MovingSteeringParams.ReactionTime;
		const float DeltaTime = Context.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTransformFragment& TransformFragment = TransformList[EntityIt];
			const FMassVelocityFragment& VelocityFragment = VelocityList[EntityIt];
			FMassSteeringFragment& Steering = SteeringList[EntityIt];
			FMassStandingSteeringFragment& StandingSteering = StandingSteeringList[EntityIt];
			FMassGhostLocationFragment& Ghost = GhostList[EntityIt];
			FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIt];
			FArcMassForceFragment& Force = ForceList[EntityIt];
			const FArcMassMovementLimitsFragment& Limits = LimitsList[EntityIt];
			FMassDesiredMovementFragment& DesiredMovement = MovementList[EntityIt];
			FArcMassStandBrakeFragment& StandBrake = StandBrakeList[EntityIt];
			const FMassEntityHandle Entity = Context.GetEntity(EntityIt);

			const FTransform& Transform = TransformFragment.GetTransform();

			FVector::FReal SlowdownScale = 1;

			const FVector CurrentLocation = Transform.GetLocation();
			const FVector CurrentForward = Transform.GetRotation().GetForwardVector();

			// Unlike the engine processor, the move target speed follows the Mover's active SpeedScale.
			const FVector::FReal MoveTargetDesiredSpeed = MoveTarget.DesiredSpeed.Get() * Limits.SpeedScale;

			const FVector::FReal LookAheadDistance = FMath::Max<FVector::FReal>(1., MovingSteeringParams.LookAheadTime * MoveTargetDesiredSpeed);

#if WITH_MASSGAMEPLAY_DEBUG
			UE::MassNavigation::Debug::FDebugContext NavigationDebugContext(Context, FallbackLogOwner, LogMassNavigation, DebugDrawWorld, Entity, EntityIt);
			const bool bDisplayDebug = NavigationDebugContext.ShouldLogEntity();
			const UObject* LogOwner = NavigationDebugContext.GetLogOwner();
#endif

			const uint16 CurrentActionID = MoveTarget.GetCurrentActionID();
			if (StandBrake.bBraking && StandBrake.ActionID != CurrentActionID)
			{
				StandBrake.bBraking = false;
			}

			if (MoveTarget.GetCurrentAction() == EMassMovementAction::Move)
			{
				FVector::FReal ArrivalFade = 1.;
				if (MoveTarget.IntentAtGoal == EMassMovementAction::Stand)
				{
					ArrivalFade = FMath::Clamp(MoveTarget.DistanceToGoal / LookAheadDistance, 0., 1.);
				}
				const FVector::FReal SteeringPredictionDistance = LookAheadDistance * ArrivalFade;

				const FVector TargetSide = FVector::CrossProduct(MoveTarget.Forward, FVector::UpVector);
				const FVector Delta = CurrentLocation - MoveTarget.Center;

				const FVector::FReal ForwardOffset = FVector::DotProduct(MoveTarget.Forward, Delta);

				const FVector::FReal SidewaysOffset = FVector::DotProduct(TargetSide, Delta);
				const FVector::FReal SteerForward = FMath::Sqrt(FMath::Max(0., FMath::Square(SteeringPredictionDistance) - FMath::Square(SidewaysOffset)));

				FVector SteerTarget = MoveTarget.Center + MoveTarget.Forward * FMath::Clamp(ForwardOffset + SteerForward, 0., SteeringPredictionDistance);

				FVector SteerDirection = SteerTarget - CurrentLocation;
				SteerDirection.Z = 0.;
				const FVector::FReal DistanceToSteerTarget = SteerDirection.Length();
				if (DistanceToSteerTarget > KINDA_SMALL_NUMBER)
				{
					SteerDirection *= 1. / DistanceToSteerTarget;
				}

#if WITH_MASSGAMEPLAY_DEBUG
				if (bDisplayDebug)
				{
					const FVector ZOffset(0,0,25);
					UE_VLOG_SEGMENT_THICK(LogOwner, LogMassNavigation, Log, CurrentLocation + ZOffset, CurrentLocation + ZOffset + 100.*SteerDirection,
						FColor::Red, /*Thickness*/2, TEXT("SteerDirection"));
				}
#endif //WITH_MASSGAMEPLAY_DEBUG

				FVector::FReal DesiredSpeed = MoveTargetDesiredSpeed;

				if (MovingSteeringParams.bAllowSpeedVariance)
				{
					const FVector::FReal DirSpeedScale = ArcMassSteerToMoveTarget::CalcDirectionalSpeedScale(CurrentForward, SteerDirection);
					DesiredSpeed *= DirSpeedScale;

					FVector::FReal CatchupDesiredSpeed = DesiredSpeed;
					if (ForwardOffset < 0.)
					{
						const FVector::FReal T = FMath::Min(-ForwardOffset / LookAheadDistance, 1.);
						CatchupDesiredSpeed = FMath::Lerp(DesiredSpeed, Limits.MaxSpeed, T);
					}
					else if (ForwardOffset > 0.)
					{
						const FVector::FReal T = FMath::Min(ForwardOffset / LookAheadDistance, 1.);
						CatchupDesiredSpeed = FMath::Lerp(DesiredSpeed, DesiredSpeed * 0., 1. - FMath::Square(1. - T));
					}

					const FVector::FReal DeviantSpeed = FMath::Min(FMath::Abs(SidewaysOffset) / LookAheadDistance, 1.) * DesiredSpeed;

					DesiredSpeed = FMath::Max(CatchupDesiredSpeed, DeviantSpeed);
				}

				if (MoveTarget.IntentAtGoal == EMassMovementAction::Stand)
				{
					const FVector::FReal NormalizedDistanceToSteerTarget = FMath::Clamp(DistanceToSteerTarget / LookAheadDistance, 0., 1.);
					DesiredSpeed *= ArcMassSteerToMoveTarget::ArrivalSpeedEnvelope(FMath::Max(ArrivalFade, NormalizedDistanceToSteerTarget));
				}

				constexpr FVector::FReal FallingBehindScale = 0.8;
				if (MoveTarget.EntityDistanceToGoal != FMassMoveTargetFragment::UnsetDistance)
				{
					MoveTarget.bSteeringFallingBehind = (MoveTarget.EntityDistanceToGoal - MoveTarget.DistanceToGoal) > LookAheadDistance * FallingBehindScale;
				}
				else
				{
					MoveTarget.bSteeringFallingBehind = ForwardOffset < -LookAheadDistance * FallingBehindScale;
				}

				Steering.DesiredVelocity = SteerDirection * DesiredSpeed;

				Force.Value = SteerK * (Steering.DesiredVelocity - DesiredMovement.DesiredVelocity);

				if (bPreventSteeringSlowdown && DesiredMovement.DesiredVelocity.Size() <= MovementParams.DefaultDesiredSpeed)
				{
					const FVector::FReal DistanceToMoveTarget = FVector::Dist(CurrentLocation, MoveTarget.Center);
					const FVector::FReal ExtraDistance = FMath::Max(0, DistanceToMoveTarget - (1.1*LookAheadDistance));
					SlowdownScale = 1 - FMath::Clamp(ExtraDistance/MovingSteeringParams.SteeringPreventSlowdownAttenuationDistance, 0, 1);

					const FVector DesiredVelocityNormal = DesiredMovement.DesiredVelocity.GetSafeNormal();
					const FVector::FReal ProjectedForceSize = Force.Value.Dot(DesiredVelocityNormal);
					if (ProjectedForceSize < 0)
					{
						Force.Value += SlowdownScale * FMath::Abs(ProjectedForceSize) * DesiredVelocityNormal;
					}
				}
			}
			else if (MoveTarget.GetCurrentAction() == EMassMovementAction::Stand)
			{
				const FVector::FReal PerEntityScale = UE::RandomSequence::FRand(Entity.Index);
				const FVector::FReal TargetMoveThreshold = StandingSteeringParams.TargetMoveThreshold * (1. - StandingSteeringParams.TargetMoveThresholdVariance + PerEntityScale * StandingSteeringParams.TargetMoveThresholdVariance * 2.);

				if (Ghost.LastSeenActionID != CurrentActionID)
				{
					// A Stand issued at speed anchors at the predicted stop point and brakes, instead of steering back to the issued Center.
					const FVector::FReal BeginSpeed = DesiredMovement.DesiredVelocity.Size();
					if (BeginSpeed >= StandingSteeringParams.LowSpeedThreshold && BeginSpeed > KINDA_SMALL_NUMBER)
					{
						const FVector::FReal Deceleration = FMath::Max<FVector::FReal>(Limits.BrakingDeceleration, KINDA_SMALL_NUMBER);
						const FVector::FReal StopDistance = FMath::Square(BeginSpeed) / (2. * Deceleration) + BeginSpeed * Limits.BrakingLag;
						MoveTarget.Center = CurrentLocation + (DesiredMovement.DesiredVelocity / BeginSpeed) * StopDistance;
						StandBrake.bBraking = true;
						StandBrake.ActionID = CurrentActionID;
					}

					Ghost.Location = MoveTarget.Center;
					Ghost.Velocity = FVector::ZeroVector;
					Ghost.LastSeenActionID = CurrentActionID;

					StandingSteering.TargetLocation = MoveTarget.Center;
					StandingSteering.TrackedTargetSpeed = 0.0f;
					StandingSteering.bIsUpdatingTarget = false;
					StandingSteering.TargetSelectionCooldown = StandingSteeringParams.TargetSelectionCooldown * FMath::RandRange(1.f - StandingSteeringParams.TargetSelectionCooldownVariance, 1.f + StandingSteeringParams.TargetSelectionCooldownVariance);
					StandingSteering.bEnteredFromMoveAction = MoveTarget.GetPreviousAction() == EMassMovementAction::Move;
				}

				if (StandBrake.bBraking)
				{
					const FVector::FReal BrakeSpeed = DesiredMovement.DesiredVelocity.Size();
					if (BrakeSpeed < StandingSteeringParams.LowSpeedThreshold || BrakeSpeed <= KINDA_SMALL_NUMBER)
					{
						StandBrake.bBraking = false;
						DesiredMovement.DesiredVelocity = FVector::ZeroVector;
						Force.Value = FVector::ZeroVector;

						MoveTarget.Center = CurrentLocation;
						Ghost.Location = CurrentLocation;
						Ghost.Velocity = FVector::ZeroVector;
						StandingSteering.TargetLocation = CurrentLocation;
					}
					else
					{
						const FVector::FReal MaxBrake = DeltaTime > 0.f ? BrakeSpeed / DeltaTime : BrakeSpeed;
						Steering.DesiredVelocity = FVector::ZeroVector;
						Force.Value = -(DesiredMovement.DesiredVelocity / BrakeSpeed) * FMath::Min<FVector::FReal>(Limits.BrakingDeceleration, MaxBrake);
					}
				}

				if (!StandBrake.bBraking)
				{
					StandingSteering.TargetSelectionCooldown = FMath::Max(0.0f, StandingSteering.TargetSelectionCooldown - DeltaTime);

					if (!StandingSteering.bIsUpdatingTarget)
					{
						if (StandingSteering.TargetSelectionCooldown <= 0.0f
							&& FVector::DistSquared(StandingSteering.TargetLocation, Ghost.Location) > FMath::Square(TargetMoveThreshold))
						{
							StandingSteering.TargetLocation = Ghost.Location;
							StandingSteering.TrackedTargetSpeed = 0.0f;
							StandingSteering.bIsUpdatingTarget = true;
							StandingSteering.bEnteredFromMoveAction = false;
						}
					}
					else
					{
						StandingSteering.TargetLocation = Ghost.Location;
						const FVector::FReal GhostSpeed = Ghost.Velocity.Length();

						if (GhostSpeed > (StandingSteering.TrackedTargetSpeed * StandingSteeringParams.TargetSpeedHysteresisScale))
						{
							const FVector::FReal TrackedTargetSpeed = FMath::Max(StandingSteering.TrackedTargetSpeed, GhostSpeed);
							StandingSteering.TrackedTargetSpeed = static_cast<float>(TrackedTargetSpeed);
						}
						else
						{
							StandingSteering.TargetSelectionCooldown = StandingSteeringParams.TargetSelectionCooldown * FMath::RandRange(1.0f - StandingSteeringParams.TargetSelectionCooldownVariance, 1.0f + StandingSteeringParams.TargetSelectionCooldownVariance);
							StandingSteering.bIsUpdatingTarget = false;
						}
					}

					FVector SteerDirection = FVector::ZeroVector;
					FVector::FReal DesiredSpeed = 0.;

					FVector Delta = StandingSteering.TargetLocation - CurrentLocation;
					Delta.Z = 0.;
					const FVector::FReal Distance = Delta.Size();
					if (Distance > StandingSteeringParams.DeadZoneRadius)
					{
#if WITH_MASSGAMEPLAY_DEBUG
						if (bDisplayDebug)
						{
							UE_VLOG_UELOGF(LogOwner, LogMassNavigation, Verbose, "Standing steering: out of deadzone (Distance: %.2f)", Distance);
						}
#endif

						SteerDirection = Delta / Distance;
						if (StandingSteering.bEnteredFromMoveAction)
						{
							const FVector::FReal Range = FMath::Max(1., LookAheadDistance - StandingSteeringParams.DeadZoneRadius);
							const FVector::FReal SpeedFade = FMath::Clamp((Distance - StandingSteeringParams.DeadZoneRadius) / Range, 0., 1.);
							DesiredSpeed = MoveTargetDesiredSpeed * ArcMassSteerToMoveTarget::CalcDirectionalSpeedScale(CurrentForward, SteerDirection) * ArcMassSteerToMoveTarget::ArrivalSpeedEnvelope(SpeedFade);
						}
						else
						{
							const FVector::FReal Range = FMath::Max(1., LookAheadDistance - StandingSteeringParams.DeadZoneRadius);
							const FVector::FReal SpeedFade = FMath::Clamp((Distance - StandingSteeringParams.DeadZoneRadius) / Range, 0., 1.);
							DesiredSpeed = MoveTargetDesiredSpeed * ArcMassSteerToMoveTarget::ArrivalSpeedEnvelope(SpeedFade);
						}

						Steering.DesiredVelocity = SteerDirection * DesiredSpeed;
						Force.Value = SteerK * (Steering.DesiredVelocity - DesiredMovement.DesiredVelocity);
						Force.Value = Force.Value.GetClampedToMaxSize(Limits.MaxAcceleration);
					}
					else
					{
						if (DesiredMovement.DesiredVelocity.SquaredLength() < FMath::Square(StandingSteeringParams.LowSpeedThreshold))
						{
							DesiredMovement.DesiredVelocity = FVector::ZeroVector;
							Force.Value = FVector::ZeroVector;
						}
					}
				}

				MoveTarget.bSteeringFallingBehind = false;
			}
			else if (MoveTarget.GetCurrentAction() == EMassMovementAction::Animate)
			{
				Steering.Reset();
				MoveTarget.bSteeringFallingBehind = false;
				Force.Value = FVector::ZeroVector;

				if (bIsCodeDriven)
				{
					DesiredMovement.DesiredVelocity = FVector::ZeroVector;
				}
				else
				{
					Steering.DesiredVelocity = VelocityFragment.Value;
					DesiredMovement.DesiredVelocity = VelocityFragment.Value;
					if (!DesiredMovement.DesiredVelocity.IsNearlyZero())
					{
						DesiredMovement.DesiredFacing = FQuat(DesiredMovement.DesiredVelocity.Rotation());
					}
				}
			}

#if WITH_MASSGAMEPLAY_DEBUG
			FColor EntityColor = FColor::White;
			if (bDisplayDebug)
			{
				const FVector ZOffset(0,0,25);

				const FColor LightEntityColor = UE::MassNavigation::Debug::MixColors(EntityColor, FColor::White);

				const FVector MoveTargetCenter = MoveTarget.Center + ZOffset;

				UE_VLOG_CIRCLE_THICK(LogOwner, LogMassNavigation, Log, MoveTargetCenter, FVector::UpVector, 5.f, EntityColor, /*Thickness*/2,
					TEXT("MoveTarget\ngoal: %s"), *UEnum::GetDisplayValueAsText(MoveTarget.IntentAtGoal).ToString());

				UE_VLOG_CIRCLE_THICK(LogOwner, LogMassNavigation, Log, Ghost.Location, FVector::UpVector, 5.f, FColor::Silver, /*Thickness*/2, TEXT("Ghost"));

				UE_VLOG_SEGMENT_THICK(LogOwner, LogMassNavigation, Log, MoveTargetCenter, MoveTargetCenter + MoveTarget.Forward * 100, EntityColor, /*Thickness*/1,
					TEXT("MoveTarget\nforward"));

				if (MoveTargetDesiredSpeed > 0.f && FVector::Dist2D(CurrentLocation, MoveTarget.Center) > LookAheadDistance * 1.5f)
				{
					UE_VLOG_SEGMENT_THICK(LogOwner, LogMassNavigation, Log, MoveTargetCenter, CurrentLocation + ZOffset, FColor::Red, /*Thickness*/1, TEXT("LOST"));
				}
				else
				{
					UE_VLOG_SEGMENT_THICK(LogOwner, LogMassNavigation, Log, MoveTargetCenter, CurrentLocation + ZOffset, LightEntityColor, /*Thickness*/1, TEXT(""));
				}

				UE_VLOG_SEGMENT_THICK(LogOwner, LogMassNavigation, Log, CurrentLocation + ZOffset, CurrentLocation + ZOffset + DesiredMovement.DesiredVelocity, FColor::Yellow, /*Thickness*/4,
					TEXT("Mvt DesiredVelocity %.1f"), DesiredMovement.DesiredVelocity.Length());

				UE_VLOG_SEGMENT_THICK(LogOwner, LogMassNavigation, Log, CurrentLocation + ZOffset + FVector(0,0,2),
					CurrentLocation + Steering.DesiredVelocity + ZOffset + FVector(0,0,2), FColor::Orange, /*Thickness*/4,
					TEXT("Steering DesiredVelocity %.1f"), Steering.DesiredVelocity.Length());

				UE_VLOG_SEGMENT_THICK(LogOwner, LogMassNavigation, Log, CurrentLocation + ZOffset, CurrentLocation + Force.Value + ZOffset, FColor::Emerald, /*Thickness*/4,
					TEXT("Steering Force %.1f"), Force.Value.Length());

				constexpr FVector::FReal SlowdownScaleDisplayThreshold = 0.95;
				if (SlowdownScale < SlowdownScaleDisplayThreshold)
				{
					UE_VLOG_CIRCLE_THICK(FallbackLogOwner, LogMassNavigation, Log, CurrentLocation, FVector::UpVector, (1-SlowdownScale)*100, FColor::Red, /*Thickness*/2,
						TEXT("Slowdown scale: %.0f"), SlowdownScale);
				}
			}
#endif // WITH_MASSGAMEPLAY_DEBUG
		}
	});
}
