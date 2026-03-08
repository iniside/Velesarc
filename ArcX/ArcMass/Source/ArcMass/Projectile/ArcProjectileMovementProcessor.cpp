// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcProjectileMovementProcessor.h"

#include "ArcProjectileFragments.h"
#include "ArcProjectileCollisionFilter.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassActorSubsystem.h"

// Chaos direct access
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/ChaosScene.h"
#include "SQAccelerator.h"
#include "CollisionQueryFilterCallbackCore.h"
#include "SceneQueryCommonParams.h"
#include "ChaosSQTypes.h"
#include "Chaos/Sphere.h"
#include "Chaos/ChaosUserEntity.h"
#include "Chaos/ParticleHandle.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/PrimitiveComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcProjectileMovementProcessor)

namespace
{
	/** Extract the owning actor and component from a Chaos geometry particle via user data. */
	void ResolveHitActorComponent(const Chaos::FGeometryParticle* Particle, AActor*& OutActor, UPrimitiveComponent*& OutComponent)
	{
		OutActor = nullptr;
		OutComponent = nullptr;

		if (!Particle)
		{
			return;
		}

		FBodyInstance* BodyInstance = nullptr;
		void* UserData = Particle->UserData();
		if (UserData)
		{
			BodyInstance = FChaosUserData::Get<FBodyInstance>(UserData);
			if (!BodyInstance)
			{
				FChaosUserEntityAppend* ChaosUserEntityAppend = FChaosUserData::Get<FChaosUserEntityAppend>(UserData);
				if (ChaosUserEntityAppend)
				{
					BodyInstance = FChaosUserData::Get<FBodyInstance>(ChaosUserEntityAppend->ChaosUserData);
				}
			}
		}

		if (BodyInstance)
		{
			OutComponent = BodyInstance->OwnerComponent.Get();
			if (OutComponent)
			{
				OutActor = OutComponent->GetOwner();
			}
		}
	}
}

UArcProjectileMovementProcessor::UArcProjectileMovementProcessor()
	: EntityQuery{*this}
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcProjectileMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcProjectileFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcProjectileCollisionFilterFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcProjectileHomingFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcProjectileConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcProjectileTag>(EMassFragmentPresence::All);
}

void UArcProjectileMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	if (DeltaTime <= 0.f)
	{
		return;
	}

	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	FPhysScene* PhysScene = World->GetPhysicsScene();
	if (!PhysScene)
	{
		return;
	}

	const auto* SpatialAcceleration = PhysScene->GetSpacialAcceleration();
	if (!SpatialAcceleration)
	{
		return;
	}

	FChaosSQAccelerator SQAccelerator(*SpatialAcceleration);

	const float WorldGravityZ = World->GetGravityZ(); // Negative value (e.g. -980)

	// Overlap filter: report all touches (used for sphere overlap).
	FOverlapAllCollisionQueryFilterCallback OverlapAllCallback;

	const FChaosQueryFlags QueryFlags(
		FChaosQueryFlag::eSTATIC | FChaosQueryFlag::eDYNAMIC | FChaosQueryFlag::ePREFILTER
	);
	const EHitFlags OutputFlags = EHitFlags::Position | EHitFlags::Normal | EHitFlags::Distance;

	// Custom filter callback — reused across entities, updated per-entity.
	FArcProjectileCollisionFilterCallback BlockFilterCallback;

	TArray<FMassEntityHandle> EntitiesToDestroy;

	EntityQuery.ForEachEntityChunk(Context,
		[&](FMassExecutionContext& Ctx)
	{
		TArrayView<FTransformFragment> Transforms = Ctx.GetMutableFragmentView<FTransformFragment>();
		TArrayView<FArcProjectileFragment> Projectiles = Ctx.GetMutableFragmentView<FArcProjectileFragment>();
		const TConstArrayView<FArcProjectileCollisionFilterFragment> CollisionFilters = Ctx.GetFragmentView<FArcProjectileCollisionFilterFragment>();
		const TConstArrayView<FArcProjectileHomingFragment> HomingFragments = Ctx.GetFragmentView<FArcProjectileHomingFragment>();
		TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const FArcProjectileConfigFragment& Config = Ctx.GetConstSharedFragment<FArcProjectileConfigFragment>();

		const bool bHasHoming = !HomingFragments.IsEmpty();
		const bool bHasActors = !ActorFragments.IsEmpty();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcProjectileFragment& Projectile = Projectiles[EntityIt];
			if (Projectile.bPendingDestroy)
			{
				continue;
			}

			FTransformFragment& Transform = Transforms[EntityIt];
			FVector CurrentPos = Transform.GetTransform().GetLocation();

			// -----------------------------------------------------------
			// 1. Velocity update
			// -----------------------------------------------------------

			// Gravity
			if (Config.GravityScale != 0.f)
			{
				Projectile.Velocity.Z += WorldGravityZ * Config.GravityScale * DeltaTime;
			}

			// Homing
			if (Config.bIsHoming && bHasHoming)
			{
				const FArcProjectileHomingFragment& Homing = HomingFragments[EntityIt];
				if (EntityManager.IsEntityValid(Homing.TargetEntity))
				{
					const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Homing.TargetEntity);
					if (TargetTransform)
					{
						const FVector ToTarget = (TargetTransform->GetTransform().GetLocation() - CurrentPos).GetSafeNormal();
						Projectile.Velocity += ToTarget * Config.HomingAcceleration * DeltaTime;
					}
				}
			}

			// Clamp to MaxSpeed
			if (Config.MaxSpeed > 0.f)
			{
				const float SpeedSq = Projectile.Velocity.SizeSquared();
				if (SpeedSq > FMath::Square(Config.MaxSpeed))
				{
					Projectile.Velocity = Projectile.Velocity.GetSafeNormal() * Config.MaxSpeed;
				}
			}

			const float Speed = Projectile.Velocity.Size();
			if (Speed < UE_KINDA_SMALL_NUMBER)
			{
				Projectile.bPendingDestroy = true;
				EntitiesToDestroy.Add(Ctx.GetEntity(EntityIt));
				continue;
			}

			const FVector MoveDir = Projectile.Velocity / Speed;
			const float MoveDist = Speed * DeltaTime;

			// Update rotation to follow velocity
			if (Config.bRotationFollowsVelocity)
			{
				Transform.GetMutableTransform().SetRotation(MoveDir.ToOrientationQuat());
			}

			// Sync actor transform to entity transform before collision (so actor follows entity)
			auto SyncActorTransform = [&]()
			{
				if (bHasActors)
				{
					if (AActor* Actor = ActorFragments[EntityIt].GetMutable())
					{
						Actor->SetActorTransform(Transform.GetTransform());
					}
				}
			};

			// -----------------------------------------------------------
			// 2. Collision detection via Chaos direct queries
			// -----------------------------------------------------------

			// Set per-entity ignore list on filter callback
			BlockFilterCallback.SetIgnoredActors(&CollisionFilters[EntityIt].IgnoredActors);

			ChaosInterface::FSQHitBuffer<ChaosInterface::FRaycastHit> HitBuffer(/*bSingle=*/ true);
			const Chaos::Filter::FQueryFilterData FilterData;
			const ChaosInterface::FSceneQueryCommonParams CommonParams(BlockFilterCallback, FilterData, QueryFlags);

			SQAccelerator.Raycast(
				CurrentPos,
				MoveDir,
				MoveDist,
				HitBuffer,
				OutputFlags,
				CommonParams
			);

			if (HitBuffer.HasBlockingHit())
			{
				const ChaosInterface::FRaycastHit* BlockHit = HitBuffer.GetBlock();
				if (BlockHit)
				{
					const FVector HitPos = BlockHit->WorldPosition;
					const FVector HitNormal = BlockHit->WorldNormal;
					Transform.GetMutableTransform().SetLocation(HitPos);
					Projectile.DistanceTraveled += static_cast<float>(BlockHit->Distance);

					if (Config.bShouldBounce)
					{
						// Reflect velocity
						FVector ReflectedVelocity = FMath::GetReflectionVector(Projectile.Velocity, HitNormal);

						// Apply bounciness (normal component loss)
						const float NormalComponent = FVector::DotProduct(ReflectedVelocity, HitNormal);
						const FVector NormalVelocity = HitNormal * NormalComponent;
						const FVector TangentVelocity = ReflectedVelocity - NormalVelocity;

						ReflectedVelocity = TangentVelocity * (1.f - Config.Friction) + NormalVelocity * Config.Bounciness;
						Projectile.Velocity = ReflectedVelocity;

						// Check stop threshold
						if (ReflectedVelocity.SizeSquared() < FMath::Square(Config.BounceVelocityStopThreshold))
						{
							Projectile.bPendingDestroy = true;
							EntitiesToDestroy.Add(Ctx.GetEntity(EntityIt));
						}
					}
					else
					{
						// Notify actor before destruction
						if (bHasActors)
						{
							if (AActor* Actor = ActorFragments[EntityIt].GetMutable())
							{
								if (Actor->Implements<UArcProjectileActorInterface>())
								{
									FHitResult HitResult;
									HitResult.ImpactPoint = HitPos;
									HitResult.ImpactNormal = HitNormal;
									HitResult.Location = HitPos;
									HitResult.Normal = HitNormal;
									HitResult.Distance = static_cast<float>(BlockHit->Distance);
									HitResult.TraceStart = CurrentPos;
									HitResult.TraceEnd = CurrentPos + MoveDir * MoveDist;
									HitResult.FaceIndex = BlockHit->FaceIndex;

									AActor* HitActor = nullptr;
									UPrimitiveComponent* HitComponent = nullptr;
									ResolveHitActorComponent(BlockHit->Actor, HitActor, HitComponent);
									if (HitActor)
									{
										HitResult.HitObjectHandle = FActorInstanceHandle(HitActor);
									}
									if (HitComponent)
									{
										HitResult.Component = HitComponent;
									}

									IArcProjectileActorInterface::Execute_OnProjectileCollision(Actor, HitResult);
								}
							}
						}
						Projectile.bPendingDestroy = true;
						EntitiesToDestroy.Add(Ctx.GetEntity(EntityIt));
					}
				}
			}
			else
			{
				// No hit — advance position
				const FVector NewPos = CurrentPos + MoveDir * MoveDist;
				Transform.GetMutableTransform().SetLocation(NewPos);
				Projectile.DistanceTraveled += MoveDist;

				// Sphere overlap at new position
				if (Config.CollisionRadius > 0.f)
				{
					Chaos::FImplicitSphere3 OverlapSphere(Chaos::FVec3(0), Config.CollisionRadius);
					const FTransform OverlapPose(FQuat::Identity, NewPos);

					ChaosInterface::FSQHitBuffer<ChaosInterface::FOverlapHit> OverlapBuffer;
					const Chaos::Filter::FQueryFilterData OverlapFilterData;
					const ChaosInterface::FSceneQueryCommonParams OverlapParams(
						BlockFilterCallback, OverlapFilterData, QueryFlags
					);

					SQAccelerator.Overlap(
						OverlapSphere,
						OverlapPose,
						OverlapBuffer,
						OverlapParams
					);

					if (OverlapBuffer.GetNumHits() > 0)
					{
						// Notify actor before destruction
						if (bHasActors)
						{
							if (AActor* Actor = ActorFragments[EntityIt].GetMutable())
							{
								if (Actor->Implements<UArcProjectileActorInterface>())
								{
									FHitResult HitResult;
									HitResult.ImpactPoint = NewPos;
									HitResult.Location = NewPos;
									HitResult.TraceStart = CurrentPos;
									HitResult.TraceEnd = NewPos;

									const ChaosInterface::FOverlapHit* OverlapHits = OverlapBuffer.GetHits();
									if (OverlapHits)
									{
										AActor* HitActor = nullptr;
										UPrimitiveComponent* HitComponent = nullptr;
										ResolveHitActorComponent(OverlapHits[0].Actor, HitActor, HitComponent);
										if (HitActor)
										{
											HitResult.HitObjectHandle = FActorInstanceHandle(HitActor);
										}
										if (HitComponent)
										{
											HitResult.Component = HitComponent;
										}
									}

									IArcProjectileActorInterface::Execute_OnProjectileCollision(Actor, HitResult);
								}
							}
						}
						Projectile.bPendingDestroy = true;
						EntitiesToDestroy.Add(Ctx.GetEntity(EntityIt));
					}
				}

				// Check max distance
				if (!Projectile.bPendingDestroy && Projectile.DistanceTraveled >= Config.MaxDistance)
				{
					if (bHasActors)
					{
						if (AActor* Actor = ActorFragments[EntityIt].GetMutable())
						{
							if (Actor->Implements<UArcProjectileActorInterface>())
							{
								IArcProjectileActorInterface::Execute_OnProjectileExpired(Actor);
							}
						}
					}
					Projectile.bPendingDestroy = true;
					EntitiesToDestroy.Add(Ctx.GetEntity(EntityIt));
				}
			}

			// Sync actor position to entity transform
			SyncActorTransform();
		}
	});

	// Batch-destroy
	if (EntitiesToDestroy.Num() > 0)
	{
		EntityManager.Defer().DestroyEntities(EntitiesToDestroy);
	}
}
