// Copyright Lukasz Baran. All Rights Reserved.

#include "ChaosNavWalkingMode.h"

#include "MoverComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Chaos/ContactModification.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/PhysicsObjectInterface.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Chaos/Character/CharacterGroundConstraintSettings.h"
#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/Character/ChaosCharacterInputs.h"
#include "ChaosMover/Character/Settings/SharedChaosCharacterMovementSettings.h"
#include "ChaosMover/Character/Transitions/ChaosCharacterFallingCheck.h"
#include "ChaosMover/Utilities/ChaosGroundMovementUtils.h"
#include "ChaosMover/Utilities/ChaosMoverQueryUtils.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "Math/UnitConversion.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "PhysicsEngine/PhysicsBodyInstanceOwnerInterface.h"
#include "VisualLogger/VisualLogger.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"

UChaosNavWalkingMode::UChaosNavWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
	GameplayTags.AddTag(Mover_IsOnGround);

	RadialForceLimit = 2000.0f;
	SwingTorqueLimit = 3000.0f;
	TwistTorqueLimit = 1500.0f;

	Transitions.Add(CreateDefaultSubobject<UChaosCharacterFallingCheck>(TEXT("DefaultFallingCheck")));
}

void UChaosNavWalkingMode::UpdateConstraintSettings(Chaos::FCharacterGroundConstraintSettings& ConstraintSettings) const
{
	Super::UpdateConstraintSettings(ConstraintSettings);
	ConstraintSettings.FrictionForceLimit = FUnitConversion::Convert(FrictionForceLimit, EUnit::Newtons, EUnit::KilogramCentimetersPerSecondSquared);
	ConstraintSettings.DampingFactor = GroundDamping;
	ConstraintSettings.MotionTargetMassBias = FractionalGroundReaction;
	ConstraintSettings.RadialForceMotionTargetScaling = FractionalRadialForceLimitScaling;
}

void UChaosNavWalkingMode::GenerateMove_Implementation(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("No Simulation set on ChaosFallingMode"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (!DefaultSimInputs || !CharacterInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("ChaosFallingMode requires FChaosMoverSimulationDefaultInputs and FCharacterDefaultInputs"));
		return;
	}

	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	const FVector UpDirection = DefaultSimInputs->UpDir;

	// Try to use the floor as the basis for the intended move direction (i.e. try to walk along slopes, rather than into them)
	UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard_Mutable();
	FFloorCheckResult LastFloorResult;
	FVector MovementNormal = UpDirection;
	if (!bMaintainHorizontalGroundVelocity && SimBlackboard && SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult) && LastFloorResult.IsWalkableFloor())
	{
		MovementNormal = LastFloorResult.HitResult.ImpactNormal;
	}

	FRotator IntendedOrientation_WorldSpace;
	// If there's no intent from input to change orientation, use the current orientation
	if (!CharacterInputs || CharacterInputs->OrientationIntent.IsNearlyZero())
	{
		IntendedOrientation_WorldSpace = StartingSyncState->GetOrientation_WorldSpace();
	}
	else
	{
		IntendedOrientation_WorldSpace = CharacterInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
	}

	const FQuat WorldToGravityTransform = FQuat::FindBetweenNormals(FVector::UpVector, DefaultSimInputs->UpDir);
	IntendedOrientation_WorldSpace = UMovementUtils::ApplyGravityToOrientationIntent(IntendedOrientation_WorldSpace, WorldToGravityTransform, bShouldCharacterRemainUpright);
	const FVector CurrentVelocity = StartingSyncState->GetVelocity_WorldSpace();

	const FChaosMoverRequestedMoveInputs* RequestedMove = StartState.InputCmd.InputCollection.FindDataByType<FChaosMoverRequestedMoveInputs>();
	if (RequestedMove)
	{
		FRequestedMoveParams Params;

		Params.PriorVelocity = FVector::VectorPlaneProject(CurrentVelocity, MovementNormal);
		Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
		Params.GroundNormal = MovementNormal;
		//Params.TurningRate = SharedSettings->TurningRate;
		Params.MaxSpeed = GetMaxSpeed();
		Params.Acceleration = GetAcceleration();
		//Params.Deceleration = SharedSettings->Deceleration;
		Params.DeltaSeconds = DeltaSeconds;
		Params.WorldToGravityQuat = WorldToGravityTransform;

		Params.bRequestedMoveWithMaxSpeed = RequestedMove->bForceMaxSpeed;
		Params.bShouldComputeAcceleration = RequestedMove->bUseAcceleration;
		Params.RequestedVelocity = RequestedMove->RequestedVelocity;

		OutProposedMove = UChaosGroundMovementUtils::ComputeRequestedMove(Params);
	}
	else
	{
		FGroundMoveParams Params;

		if (CharacterInputs)
		{
			Params.MoveInputType = CharacterInputs->GetMoveInputType();

			const bool bMaintainInputMagnitude = true;
			Params.MoveInput = CharacterInputs->GetMoveInput_WorldSpace();
			//Params.MoveInput = UPlanarConstraintUtils::ConstrainDirectionToPlane(MoverComp->GetPlanarConstraint(), CharacterInputs->GetMoveInput_WorldSpace(), bMaintainInputMagnitude);
		}
		else
		{
			Params.MoveInputType = EMoveInputType::None;
			Params.MoveInput = FVector::ZeroVector;
		}

		Params.OrientationIntent = IntendedOrientation_WorldSpace;
		Params.PriorVelocity = FVector::VectorPlaneProject(CurrentVelocity, MovementNormal);
		Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
		Params.GroundNormal = MovementNormal;
		//Params.TurningRate = SharedSettings->TurningRate;
		//Params.TurningBoost = SharedSettings->TurningBoost;
		Params.MaxSpeed = GetMaxSpeed();
		Params.Acceleration = GetAcceleration();
		//Params.Deceleration = SharedSettings->Deceleration;
		Params.DeltaSeconds = DeltaSeconds;
		Params.WorldToGravityQuat = WorldToGravityTransform;
		Params.UpDirection = UpDirection;
		//Params.bUseAccelerationForVelocityMove = SharedSettings->bUseAccelerationForVelocityMove;

		//if (Params.MoveInput.SizeSquared() > 0.f && !UMovementUtils::IsExceedingMaxSpeed(Params.PriorVelocity, GetMaxSpeed()))
		//{
		//	Params.Friction = SharedSettings->GroundFriction;
		//}
		//else
		//{
		//	Params.Friction = SharedSettings->bUseSeparateBrakingFriction ? SharedSettings->BrakingFriction : SharedSettings->GroundFriction;
		//	Params.Friction *= SharedSettings->BrakingFrictionFactor;
		//}

		OutProposedMove = UGroundMovementUtils::ComputeControlledGroundMove(Params);
	}

	if (SimBlackboard)
	{
		// Update the floor result and check the proposed move to prevent movement onto unwalkable surfaces
		FVector OutDeltaPos = FVector::ZeroVector;
		FFloorCheckResult FloorResult;
		FWaterCheckResult WaterResult;
		GetFloorAndCheckMovement(*StartingSyncState, OutProposedMove, *DefaultSimInputs, DeltaSeconds, FloorResult, WaterResult, OutDeltaPos);

		OutProposedMove.LinearVelocity = (DeltaSeconds > 0.0f) ? (OutDeltaPos / DeltaSeconds) : FVector::ZeroVector;

		SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
		SimBlackboard->Set(CommonBlackboard::LastWaterResult, WaterResult);

		if (bMaintainHorizontalGroundVelocity)
		{
			// So far have assumed we are on level ground, so now add velocity up the slope
			const double CosAngleBetweenUpAndNormal = UpDirection.Dot(FloorResult.HitResult.ImpactNormal);
			if (CosAngleBetweenUpAndNormal > UE_SMALL_NUMBER)
			{
				OutProposedMove.LinearVelocity -= UpDirection * OutProposedMove.LinearVelocity.Dot(FloorResult.HitResult.ImpactNormal) / CosAngleBetweenUpAndNormal;
			}
		}
	};
}

void UChaosNavWalkingMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("No Simulation set on ChaosFallingMode"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	const FCharacterDefaultInputs* CharacterInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (!DefaultSimInputs || !CharacterInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("ChaosFallingMode requires FChaosMoverSimulationDefaultInputs and FCharacterDefaultInputs"));
		return;
	}

	FProposedMove ProposedMove = Params.ProposedMove;
	const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	OutputSyncState = *StartingSyncState;

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	const FVector UpDirection = DefaultSimInputs->UpDir;

	FVector GroundNormal = UpDirection;
	const UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard();
	FFloorCheckResult FloorResult;
	if (SimBlackboard)
	{
		SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult);
		GroundNormal = FloorResult.HitResult.ImpactNormal;
	};

	if (FloorResult.IsWalkableFloor())
	{
		FVector TargetPosition = StartingSyncState->GetLocation_WorldSpace();

		// The base movement mode does not apply gravity in walking mode so apply here.
		// Also remove the gravity that will be applied by the physics simulation.
		// This is so that the gravity in this mode will be consistent with the gravity
		// set on the mover, not the default physics gravity
		const FVector ProjectedVelocity = StartingSyncState->GetVelocity_WorldSpace() + DefaultSimInputs->Gravity * DeltaSeconds;
		FVector TargetVelocity = ProjectedVelocity - DefaultSimInputs->PhysicsObjectGravity * FVector::UpVector * DeltaSeconds;

		// If we have movement intent and not moving straight up/down then use the proposed move plane velocity
		// otherwise just fall with gravity
		constexpr float ParallelCosThreshold = 0.999f;
		const bool bNonVerticalVelocity = !FVector::Parallel(TargetVelocity.GetSafeNormal(), UpDirection, ParallelCosThreshold);
		const bool bUseProposedMove = bNonVerticalVelocity || ProposedMove.bHasDirIntent;
		bool bNormalVelocityIntent = false;

		if (bUseProposedMove)
		{
			const FVector ProposedMovePlaneVelocity = ProposedMove.LinearVelocity - ProposedMove.LinearVelocity.ProjectOnToNormal(GroundNormal);

			// If there is velocity intent in the normal direction then use the velocity from the proposed move. Otherwise
			// retain the previous vertical velocity
			FVector ProposedNormalVelocity = ProposedMove.LinearVelocity - ProposedMovePlaneVelocity;
			if (ProposedNormalVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER)
			{
				TargetVelocity += ProposedNormalVelocity - TargetVelocity.ProjectOnToNormal(GroundNormal);
				bNormalVelocityIntent = true;
			}

			TargetPosition += ProposedMovePlaneVelocity * DeltaSeconds;
		}

		FVector ProjectedGroundVelocity = UChaosGroundMovementUtils::ComputeLocalGroundVelocity_Internal(StartingSyncState->GetLocation_WorldSpace(), FloorResult);
		const Chaos::FPBDRigidParticleHandle* GroundParticle = UChaosGroundMovementUtils::GetRigidParticleHandleFromFloorResult_Internal(FloorResult);
		if (GroundParticle && GroundParticle->IsDynamic() && GroundParticle->GravityEnabled())
		{
			// This might not be correct if different physics objects have different gravity but is saves having to go
			// to the component to get the gravity on the physics volume.
			ProjectedGroundVelocity += DefaultSimInputs->PhysicsObjectGravity * DefaultSimInputs->UpDir * DeltaSeconds;
		}
		const bool bIsGroundMoving = ProjectedGroundVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER;
		const FVector ProjectedRelativeVelocity = TargetVelocity - ProjectedGroundVelocity;
		const float ProjectedRelativeNormalVelocity = FloorResult.HitResult.ImpactNormal.Dot(TargetVelocity - ProjectedGroundVelocity);
		const float ProjectedRelativeVerticalVelocity = GroundNormal.Dot(TargetVelocity - ProjectedGroundVelocity);
		const float VerticalVelocityLimit = bNormalVelocityIntent ? 2.0f / DeltaSeconds : FMath::Abs(GroundNormal.Dot(DefaultSimInputs->Gravity) * DeltaSeconds);

		bool bIsLiftingOffSurface = false;
		if ((ProjectedRelativeNormalVelocity > VerticalVelocityLimit) && bIsGroundMoving && (ProjectedRelativeVerticalVelocity > VerticalVelocityLimit))
		{
			bIsLiftingOffSurface = true;
		}

		// Determine if the character is stepping up or stepping down.
		// If stepping up make sure that the step height is less than the max step height
		// and the new surface has CanCharacterStepUpOn set to true.
		// If stepping down make sure the step height is less than the max step height.
		const float InitialHeightAboveFloor = FloorResult.FloorDist - GetTargetHeight();
		const float EndHeightAboveFloor = InitialHeightAboveFloor + ProjectedRelativeVerticalVelocity * DeltaSeconds;
		const bool bIsSteppingDown = InitialHeightAboveFloor > UE_KINDA_SMALL_NUMBER;
		//const bool bIsWithinReach = EndHeightAboveFloor <= SharedSettings->MaxStepHeight;
		//const bool bIsSupported = bIsWithinReach && !bIsLiftingOffSurface;
		//const bool bNeedsVerticalVelocityToTarget = bIsSupported && bIsSteppingDown && (EndHeightAboveFloor > 0.0f) && !bIsLiftingOffSurface;
		//if (bNeedsVerticalVelocityToTarget)
		//{
		//	TargetVelocity -= FractionalDownwardVelocityToTarget * (EndHeightAboveFloor / DeltaSeconds) * UpDirection;
		//}

		// Put the target position on the floor at the target height
		if (!bIsGroundMoving)
		{
			TargetPosition -= UpDirection * InitialHeightAboveFloor;
		}

		// Target orientation
		// This is always applied regardless of whether the character is supported
		const FRotator TargetOrientation = UMovementUtils::ApplyAngularVelocityToRotator(StartingSyncState->GetOrientation_WorldSpace(), ProposedMove.AngularVelocityDegrees, DeltaSeconds);

		OutputSyncState.SetTransforms_WorldSpace(TargetPosition,
			TargetOrientation,
			TargetVelocity,
			ProposedMove.AngularVelocityDegrees);
	}
	
	OutputState.MovementEndState.RemainingMs = 0.0f;
	OutputState.MovementEndState.NextModeName = Params.StartState.SyncState.MovementMode;
	OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;
}

bool UChaosNavWalkingMode::CanStepUpOnHitSurface(const FFloorCheckResult& FloorResult, const FVector& CharacterVelocity) const
{
	const float StepHeight = GetTargetHeight() - FloorResult.FloorDist;

	bool bWalkable = false; //StepHeight <= SharedSettings->MaxStepHeight;
	constexpr float MinStepHeight = 2.0f;
	const bool SteppingUp = StepHeight > MinStepHeight;
	if (bWalkable && SteppingUp)
	{
		bWalkable = UGroundMovementUtils::CanStepUpOnHitSurface(FloorResult.HitResult);

		// Make sure the surface isn't moving away relative to the character movement when stepping up
		if (const Chaos::FPBDRigidParticleHandle* GroundParticle = UChaosGroundMovementUtils::GetRigidParticleHandleFromFloorResult_Internal(FloorResult))
		{
			const FVector Pos = GroundParticle->IsKinematic() ? FVector(GroundParticle->GetX()) : GroundParticle->GetTransformXRCom().GetLocation();
            const FVector R = FloorResult.HitResult.ImpactPoint - Pos;
            const FVector GroundV = GroundParticle->GetV() + GroundParticle->GetW().Cross(R);
            const FVector Dir = CharacterVelocity.GetSafeNormal();
            float SurfaceV = Chaos::FVec3::VectorPlaneProject(CharacterVelocity - GroundV, FloorResult.HitResult.ImpactNormal).Dot(Dir);
            bWalkable = bWalkable && (SurfaceV > UE_KINDA_SMALL_NUMBER);
		}
	}

	return bWalkable;
}

void UChaosNavWalkingMode::GetFloorAndCheckMovement(const FMoverDefaultSyncState& SyncState, const FProposedMove& ProposedMove, const FChaosMoverSimulationDefaultInputs& DefaultSimInputs, float DeltaSeconds, FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult, FVector& OutDeltaPos) const
{
	FVector DeltaPos = ProposedMove.LinearVelocity * DeltaSeconds;
	OutDeltaPos = DeltaPos;

	const float VelocityPadding = FMath::Max(DefaultSimInputs.UpDir.Dot(SyncState.GetVelocity_WorldSpace()) * DeltaSeconds, 0.0f);

	UE::ChaosMover::Utils::FFloorSweepParams SweepParams{
		.ResponseParams = DefaultSimInputs.CollisionResponseParams,
		.QueryParams = DefaultSimInputs.CollisionQueryParams,
		.Location = SyncState.GetLocation_WorldSpace(),
		.DeltaPos = DeltaPos,
		.UpDir = DefaultSimInputs.UpDir,
		.World = DefaultSimInputs.World,
		.QueryDistance = 0,//GetTargetHeight() + SharedSettings->MaxStepHeight + VelocityPadding,
		.QueryRadius = FMath::Min(GetGroundQueryRadius(), FMath::Max(DefaultSimInputs.PawnCollisionRadius - 5.0f, 0.0f)),
		.MaxWalkSlopeCosine = GetMaxWalkSlopeCosine(),
		.TargetHeight = GetTargetHeight(),
		.CollisionChannel = DefaultSimInputs.CollisionChannel
	};

	// First, try a sweep at the end position
	UE::ChaosMover::Utils::FloorSweep_Internal(SweepParams, OutFloorResult, OutWaterResult);

	if (!OutFloorResult.bBlockingHit)
	{
		// No result at the end position. Fall back on the current floor result
		return;
	}

	bool bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult, ProposedMove.LinearVelocity);
	if (bWalkableFloor)
	{
		// Walkable floor found
		return;
	}

	// Hit something but not walkable. Try a new query to find a walkable surface
	const float StepBlockedHeight = GetTargetHeight() - DefaultSimInputs.PawnCollisionHalfHeight + DefaultSimInputs.PawnCollisionRadius;
	const float StepHeight = GetTargetHeight() - OutFloorResult.FloorDist;

	bool bIsDynamicSurface = false;
	if (const Chaos::FPBDRigidParticleHandle* GroundParticle = UChaosGroundMovementUtils::GetRigidParticleHandleFromFloorResult_Internal(OutFloorResult))
	{
		bIsDynamicSurface = GroundParticle->IsDynamic();
	}

	if ((StepHeight > StepBlockedHeight) || bIsDynamicSurface)
	{
		// Collision should prevent movement. Just try to find ground at start of movement
		SweepParams.QueryRadius = 0.25f * GetGroundQueryRadius();
		SweepParams.DeltaPos = FVector::ZeroVector;

		UE::ChaosMover::Utils::FloorSweep_Internal(SweepParams, OutFloorResult, OutWaterResult);
		OutFloorResult.bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult, ProposedMove.LinearVelocity);
		return;
	}

	if (DeltaPos.SizeSquared() < UE_SMALL_NUMBER)
	{
		// Stationary
		OutDeltaPos = FVector::ZeroVector;
		return;
	}

	// Try to limit the movement to remain on a walkable surface
	FVector HorizSurfaceDir = FVector::VectorPlaneProject(OutFloorResult.HitResult.ImpactNormal, DefaultSimInputs.UpDir);
	float HorizSurfaceDirSizeSq = HorizSurfaceDir.SizeSquared();
	bool bFoundOutwardDir = false;
	if (HorizSurfaceDirSizeSq > UE_SMALL_NUMBER)
	{
		HorizSurfaceDir *= FMath::InvSqrt(HorizSurfaceDirSizeSq);
		bFoundOutwardDir = true;
	}
	else
	{
		// Flat unwalkable surface. Try and get the horizontal direction from the normal instead
		HorizSurfaceDir = FVector::VectorPlaneProject(OutFloorResult.HitResult.Normal, DefaultSimInputs.UpDir);
		HorizSurfaceDirSizeSq = HorizSurfaceDir.SizeSquared();

		if (HorizSurfaceDirSizeSq > UE_SMALL_NUMBER)
		{
			HorizSurfaceDir *= FMath::InvSqrt(HorizSurfaceDirSizeSq);
			bFoundOutwardDir = true;
		}
	}

	if (bFoundOutwardDir)
	{
		const float DP = DeltaPos.Dot(HorizSurfaceDir);
		FVector NewDeltaPos = DeltaPos;
		if (DP > 0.0f)
		{
			// If we're moving away try a ray query at the end of the motion
			SweepParams.QueryRadius = 0.0f;
		}
		else
		{
			// Otherwise, try to find a walkable floor along the surface
			SweepParams.QueryRadius = 0.25f * GetGroundQueryRadius();
			NewDeltaPos = DeltaPos - DP * HorizSurfaceDir;
		}
		SweepParams.DeltaPos = NewDeltaPos;

		UE::ChaosMover::Utils::FloorSweep_Internal(SweepParams, OutFloorResult, OutWaterResult);
		OutFloorResult.bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult, ProposedMove.LinearVelocity);

		if (OutFloorResult.bWalkableFloor)
		{
			OutDeltaPos = NewDeltaPos;
		}
	}
	else
	{
		OutDeltaPos = FVector::ZeroVector;
	}
}

void UChaosNavWalkingMode::ModifyContacts(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, const UE::ChaosMover::FSimulationOutputData& OutputData, Chaos::FCollisionContactModifier& Modifier) const
{
	Super::ModifyContacts(TimeStep, InputData, OutputData, Modifier);

	check(Simulation);

	// Get the updated (character) particle
	Chaos::FGeometryParticleHandle* UpdatedParticle = nullptr;
	const FChaosMoverSimulationDefaultInputs* SimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	if (SimInputs)
	{
		Chaos::FReadPhysicsObjectInterface_Internal ReadInterface = Chaos::FPhysicsObjectInternalInterface::GetRead();
		UpdatedParticle = ReadInterface.GetParticle(SimInputs->PhysicsObject);
	}

	if (!UpdatedParticle)
	{
		return;
	}

	// Try and find the ground particle if there is one in the latest floor result
	Chaos::FGeometryParticleHandle* GroundParticle = nullptr;
	FFloorCheckResult FloorResult;
	FloorResult.FloorDist = 1.0e10;
	if (const UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard())
	{
		SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult);
		Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
		GroundParticle = Interface.GetParticle(FloorResult.HitResult.PhysicsObject);
	}

	const float CharacterHeight = UpdatedParticle->GetX().Dot(SimInputs->UpDir);
	const float EndCapHeight = CharacterHeight - SimInputs->PawnCollisionHalfHeight + SimInputs->PawnCollisionRadius;
	constexpr float CosThetaMax = 0.707f;

	float MinContactHeightStepUps = CharacterHeight - 1.0e10f;
	const float StepDistance = FMath::Abs(GetTargetHeight() - FloorResult.FloorDist);
	if (StepDistance >= UE_KINDA_SMALL_NUMBER)
	{
		//MinContactHeightStepUps = CharacterHeight - GetTargetHeight() + SharedSettings->MaxStepHeight;
	}

	for (Chaos::FContactPairModifier& PairModifier : Modifier.GetContacts(UpdatedParticle))
	{
		const int32 CharacterIdx = UpdatedParticle == PairModifier.GetParticlePair()[0] ? 0 : 1;
		Chaos::FGeometryParticleHandle* OtherParticle = PairModifier.GetOtherParticle(UpdatedParticle);
		if (!OtherParticle)
		{
			continue;
		}
		const bool bOtherParticleIsGround = OtherParticle == GroundParticle;

		for (int32 Idx = 0; Idx < PairModifier.GetNumContacts(); ++Idx)
		{
			Chaos::FVec3 Point0, Point1;
			PairModifier.GetWorldContactLocations(Idx, Point0, Point1);
			Chaos::FVec3 CharacterPoint = CharacterIdx == 0 ? Point0 : Point1;

			Chaos::FVec3 ContactNormal = PairModifier.GetWorldNormal(Idx);
			if ((ContactNormal.Z > CosThetaMax) && CharacterPoint.Z < EndCapHeight)
			{
				// Disable any nearly vertical contact with the end cap of the capsule
				// This will be handled by the character ground constraint
				PairModifier.SetContactPointDisabled(Idx);
			}
			else if (bOtherParticleIsGround && CharacterPoint.Z < MinContactHeightStepUps)
			{
				// In the case of steps ups disable all contacts below the max step height
				PairModifier.SetContactPointDisabled(Idx);
			}
		}
	}
}

void UChaosNavWalkingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	UMoverComponent* MoverComponent = GetMoverComponent();
	CommonLegacySettings = MoverComponent->FindSharedSettings<UCommonLegacyMovementSettings>();
	//ensureMsgf(CommonLegacySettings.IsValid(), TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));

	if (const AActor* MoverCompOwner = MoverComponent->GetOwner())
	{
		NavMoverComponent = MoverCompOwner->FindComponentByClass<UNavMoverComponent>();
	}

	if (!NavMoverComponent.IsValid())
	{
		UE_LOG(LogMover, Warning, TEXT("NavWalkingMode on %s could not find a valid NavMoverComponent and will not function properly."), *GetNameSafe(GetMoverComponent()->GetOwner()));
	}
}

void UChaosNavWalkingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;
	NavMoverComponent = nullptr;
	NavDataInterface = nullptr;

	Super::OnUnregistered();
}


void UChaosNavWalkingMode::Activate(const FMoverEventContext& Context)
{
	Super::Activate(Context);
	
	if (UMoverComponent* MoverComp = GetMoverComponent())
	{
		if (UMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable())
		{
			SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
		}
	}

	NavDataInterface = GetNavData();
	
	NavMeshProjectionTimer = (NavMeshProjectionInterval > 0.f) ? FMath::FRandRange(-NavMeshProjectionInterval, 0.f) : 0.f;
}


const INavigationDataInterface* UChaosNavWalkingMode::GetNavData() const
{
	ANavigationData* NavData = nullptr;

	if (const UWorld* World = GetWorld())
	{
		const UNavigationSystemV1* NavSys = Cast<UNavigationSystemV1>(World->GetNavigationSystem());
		if (NavSys && NavMoverComponent.IsValid())
		{
			const FNavAgentProperties& AgentProps = NavMoverComponent->GetNavAgentPropertiesRef();
			NavData = NavSys->GetNavDataForProps(AgentProps, NavMoverComponent->GetNavLocation());
		}
	}

	return NavData;
}

void UChaosNavWalkingMode::FindBestNavMeshLocation(const FVector& TraceStart, const FVector& TraceEnd, const FVector& CurrentFeetLocation, const FVector& TargetNavLocation, FHitResult& OutHitResult) const
{
	// raycast to underlying mesh to allow us to more closely follow geometry
	// we use static objects here as a best approximation to accept only objects that
	// influence navmesh generation
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectLocation), false);
	Params.AddIgnoredActor(GetMoverComponent()->GetOwner());
	
	// blocked by world static and optionally world dynamic
	FCollisionResponseParams ResponseParams(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Overlap);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldDynamic, bProjectNavMeshOnBothWorldChannels ? ECR_Overlap : ECR_Ignore);

	TArray<FHitResult> MultiTraceHits;
	GetWorld()->LineTraceMultiByChannel(MultiTraceHits, TraceStart, TraceEnd, ECC_WorldStatic, Params, ResponseParams);

	struct FRemoveNotBlockingResponseNavMeshTrace
	{
		FRemoveNotBlockingResponseNavMeshTrace(bool bInCheckOnlyWorldStatic) : bCheckOnlyWorldStatic(bInCheckOnlyWorldStatic) {}

		FORCEINLINE bool operator()(const FHitResult& TestHit) const
		{
			UPrimitiveComponent* PrimComp = TestHit.GetComponent();
			// Prefer using primitive component if valid
			if (const IPhysicsBodyInstanceOwner* BodyInstanceOwner = !PrimComp ? IPhysicsBodyInstanceOwner::GetPhysicsBodyInstanceOwnerFromHitResult(TestHit) : nullptr)
			{
				const bool bBlockOnWorldStatic = (BodyInstanceOwner->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block);
				const bool bBlockOnWorldDynamic = (BodyInstanceOwner->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Block);
				return !bBlockOnWorldStatic && (!bBlockOnWorldDynamic || bCheckOnlyWorldStatic);
			}
			else
			{
				const bool bBlockOnWorldStatic = PrimComp && (PrimComp->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block);
				const bool bBlockOnWorldDynamic = PrimComp && (PrimComp->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Block);
				return !bBlockOnWorldStatic && (!bBlockOnWorldDynamic || bCheckOnlyWorldStatic);
			}
		}

		bool bCheckOnlyWorldStatic;
	};

	MultiTraceHits.RemoveAllSwap(FRemoveNotBlockingResponseNavMeshTrace(!bProjectNavMeshOnBothWorldChannels), EAllowShrinking::No);
	if (MultiTraceHits.Num() > 0)
	{
		FVector UpDirection = GetMoverComponent()->GetUpDirection();
		MultiTraceHits.Sort([UpDirection](const FHitResult& A, const FHitResult& B)
		{
			return A.ImpactPoint.Dot(UpDirection) > B.ImpactPoint.Dot(UpDirection);
		});

		// Cache the closest hit and treat it as a blocking hit (we used an overlap to get all the world static hits so we could sort them ourselves)
		OutHitResult = MultiTraceHits[0];
		OutHitResult.bBlockingHit = true;
	}

#if UE_DEBUG_RECORDING_ENABLED
	const float DebugNormalLength = 50.0f;
	UE_VLOG_SEGMENT(GetMoverComponent(), LogChaosMover, Display, TraceStart, TraceEnd, FColor::Green, TEXT(""));
	for (int i = 0; i < MultiTraceHits.Num(); i++)
	{
		UE_VLOG_SPHERE(GetMoverComponent(), "AsyncNavWalkingMode", Display, MultiTraceHits[i].ImpactPoint, i == 0 ? 5.0f : 2.5f, i == 0 ? FColor::Red : FColor::Yellow, TEXT("%i"), i);
		UE_VLOG_ARROW(GetMoverComponent(), "AsyncNavWalkingMode", Display, MultiTraceHits[i].ImpactPoint, (MultiTraceHits[i].ImpactPoint + DebugNormalLength * MultiTraceHits[i].ImpactNormal), i == 0 ? FColor::Red : FColor::Yellow, TEXT(""));
	}
#endif // UE_DEBUG_RECORDING_ENABLED

	//if (AsyncWalkingModeCVars::bUseNavMeshNormal && CachedNavLocation.HasNodeRef() && MultiTraceHits.Num() > 0)
	//{
	//	FVector NavMeshNormal;
	//	if (NavMovementUtils::CalculateNavMeshNormal(CachedNavLocation, NavMeshNormal, NavDataInterface.Get(), GetMoverComponent()))
	//	{
	//		OutHitResult.ImpactNormal = NavMeshNormal;
	//		OutHitResult.Normal = NavMeshNormal;
	//		UE_VLOG_ARROW(GetMoverComponent(), "AsyncNavWalkingMode", Display, OutHitResult.ImpactPoint, (OutHitResult.ImpactPoint + DebugNormalLength * OutHitResult.ImpactNormal), FColor::Magenta, TEXT("NavMeshNormal"));
	//	}
	//}
}

FVector UChaosNavWalkingMode::ProjectLocationFromNavMesh(float DeltaSeconds, const FVector& CurrentFeetLocation, const FVector& TargetNavLocation, float UpOffset, float DownOffset)
{
	FVector NewLocation = TargetNavLocation;

	const float VerticalOffset = -(DownOffset + UpOffset);
	if (VerticalOffset > -UE_SMALL_NUMBER)
	{
		return NewLocation;
	}

	const UMoverComponent* MoverComp = GetMoverComponent();
	const FVector UpDirection = MoverComp->GetUpDirection();

	const FVector TraceStart = TargetNavLocation + UpOffset * UpDirection;
	const FVector TraceEnd = TargetNavLocation + DownOffset * -UpDirection;

	FFloorCheckResult CachedFloorCheckResult;
	UMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable();
	bool bHasValidFloorResult = SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, CachedFloorCheckResult);
	FHitResult CachedProjectedNavMeshHitResult = CachedFloorCheckResult.HitResult;

	// We can skip this trace if we are checking at the same location as the last trace (ie, we haven't moved).
	const bool bCachedLocationStillValid = (CachedProjectedNavMeshHitResult.bBlockingHit &&
		CachedProjectedNavMeshHitResult.TraceStart == TraceStart &&
		CachedProjectedNavMeshHitResult.TraceEnd == TraceEnd);

	// Check periodically or if we have no information about our last floor result
	UE_VLOG_SPHERE(GetMoverComponent(), LogChaosMover, Display, TargetNavLocation, 5.0f, FColor::Blue, TEXT("TargetNavLocation"));
	NavMeshProjectionTimer -= DeltaSeconds;
	if (NavMeshProjectionTimer <= 0.0f || !bHasValidFloorResult)
	{
		if (!bCachedLocationStillValid)
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("ProjectLocationFromNavMesh(): %s interval: %.3f [SKIP TRACE]"), *GetNameSafe(GetMoverComponent()->GetOwner()), NavMeshProjectionInterval);

			FHitResult HitResult;
			FindBestNavMeshLocation(TraceStart, TraceEnd, CurrentFeetLocation, TargetNavLocation, HitResult);

			// discard result if we were already inside something
			if (HitResult.bStartPenetrating || !HitResult.bBlockingHit)
			{
				CachedProjectedNavMeshHitResult.Reset();
				const FFloorCheckResult EmptyFloorCheckResult;
				SimBlackboard->Set(CommonBlackboard::LastFloorResult, EmptyFloorCheckResult);
			}
			else
			{
				CachedProjectedNavMeshHitResult = HitResult;
				
				FFloorCheckResult FloorCheckResult;
				FloorCheckResult.bBlockingHit = HitResult.bBlockingHit;
				FloorCheckResult.bLineTrace = true;
				FloorCheckResult.bWalkableFloor = true;
				FloorCheckResult.LineDist = FMath::Abs((CurrentFeetLocation - CachedProjectedNavMeshHitResult.ImpactPoint).Dot(UpDirection));
				FloorCheckResult.FloorDist = FloorCheckResult.LineDist; // This is usually set from a sweep trace but it doesn't really hurt setting it. 
				FloorCheckResult.HitResult = CachedProjectedNavMeshHitResult;
				SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorCheckResult);
			}
		}
		else
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("ProjectLocationFromNavMesh(): %s interval: %.3f [SKIP TRACE]"), *GetNameSafe(GetMoverComponent()->GetOwner()), NavMeshProjectionInterval);
		}

		// Wrap around to maintain same relative offset to tick time changes.
		// Prevents large framerate spikes from aligning multiple characters to the same frame (if they start staggered, they will now remain staggered).
		float ModTime = 0.f;
		float Interval = NavMeshProjectionInterval;
		if (Interval > UE_SMALL_NUMBER)
		{
			ModTime = FMath::Fmod(-NavMeshProjectionTimer, Interval);
		}

		NavMeshProjectionTimer = Interval - ModTime;
	}

	// Project to last plane we found.
	if (CachedProjectedNavMeshHitResult.bBlockingHit)
	{
		if (bCachedLocationStillValid && FMath::IsNearlyEqual(CurrentFeetLocation.Dot(UpDirection), CachedProjectedNavMeshHitResult.ImpactPoint.Dot(UpDirection), (FVector::FReal)0.01f))
		{
			// Already at destination.
			UMovementUtils::SetGravityVerticalComponent(NewLocation, CurrentFeetLocation.Dot(UpDirection), UpDirection);
		}
		else
		{
			const FVector ProjectedPoint = FMath::LinePlaneIntersection(TraceStart, TraceEnd, CachedProjectedNavMeshHitResult.ImpactPoint, CachedProjectedNavMeshHitResult.ImpactNormal);
			UE_VLOG_SPHERE(GetMoverComponent(), "AsyncNavWalkingMode", Display, ProjectedPoint, 2.5f, FColor::Orange, TEXT("ProjectedPoint"));
			FVector::FReal ProjectedVertical = ProjectedPoint.Dot(UpDirection);

			// Limit to not be too far above or below NavMesh location
			const FVector::FReal VertTraceStart = TraceStart.Dot(UpDirection);
			const FVector::FReal VertTraceEnd = TraceEnd.Dot(UpDirection);
			const FVector::FReal TraceMin = FMath::Min(VertTraceStart, VertTraceEnd);
			const FVector::FReal TraceMax = FMath::Max(VertTraceStart, VertTraceEnd);
			ProjectedVertical = FMath::Clamp(ProjectedVertical, TraceMin, TraceMax);

			// Interp for smoother updates (less "pop" when trace hits something new). 0 interp speed is instant.
			const FVector::FReal InterpSpeed = FMath::Max<FVector::FReal>(0.f, NavMeshProjectionInterpSpeed);
			ProjectedVertical = FMath::FInterpTo(CurrentFeetLocation.Dot(UpDirection), ProjectedVertical, (FVector::FReal)DeltaSeconds, InterpSpeed);
			ProjectedVertical = FMath::Clamp(ProjectedVertical, TraceMin, TraceMax);

			// Final result
			UMovementUtils::SetGravityVerticalComponent(NewLocation, ProjectedVertical, UpDirection);
		}
	}

	return NewLocation;
}

void UChaosNavWalkingMode::CaptureOutputState(const FMoverDefaultSyncState& StartSyncState, const FVector& FinalLocation, const FRotator& FinalRotation, const FMovementRecord& Record, const FVector& AngularVelocityDegrees, FMoverDefaultSyncState& OutputSyncState, FMoverTickEndData& TickEndData) const
{
	UMoverBlackboard* SimBlackboard = GetMoverComponent()->GetSimBlackboard_Mutable();

	// If we're on a dynamic base and we're not trying to move, keep using the same relative actor location. This prevents slow relative 
	//  drifting that can occur from repeated floor sampling as the base moves through the world.
	SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);

	OutputSyncState.SetTransforms_WorldSpace(FinalLocation,
		FinalRotation,
		Record.GetRelevantVelocity(),
		AngularVelocityDegrees,
		nullptr);	// no movement base

	TickEndData.MovementEndState.bEndedWithNoChanges = OutputSyncState.IsNearlyEqual(StartSyncState);
}