/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "Components/ArcAsyncProjectileMovementComp.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

void UArcAsyncProjectileMovementComp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// Fast Arrays don't use push model, but there's no harm in marking them with it.
	// The flag will just be ignored.
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UArcAsyncProjectileMovementComp
		, RepProjectileMove
		, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UArcAsyncProjectileMovementComp
		, RepLocalVelocity
		, Params);

	// DOREPLIFETIME_WITH_PARAMS_FAST(UAbilitySystemComponent, ReplicatedLooseTags,
	// Params);

	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void UArcAsyncProjectileMovementComp::OnRep_AsyncNewLocation(const FPepProjectileMovemnt& OldAsycNewLocation)
{
	UE_LOG(LogTemp
		, Log
		, TEXT("AsyncNewLocation %s")
		, *RepProjectileMove.AsyncNewLocation.ToString());
	UE_LOG(LogTemp
		, Log
		, TEXT("LocalAsyncNewLocation %s")
		, *LocalAsyncNewLocation.ToString());

	UE_LOG(LogTemp
		, Log
		, TEXT("Rep Velocity %f")
		, RepProjectileMove.Velocity);
}

void UArcAsyncProjectileMovementComp::OnRep_RepLocalVelocity(const FVector& OldRepLocalVelocity)
{
	Velocity = RepLocalVelocity;
}

void FArcAsyncActorComponentTickFunction::ExecuteTick(float DeltaTime
													  , enum ELevelTick TickType
													  , ENamedThreads::Type CurrentThread
													  , const FGraphEventRef& MyCompletionGraphEvent)
{
	// TRACE_CPUPROFILER_EVENT_SCOPE(FActorComponentTickFunction::ExecuteTick);
	// ExecuteTickHelper(Target, Target->bTickInEditor, DeltaTime, TickType, [this,
	// TickType](float DilatedTime)
	{
		Target->AsyncPhysicsTickComponent(DeltaTime
			, DeltaTime); // , this);
	} //);
}

FString FArcAsyncActorComponentTickFunction::DiagnosticMessage()
{
	return Target->GetFullName() + TEXT("[TickComponent]");
}

FName FArcAsyncActorComponentTickFunction::DiagnosticContext(bool bDetailed)
{
	if (bDetailed)
	{
		AActor* OwningActor = Target->GetOwner();
		FString OwnerClassName = OwningActor ? OwningActor->GetClass()->GetName() : TEXT("None");
		// Format is "ComponentClass/OwningActorClass/ComponentName"
		FString ContextString = FString::Printf(TEXT("%s/%s/%s")
			, *Target->GetClass()->GetName()
			, *OwnerClassName
			, *Target->GetName());
		return FName(*ContextString);
	}
	else
	{
		return Target->GetClass()->GetFName();
	}
}

// Sets default values for this component's properties
UArcAsyncProjectileMovementComp::UArcAsyncProjectileMovementComp()
{
	// Set this component to be initialized when the game starts, and to be ticked every
	// frame.  You can turn these features off to improve performance if you don't need
	// them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = true;

	AsyncPrimaryComponentTick.bCanEverTick = true;
	AsyncPrimaryComponentTick.bStartWithTickEnabled = true;
	AsyncPrimaryComponentTick.bAllowTickOnDedicatedServer = true;
	AsyncPrimaryComponentTick.bRunOnAnyThread = true;
	AsyncPrimaryComponentTick.TickInterval = 0.033f;
	// TODO 5.1 update
	// bAsyncPhysicsTickEnabled = true;

	bUpdateOnlyIfRendered = false;
	bInitialVelocityInLocalSpace = true;
	bForceSubStepping = false;
	bSimulationEnabled = true;
	bSweepCollision = true;
	bInterpMovement = false;
	bInterpRotation = false;
	bInterpolationComplete = true;
	InterpLocationTime = 0.100f;
	InterpRotationTime = 0.050f;
	InterpLocationMaxLagDistance = 300.0f;
	InterpLocationSnapToTargetDistance = 500.0f;

	Velocity = FVector(1.f
		, 0.f
		, 0.f);

	ProjectileGravityScale = 1.f;

	Bounciness = 0.6f;
	Friction = 0.2f;
	BounceVelocityStopSimulatingThreshold = 5.f;
	MinFrictionFraction = 0.0f;

	HomingAccelerationMagnitude = 0.f;

	bWantsInitializeComponent = true;
	bComponentShouldUpdatePhysicsVolume = false;

	MaxSimulationTimeStep = 0.05f;
	MaxSimulationIterations = 4;
	BounceAdditionalIterations = 1;

	bBounceAngleAffectsFriction = false;
	bIsSliding = false;
	PreviousHitTime = 1.f;
	PreviousHitNormal = FVector::UpVector;
	SetIsReplicatedByDefault(true);
	// ...
}

// Called when the game starts
void UArcAsyncProjectileMovementComp::BeginPlay()
{
	Super::BeginPlay();

	// ...
}

// Called every frame
void UArcAsyncProjectileMovementComp::TickComponent(float DeltaTime
													, ELevelTick TickType
													, FActorComponentTickFunction* ThisTickFunction)
{
	// TODO we maye use MASS to calculate interpolation.
	Super::TickComponent(DeltaTime
		, TickType
		, ThisTickFunction);
	FHitResult Hit(ForceInit);
	// TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags,
	// MoveComponentFlags | MOVECOMP_NeverIgnoreBlockingOverlaps);
	// MoveUpdatedComponent(MoveDelta, FVector::ZeroVector, bSweepCollision, &Hit);
	if (UpdatedComponent != nullptr && RepProjectileMove.AsyncNewLocation.Size() > 0)
	{
		FVector CurrentLocation = UpdatedComponent->GetComponentLocation();
		FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation
			, RepProjectileMove.AsyncNewLocation
			, DeltaTime
			, RepProjectileMove.Velocity);

		FVector MoveDelta = NewLocation - CurrentLocation;

		TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags
			, MoveComponentFlags | MOVECOMP_NeverIgnoreBlockingOverlaps);
		MoveUpdatedComponent(MoveDelta
			, FRotator::ZeroRotator
			, bSweepCollision
			, &Hit);

		// const float RemainingTime = GetWorld()->GetTimeSeconds() - LastUpdateTime;
		////const float CurrentSmoothTime = TargetDelta - RemainingTime;
		// float LerpPercent = FMath::Clamp(RemainingTime/DeltaTime, 0.0f, 1);
		//
		// double Distance = FMath::Min(FVector::Distance(CurrentLocation,
		// AsyncNewLocation), 1);
		//
		// CurrentLocation = FMath::LerpStable(CurrentLocation, AsyncNewLocation,
		// LerpPercent); UpdatedComponent->SetWorldLocation(CurrentLocation);
		if (GetNetMode() == ENetMode::NM_DedicatedServer)
		{
			UE_LOG(LogTemp
				, Log
				, TEXT("Server Location %s")
				, *CurrentLocation.ToString());
			UE_LOG(LogTemp
				, Log
				, TEXT("Server Velocity %f")
				, RepProjectileMove.Velocity);
			// Correction ?
		}
	}
	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		FVector CurrentLocation = UpdatedComponent->GetComponentLocation();
		UE_LOG(LogTemp
			, Log
			, TEXT("Client Location %s")
			, *CurrentLocation.ToString());
		UE_LOG(LogTemp
			, Log
			, TEXT("Client Velocity %f")
			, RepProjectileMove.Velocity);
		UE_LOG(LogTemp
			, Log
			, TEXT("Client Vector Velocity %s")
			, *Velocity.ToString());
		// Correction ?
	}

	// ...
}

void UArcAsyncProjectileMovementComp::AsyncPhysicsTickComponent(float DeltaTime
																, float SimTime)
{
	if (bInterpMovement && !bInterpolationComplete)
	{
		// QUICK_SCOPE_CYCLE_COUNTER(STAT_ProjectileMovementComponent_TickInterpolation);
		TickInterpolation(DeltaTime);
	}

	// Consume PendingForce and reset to zero.
	// At this point, any calls to AddForce() will apply to the next frame.
	PendingForceThisUpdate = PendingForce;
	ClearPendingForce();

	// skip if don't want component updated when not rendered or updated component can't
	// move
	if (HasStoppedSimulation() || ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	if (!IsValid(UpdatedComponent) || !bSimulationEnabled)
	{
		return;
	}

	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (!ActorOwner || !CheckStillInWorld())
	{
		return;
	}

	if (UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	float RemainingTime = DeltaTime;
	int32 NumImpacts = 0;
	int32 NumBounces = 0;
	int32 LoopCount = 0;
	int32 Iterations = 0;
	FHitResult Hit(1.f);

	/// while (bSimulationEnabled && RemainingTime >= MIN_TICK_TIME && (Iterations <
	/// MaxSimulationIterations) && IsValid(ActorOwner) && !HasStoppedSimulation())
	{
		LoopCount++;
		Iterations++;

		// subdivide long ticks to more closely follow parabolic trajectory
		const float InitialTimeRemaining = RemainingTime;
		const float TimeTick = DeltaTime;
		// ShouldUseSubStepping() ? GetSimulationTimeStep(RemainingTime, Iterations) :
		// RemainingTime;
		RemainingTime -= TimeTick;

		// Logging
		// UE_LOG(LogProjectileMovement, Verbose, TEXT("Projectile %s: (Role: %d,
		// Iteration %d, step %.3f, [%.3f / %.3f] cur/total) sim (Pos %s, Vel %s)"),
		// *GetNameSafe(ActorOwner), (int32)ActorOwner->GetLocalRole(), LoopCount,
		// TimeTick, FMath::Max(0.f, DeltaTime - InitialTimeRemaining), DeltaTime,
		//	*UpdatedComponent->GetComponentLocation().ToString(), *Velocity.ToString());

		// Initial move state
		Hit.Time = 1.f;
		const FVector OldVelocity = Velocity;
		const FVector MoveDelta = ComputeMoveDelta(OldVelocity
			, TimeTick);
		FQuat NewRotation = (bRotationFollowsVelocity && !OldVelocity.IsNearlyZero(0.01f))
							? OldVelocity.ToOrientationQuat()
							: UpdatedComponent->GetComponentQuat();

		if (bRotationFollowsVelocity && bRotationRemainsVertical)
		{
			FRotator DesiredRotation = NewRotation.Rotator();
			DesiredRotation.Pitch = 0.0f;
			DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
			DesiredRotation.Roll = 0.0f;
			NewRotation = DesiredRotation.Quaternion();
		}

		// Move the component
		if (bShouldBounce)
		{
			// If we can bounce, we are allowed to move out of penetrations, so use
			// SafeMoveUpdatedComponent which does that automatically.
			SafeMoveUpdatedComponent(MoveDelta
				, NewRotation
				, bSweepCollision
				, Hit);
		}
		else
		{
			AsyncMoveDelta = MoveDelta;
			// If we can't bounce, then we shouldn't adjust if initially penetrating,
			// because that should be a blocking hit that causes a hit event and stop
			// simulation. TGuardValue<EMoveComponentFlags>
			// ScopedFlagRestore(MoveComponentFlags, MoveComponentFlags |
			// MOVECOMP_NeverIgnoreBlockingOverlaps); MoveUpdatedComponent(MoveDelta,
			// NewRotation, bSweepCollision, &Hit);

			AsyncNewLocation = UpdatedComponent->GetComponentLocation() + MoveDelta;
			LocalAsyncNewLocation = UpdatedComponent->GetComponentLocation() + MoveDelta;
			// if (GetNetMode() == ENetMode::NM_DedicatedServer)
		}

		// If we hit a trigger that destroyed us, abort.
		if (!IsValid(ActorOwner) || HasStoppedSimulation())
		{
			return;
		}

		// Handle hit result after movement
		if (!Hit.bBlockingHit)
		{
			PreviousHitTime = 1.f;
			bIsSliding = false;

			// Only calculate new velocity if events didn't change it during the movement
			// update.
			if (Velocity == OldVelocity)
			{
				Velocity = ComputeVelocity(Velocity
					, TimeTick);
			}

			// Logging
			// UE_LOG(LogProjectileMovement, VeryVerbose, TEXT("Projectile %s: (Role: %d,
			// Iteration %d, step %.3f) no hit (Pos %s, Vel %s)"),
			// *GetNameSafe(ActorOwner), (int32)ActorOwner->GetLocalRole(), LoopCount,
			// TimeTick,
			//*UpdatedComponent->GetComponentLocation().ToString(), *Velocity.ToString());
		}
		else
		{
			// Only calculate new velocity if events didn't change it during the movement
			// update.
			if (Velocity == OldVelocity)
			{
				// re-calculate end velocity for partial time
				Velocity = (Hit.Time > KINDA_SMALL_NUMBER)
						   ? ComputeVelocity(OldVelocity
							   , TimeTick * Hit.Time)
						   : OldVelocity;
			}

			// Logging
			// UE_CLOG(UpdatedComponent != nullptr, LogProjectileMovement, VeryVerbose,
			// TEXT("Projectile %s: (Role: %d, Iteration %d, step %.3f) new hit at t=%.3f:
			// (Pos %s, Vel %s)"), 	*GetNameSafe(ActorOwner),
			//(int32)ActorOwner->GetLocalRole(), LoopCount, TimeTick, Hit.Time,
			//*UpdatedComponent->GetComponentLocation().ToString(), *Velocity.ToString());

			// Handle blocking hit
			NumImpacts++;
			float SubTickTimeRemaining = TimeTick * (1.f - Hit.Time);
			const EHandleBlockingHitResult HandleBlockingResult = HandleBlockingHit(Hit
				, TimeTick
				, MoveDelta
				, SubTickTimeRemaining);
			if (HandleBlockingResult == EHandleBlockingHitResult::Abort || HasStoppedSimulation())
			{
			}
			else if (HandleBlockingResult == EHandleBlockingHitResult::Deflect)
			{
				NumBounces++;
				HandleDeflection(Hit
					, OldVelocity
					, NumBounces
					, SubTickTimeRemaining);
				PreviousHitTime = Hit.Time;
				PreviousHitNormal = ConstrainNormalToPlane(Hit.Normal);
			}
			else if (HandleBlockingResult == EHandleBlockingHitResult::AdvanceNextSubstep)
			{
				// Reset deflection logic to ignore this hit
				PreviousHitTime = 1.f;
			}
			else
			{
				// Unhandled EHandleBlockingHitResult
				checkNoEntry();
			}

			// Logging
			// UE_CLOG(UpdatedComponent != nullptr, LogProjectileMovement, VeryVerbose,
			// TEXT("Projectile %s: (Role: %d, Iteration %d, step %.3f) deflect at t=%.3f:
			// (Pos %s, Vel %s)"), 	*GetNameSafe(ActorOwner),
			//(int32)ActorOwner->GetLocalRole(), Iterations, TimeTick, Hit.Time,
			//*UpdatedComponent->GetComponentLocation().ToString(), *Velocity.ToString());

			// Add unprocessed time after impact
			if (SubTickTimeRemaining >= MIN_TICK_TIME)
			{
				RemainingTime += SubTickTimeRemaining;

				// A few initial impacts should possibly allow more iterations to complete
				// more of the simulation.
				if (NumImpacts <= BounceAdditionalIterations)
				{
					Iterations--;

					// Logging
					// UE_LOG(LogProjectileMovement, Verbose, TEXT("Projectile %s: (Role:
					// %d, Iteration %d, step %.3f) allowing extra iteration after bounce
					// %u (t=%.3f, adding %.3f secs)"), *GetNameSafe(ActorOwner),
					//(int32)ActorOwner->GetLocalRole(), LoopCount, TimeTick, NumBounces,
					// Hit.Time,
					// SubTickTimeRemaining);
				}
			}
		}
	}
	{
		RepProjectileMove.AsyncNewLocation = AsyncNewLocation;
		RepProjectileMove.Velocity = Velocity.Size();
		RepLocalVelocity = Velocity;

		MARK_PROPERTY_DIRTY_FROM_NAME(UArcAsyncProjectileMovementComp, RepProjectileMove, this)
		MARK_PROPERTY_DIRTY_FROM_NAME(UArcAsyncProjectileMovementComp, RepLocalVelocity, this)
		LastUpdateTime = GetWorld()->GetTimeSeconds();
		GetOwner()->ForceNetUpdate();
	}
	UpdateComponentVelocity();
}

///////////////////
//////////////////////
///

const float UArcAsyncProjectileMovementComp::MIN_TICK_TIME = 1e-6f;

void UArcAsyncProjectileMovementComp::PostLoad()
{
	Super::PostLoad();

	const FPackageFileVersion LinkerUEVer = GetLinkerUEVersion();

	if (LinkerUEVer < VER_UE4_REFACTOR_PROJECTILE_MOVEMENT)
	{
		// Old code used to treat Bounciness as Friction as well.
		Friction = FMath::Clamp(1.f - Bounciness
			, 0.f
			, 1.f);

		// Old projectiles probably don't want to use this behavior by default.
		bInitialVelocityInLocalSpace = false;
	}
}

void UArcAsyncProjectileMovementComp::InitializeComponent()
{
	Super::InitializeComponent();
	AsyncPrimaryComponentTick.Target = this;
	SetupActorComponentTickFunction(&AsyncPrimaryComponentTick);

	if (Velocity.SizeSquared() > 0.f)
	{
		// InitialSpeed > 0 overrides initial velocity magnitude.
		if (InitialSpeed > 0.f)
		{
			Velocity = Velocity.GetSafeNormal() * InitialSpeed;
		}

		if (bInitialVelocityInLocalSpace)
		{
			SetVelocityInLocalSpace(Velocity);
		}

		if (bRotationFollowsVelocity)
		{
			if (UpdatedComponent)
			{
				FRotator DesiredRotation = Velocity.Rotation();
				if (bRotationRemainsVertical)
				{
					DesiredRotation.Pitch = 0.0f;
					DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
					DesiredRotation.Roll = 0.0f;
				}

				UpdatedComponent->SetWorldRotation(DesiredRotation);
			}
		}

		UpdateComponentVelocity();

		if (UpdatedPrimitive && UpdatedPrimitive->IsSimulatingPhysics())
		{
			UpdatedPrimitive->SetPhysicsLinearVelocity(Velocity);
		}
	}
}

void UArcAsyncProjectileMovementComp::UpdateTickRegistration()
{
	if (bAutoUpdateTickRegistration)
	{
		if (!bInterpolationComplete)
		{
			SetComponentTickEnabled(true);
		}
		else
		{
			Super::UpdateTickRegistration();
		}
	}
}

bool UArcAsyncProjectileMovementComp::HandleDeflection(FHitResult& Hit
													   , const FVector& OldVelocity
													   , const uint32 NumBounces
													   , float& SubTickTimeRemaining)
{
	const FVector Normal = ConstrainNormalToPlane(Hit.Normal);

	// Multiple hits within very short time period?
	const bool bMultiHit = (PreviousHitTime < 1.f && Hit.Time <= KINDA_SMALL_NUMBER);

	// if velocity still into wall (after HandleBlockingHit() had a chance to adjust),
	// slide along wall
	const float DotTolerance = 0.01f;
	bIsSliding = (bMultiHit && FVector::Coincident(PreviousHitNormal
					  , Normal)) || ((Velocity.GetSafeNormal() | Normal) <= DotTolerance);

	if (bIsSliding)
	{
		if (bMultiHit && (PreviousHitNormal | Normal) <= 0.f)
		{
			// 90 degree or less corner, so use cross product for direction
			FVector NewDir = (Normal ^ PreviousHitNormal);
			NewDir = NewDir.GetSafeNormal();
			Velocity = Velocity.ProjectOnToNormal(NewDir);
			if ((OldVelocity | Velocity) < 0.f)
			{
				Velocity *= -1.f;
			}
			Velocity = ConstrainDirectionToPlane(Velocity);
		}
		else
		{
			// adjust to move along new wall
			Velocity = ComputeSlideVector(Velocity
				, 1.f
				, Normal
				, Hit);
		}

		// Check min velocity.
		if (IsVelocityUnderSimulationThreshold())
		{
			StopSimulating(Hit);
			return false;
		}

		// Velocity is now parallel to the impact surface.
		if (SubTickTimeRemaining > KINDA_SMALL_NUMBER)
		{
			if (!HandleSliding(Hit
				, SubTickTimeRemaining))
			{
				return false;
			}
		}
	}

	return true;
}

bool UArcAsyncProjectileMovementComp::HandleSliding(FHitResult& Hit
													, float& SubTickTimeRemaining)
{
	FHitResult InitialHit(Hit);
	const FVector OldHitNormal = ConstrainDirectionToPlane(Hit.Normal);

	// Velocity is now parallel to the impact surface.
	// Perform the move now, before adding gravity/accel again, so we don't just keep
	// hitting the surface.
	SafeMoveUpdatedComponent(Velocity * SubTickTimeRemaining
		, UpdatedComponent->GetComponentQuat()
		, bSweepCollision
		, Hit);

	if (HasStoppedSimulation())
	{
		return false;
	}

	// A second hit can deflect the velocity (through the normal bounce code), for the
	// next iteration.
	if (Hit.bBlockingHit)
	{
		const float TimeTick = SubTickTimeRemaining;
		SubTickTimeRemaining = TimeTick * (1.f - Hit.Time);

		if (HandleBlockingHit(Hit
				, TimeTick
				, Velocity * TimeTick
				, SubTickTimeRemaining) == EHandleBlockingHitResult::Abort || HasStoppedSimulation())
		{
			return false;
		}
	}
	else
	{
		// Find velocity after elapsed time
		const FVector PostTickVelocity = ComputeVelocity(Velocity
			, SubTickTimeRemaining);

		// If pointing back into surface, apply friction and acceleration.
		const FVector Force = (PostTickVelocity - Velocity);
		const float ForceDotN = (Force | OldHitNormal);
		if (ForceDotN < 0.f)
		{
			const FVector ProjectedForce = FVector::VectorPlaneProject(Force
				, OldHitNormal);
			const FVector NewVelocity = Velocity + ProjectedForce;

			const FVector FrictionForce = -NewVelocity.GetSafeNormal() * FMath::Min(-ForceDotN * Friction
											  , NewVelocity.Size());
			Velocity = ConstrainDirectionToPlane(NewVelocity + FrictionForce);
		}
		else
		{
			Velocity = PostTickVelocity;
		}

		// Check min velocity
		if (IsVelocityUnderSimulationThreshold())
		{
			StopSimulating(InitialHit);
			return false;
		}

		SubTickTimeRemaining = 0.f;
	}

	return true;
}

void UArcAsyncProjectileMovementComp::SetVelocityInLocalSpace(FVector NewVelocity)
{
	if (UpdatedComponent)
	{
		RepLocalVelocity = Velocity;
		Velocity = UpdatedComponent->GetComponentToWorld().TransformVectorNoScale(NewVelocity);
	}
}

FVector UArcAsyncProjectileMovementComp::ComputeVelocity(FVector InitialVelocity
														 , float DeltaTime) const
{
	// v = v0 + a*t
	const FVector Acceleration = ComputeAcceleration(InitialVelocity
		, DeltaTime);
	FVector NewVelocity = InitialVelocity + (Acceleration * DeltaTime);

	return LimitVelocity(NewVelocity);
}

FVector UArcAsyncProjectileMovementComp::LimitVelocity(FVector NewVelocity) const
{
	const float CurrentMaxSpeed = GetMaxSpeed();
	if (CurrentMaxSpeed > 0.f)
	{
		NewVelocity = NewVelocity.GetClampedToMaxSize(CurrentMaxSpeed);
	}

	return ConstrainDirectionToPlane(NewVelocity);
}

FVector UArcAsyncProjectileMovementComp::ComputeMoveDelta(const FVector& InVelocity
														  , float DeltaTime) const
{
	// Velocity Verlet integration
	// (http://en.wikipedia.org/wiki/Verlet_integration#Velocity_Verlet) The addition of
	// p0 is done outside this method, we are just computing the delta. p = p0 + v0*t +
	// 1/2*a*t^2

	// We use ComputeVelocity() here to infer the acceleration, to make it easier to apply
	// custom velocities. p = p0 + v0*t + 1/2*((v1-v0)/t)*t^2 p = p0 + v0*t +
	// 1/2*((v1-v0))*t

	const FVector NewVelocity = ComputeVelocity(InVelocity
		, DeltaTime);
	const FVector Delta = (InVelocity * DeltaTime) + (NewVelocity - InVelocity) * (0.5f * DeltaTime);
	return Delta;
}

FVector UArcAsyncProjectileMovementComp::ComputeAcceleration(const FVector& InVelocity
															 , float DeltaTime) const
{
	FVector Acceleration(FVector::ZeroVector);

	Acceleration.Z += GetGravityZ();

	Acceleration += PendingForceThisUpdate;

	if (bIsHomingProjectile && HomingTargetComponent.IsValid())
	{
		Acceleration += ComputeHomingAcceleration(InVelocity
			, DeltaTime);
	}

	return Acceleration;
}

// Allow the projectile to track towards its homing target.
FVector UArcAsyncProjectileMovementComp::ComputeHomingAcceleration(const FVector& InVelocity
																   , float DeltaTime) const
{
	FVector HomingAcceleration = ((HomingTargetComponent->GetComponentLocation() - UpdatedComponent->
								   GetComponentLocation()).GetSafeNormal() * HomingAccelerationMagnitude);
	return HomingAcceleration;
}

void UArcAsyncProjectileMovementComp::AddForce(FVector Force)
{
	PendingForce += Force;
}

void UArcAsyncProjectileMovementComp::ClearPendingForce(bool bClearImmediateForce)
{
	PendingForce = FVector::ZeroVector;
	if (bClearImmediateForce)
	{
		PendingForceThisUpdate = FVector::ZeroVector;
	}
}

float UArcAsyncProjectileMovementComp::GetGravityZ() const
{
	// TODO: apply buoyancy if in water
	return ShouldApplyGravity() ? Super::GetGravityZ() * ProjectileGravityScale : 0.f;
}

void UArcAsyncProjectileMovementComp::StopSimulating(const FHitResult& HitResult)
{
	Velocity = FVector::ZeroVector;
	PendingForce = FVector::ZeroVector;
	PendingForceThisUpdate = FVector::ZeroVector;
	UpdateComponentVelocity();
	SetUpdatedComponent(NULL);
	OnProjectileStop.Broadcast(HitResult);
}

UArcAsyncProjectileMovementComp::EHandleBlockingHitResult UArcAsyncProjectileMovementComp::HandleBlockingHit(
	const FHitResult& Hit
	, float TimeTick
	, const FVector& MoveDelta
	, float& SubTickTimeRemaining)
{
	AActor* ActorOwner = UpdatedComponent ? UpdatedComponent->GetOwner() : NULL;
	if (!CheckStillInWorld() || !IsValid(ActorOwner))
	{
		return EHandleBlockingHitResult::Abort;
	}

	HandleImpact(Hit
		, TimeTick
		, MoveDelta);

	if (!IsValid(ActorOwner) || HasStoppedSimulation())
	{
		return EHandleBlockingHitResult::Abort;
	}

	if (Hit.bStartPenetrating)
	{
		// UE_LOG(LogProjectileMovement, Verbose, TEXT("Projectile %s is stuck inside
		// %s.%s with velocity %s!"), *GetNameSafe(ActorOwner),
		// *Hit.HitObjectHandle.GetName(), *GetNameSafe(Hit.GetComponent()),
		// *Velocity.ToString());
		return EHandleBlockingHitResult::Abort;
	}

	SubTickTimeRemaining = TimeTick * (1.f - Hit.Time);
	return EHandleBlockingHitResult::Deflect;
}

FVector UArcAsyncProjectileMovementComp::ComputeBounceResult(const FHitResult& Hit
															 , float TimeSlice
															 , const FVector& MoveDelta)
{
	FVector TempVelocity = Velocity;
	const FVector Normal = ConstrainNormalToPlane(Hit.Normal);
	const float VDotNormal = (TempVelocity | Normal);

	// Only if velocity is opposed by normal or parallel
	if (VDotNormal <= 0.f)
	{
		// Project velocity onto normal in reflected direction.
		const FVector ProjectedNormal = Normal * -VDotNormal;

		// Point velocity in direction parallel to surface
		TempVelocity += ProjectedNormal;

		// Only tangential velocity should be affected by friction.
		const float ScaledFriction = (bBounceAngleAffectsFriction || bIsSliding)
									 ? FMath::Clamp(-VDotNormal / TempVelocity.Size()
										   , MinFrictionFraction
										   , 1.f) * Friction
									 : Friction;
		TempVelocity *= FMath::Clamp(1.f - ScaledFriction
			, 0.f
			, 1.f);

		// Coefficient of restitution only applies perpendicular to impact.
		TempVelocity += (ProjectedNormal * FMath::Max(Bounciness
							 , 0.f));

		// Bounciness could cause us to exceed max speed.
		TempVelocity = LimitVelocity(TempVelocity);
	}

	return TempVelocity;
}

void UArcAsyncProjectileMovementComp::HandleImpact(const FHitResult& Hit
												   , float TimeSlice
												   , const FVector& MoveDelta)
{
	bool bStopSimulating = false;

	if (bShouldBounce)
	{
		const FVector OldVelocity = Velocity;
		Velocity = ComputeBounceResult(Hit
			, TimeSlice
			, MoveDelta);

		// Trigger bounce events
		OnProjectileBounce.Broadcast(Hit
			, OldVelocity);

		// Event may modify velocity or threshold, so check velocity threshold now.
		Velocity = LimitVelocity(Velocity);
		if (IsVelocityUnderSimulationThreshold())
		{
			bStopSimulating = true;
		}
	}
	else
	{
		bStopSimulating = true;
	}

	if (bStopSimulating)
	{
		StopSimulating(Hit);
	}
}

bool UArcAsyncProjectileMovementComp::CheckStillInWorld()
{
	if (!UpdatedComponent)
	{
		return false;
	}

	const UWorld* MyWorld = GetWorld();
	if (!MyWorld)
	{
		return false;
	}

	// check the variations of KillZ
	AWorldSettings* WorldSettings = MyWorld->GetWorldSettings(true);
	if (!WorldSettings->AreWorldBoundsChecksEnabled())
	{
		return true;
	}
	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (!IsValid(ActorOwner))
	{
		return false;
	}
	if (ActorOwner->GetActorLocation().Z < WorldSettings->KillZ)
	{
		const UDamageType* DmgType = WorldSettings->KillZDamageType
									 ? WorldSettings->KillZDamageType->GetDefaultObject<UDamageType>()
									 : GetDefault<UDamageType>();
		ActorOwner->FellOutOfWorld(*DmgType);
		return false;
	}
	// Check if box has poked outside the world
	else if (UpdatedComponent && UpdatedComponent->IsRegistered())
	{
		const FBox& Box = UpdatedComponent->Bounds.GetBox();
		if (Box.Min.X < -HALF_WORLD_MAX || Box.Max.X > HALF_WORLD_MAX || Box.Min.Y < -HALF_WORLD_MAX || Box.Max.Y >
			HALF_WORLD_MAX || Box.Min.Z < -HALF_WORLD_MAX || Box.Max.Z > HALF_WORLD_MAX)
		{
			// UE_LOG(LogProjectileMovement, Warning, TEXT("%s is outside the world
			// bounds!"), *ActorOwner->GetName());
			ActorOwner->OutsideWorldBounds();
			// not safe to use physics or collision at this point
			ActorOwner->SetActorEnableCollision(false);
			FHitResult Hit(1.f);
			StopSimulating(Hit);
			return false;
		}
	}
	return true;
}

bool UArcAsyncProjectileMovementComp::ShouldUseSubStepping() const
{
	return bForceSubStepping || (GetGravityZ() != 0.f) || (bIsHomingProjectile && HomingTargetComponent.IsValid());
}

float UArcAsyncProjectileMovementComp::GetSimulationTimeStep(float RemainingTime
															 , int32 Iterations) const
{
	if (RemainingTime > MaxSimulationTimeStep)
	{
		if (Iterations < MaxSimulationIterations)
		{
			// Subdivide moves to be no longer than MaxSimulationTimeStep seconds
			RemainingTime = FMath::Min(MaxSimulationTimeStep
				, RemainingTime * 0.5f);
		}
		else
		{
			// If this is the last iteration, just use all the remaining time. This is
			// better than cutting things short, as the simulation won't move far enough
			// otherwise. Print a throttled warning.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (const UWorld* const World = GetWorld())
			{
				// Don't report during long hitches, we're more concerned about normal
				// behavior just to make sure we have reasonable simulation settings.
				if (World->DeltaTimeSeconds < 0.20f)
				{
					static uint32 s_WarningCount = 0;
					if ((s_WarningCount++ < 100) || (GFrameCounter & 15) == 0)
					{
						// UE_LOG(LogProjectileMovement, Warning,
						// TEXT("GetSimulationTimeStep() - Max iterations %d hit while
						// remaining time %.6f > MaxSimulationTimeStep (%.3f) for '%s'"),
						// MaxSimulationIterations, RemainingTime, MaxSimulationTimeStep,
						// *GetPathNameSafe(UpdatedComponent));
					}
				}
			}
#endif
		}
	}

	// no less than MIN_TICK_TIME (to avoid potential divide-by-zero during simulation).
	return FMath::Max(MIN_TICK_TIME
		, RemainingTime);
}

void UArcAsyncProjectileMovementComp::SetInterpolatedComponent(USceneComponent* Component)
{
	if (Component == GetInterpolatedComponent())
	{
		return;
	}

	if (Component)
	{
		ResetInterpolation();
		InterpolatedComponentPtr = Component;
		InterpInitialLocationOffset = Component->GetRelativeLocation();
		InterpInitialRotationOffset = Component->GetRelativeRotation().Quaternion();
		bInterpolationComplete = false;
	}
	else
	{
		ResetInterpolation();
		InterpolatedComponentPtr = nullptr;
		InterpInitialLocationOffset = FVector::ZeroVector;
		InterpInitialRotationOffset = FQuat::Identity;
		bInterpolationComplete = true;
	}
}

USceneComponent* UArcAsyncProjectileMovementComp::GetInterpolatedComponent() const
{
	return InterpolatedComponentPtr.Get();
}

void UArcAsyncProjectileMovementComp::MoveInterpolationTarget(const FVector& NewLocation
															  , const FRotator& NewRotation)
{
	if (!UpdatedComponent)
	{
		return;
	}

	bool bHandledMovement = false;
	if (bInterpMovement)
	{
		if (USceneComponent* InterpComponent = GetInterpolatedComponent())
		{
			// Avoid moving the child, it will interpolate later
			const FRotator InterpRelativeRotation = InterpComponent->GetRelativeRotation();
			FScopedPreventAttachedComponentMove ScopedChildNoMove(InterpComponent);

			// Update interp offset
			const FVector OldLocation = UpdatedComponent->GetComponentLocation();
			const FVector NewToOldVector = (OldLocation - NewLocation);
			InterpLocationOffset += NewToOldVector;

			// Enforce distance limits
			if (NewToOldVector.SizeSquared() > FMath::Square(InterpLocationSnapToTargetDistance))
			{
				InterpLocationOffset = FVector::ZeroVector;
			}
			else if (InterpLocationOffset.SizeSquared() > FMath::Square(InterpLocationMaxLagDistance))
			{
				InterpLocationOffset = InterpLocationMaxLagDistance * InterpLocationOffset.GetSafeNormal();
			}

			// Handle rotation
			if (bInterpRotation)
			{
				const FQuat OldRotation = UpdatedComponent->GetComponentQuat();
				InterpRotationOffset = (NewRotation.Quaternion().Inverse() * OldRotation) * InterpRotationOffset;
			}
			else
			{
				// If not interpolating rotation, we should allow the component to rotate.
				// The absolute flag will get restored by the scoped move.
				InterpComponent->SetUsingAbsoluteRotation(false);
				InterpComponent->SetRelativeRotation_Direct(InterpRelativeRotation);
				InterpRotationOffset = FQuat::Identity;
			}

			// Move the root
			UpdatedComponent->SetRelativeLocationAndRotation(NewLocation
				, NewRotation);
			bHandledMovement = true;
			bInterpolationComplete = false;
		}
		else
		{
			ResetInterpolation();
			bInterpolationComplete = true;
		}
	}

	if (!bHandledMovement)
	{
		UpdatedComponent->SetRelativeLocationAndRotation(NewLocation
			, NewRotation);
	}
}

void UArcAsyncProjectileMovementComp::ResetInterpolation()
{
	if (USceneComponent* InterpComponent = GetInterpolatedComponent())
	{
		InterpComponent->SetRelativeLocationAndRotation(InterpInitialLocationOffset
			, InterpInitialRotationOffset);
	}

	InterpLocationOffset = FVector::ZeroVector;
	InterpRotationOffset = FQuat::Identity;
	bInterpolationComplete = true;
}

void UArcAsyncProjectileMovementComp::TickInterpolation(float DeltaTime)
{
	if (!bInterpolationComplete)
	{
		if (USceneComponent* InterpComponent = GetInterpolatedComponent())
		{
			// Smooth location. Interp faster when stopping.
			const float ActualInterpLocationTime = Velocity.IsZero() ? 0.5f * InterpLocationTime : InterpLocationTime;
			if (DeltaTime < ActualInterpLocationTime)
			{
				// Slowly decay translation offset (lagged exponential smoothing)
				InterpLocationOffset = (InterpLocationOffset * (1.f - DeltaTime / ActualInterpLocationTime));
			}
			else
			{
				InterpLocationOffset = FVector::ZeroVector;
			}

			// Smooth rotation
			if (DeltaTime < InterpRotationTime && bInterpRotation)
			{
				// Slowly decay rotation offset
				InterpRotationOffset = FQuat::FastLerp(InterpRotationOffset
					, FQuat::Identity
					, DeltaTime / InterpRotationTime).GetNormalized();
			}
			else
			{
				InterpRotationOffset = FQuat::Identity;
			}

			// Test for reaching the end
			if (InterpLocationOffset.IsNearlyZero(1e-2f) && InterpRotationOffset.Equals(FQuat::Identity
					, 1e-5f))
			{
				InterpLocationOffset = FVector::ZeroVector;
				InterpRotationOffset = FQuat::Identity;
				bInterpolationComplete = true;
			}

			// Apply result
			if (UpdatedComponent)
			{
				const FVector NewRelTranslation =
						UpdatedComponent->GetComponentToWorld().InverseTransformVectorNoScale(InterpLocationOffset) +
						InterpInitialLocationOffset;
				if (bInterpRotation)
				{
					const FQuat NewRelRotation = InterpRotationOffset * InterpInitialRotationOffset;
					InterpComponent->SetRelativeLocationAndRotation(NewRelTranslation
						, NewRelRotation);
				}
				else
				{
					InterpComponent->SetRelativeLocation(NewRelTranslation);
				}
			}
		}
		else
		{
			ResetInterpolation();
			bInterpolationComplete = true;
		}
	}

	// Might be done interpolating and want to disable tick
	if (bInterpolationComplete && bAutoUpdateTickRegistration && (UpdatedComponent == nullptr))
	{
		UpdateTickRegistration();
	}
}
