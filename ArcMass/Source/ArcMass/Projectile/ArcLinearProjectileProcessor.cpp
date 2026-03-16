// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcLinearProjectileProcessor.h"

#include "ArcProjectileFragments.h"
#include "ArcProjectileCollisionFilter.h"
#include "ArcProjectileUtils.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassActorSubsystem.h"

#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/ChaosScene.h"
#include "SQAccelerator.h"
#include "CollisionQueryFilterCallbackCore.h"
#include "SceneQueryCommonParams.h"
#include "ChaosSQTypes.h"
#include "Chaos/Sphere.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcLinearProjectileProcessor)

UArcLinearProjectileProcessor::UArcLinearProjectileProcessor()
	: EntityQuery{*this}
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcLinearProjectileProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcProjectileFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcProjectileCollisionFilterFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcProjectileConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcProjectileTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcLinearProjectileTag>(EMassFragmentPresence::All);
}

void UArcLinearProjectileProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	using namespace UE::ArcMass::Projectile;

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

	const float WorldGravityZ = World->GetGravityZ();

	const FChaosQueryFlags QueryFlags(
		FChaosQueryFlag::eSTATIC | FChaosQueryFlag::eDYNAMIC | FChaosQueryFlag::ePREFILTER
	);
	const EHitFlags OutputFlags = EHitFlags::Position | EHitFlags::Normal | EHitFlags::Distance;

	FArcProjectileCollisionFilterCallback BlockFilterCallback;

	TArray<FMassEntityHandle> EntitiesToDestroy;
	TArray<FPendingHitNotification> PendingHits;

	EntityQuery.ForEachEntityChunk(Context,
		[&](FMassExecutionContext& Ctx)
	{
		TArrayView<FTransformFragment> Transforms = Ctx.GetMutableFragmentView<FTransformFragment>();
		TArrayView<FArcProjectileFragment> Projectiles = Ctx.GetMutableFragmentView<FArcProjectileFragment>();
		const TConstArrayView<FArcProjectileCollisionFilterFragment> CollisionFilters = Ctx.GetFragmentView<FArcProjectileCollisionFilterFragment>();
		TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
		const FArcProjectileConfigFragment& Config = Ctx.GetConstSharedFragment<FArcProjectileConfigFragment>();

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

			// Gravity
			Projectile.Velocity.Z += WorldGravityZ * Config.GravityScale * DeltaTime;

			// MaxSpeed clamp
			const float SpeedSq = Projectile.Velocity.SizeSquared();
			if (SpeedSq > FMath::Square(Config.MaxSpeed))
			{
				Projectile.Velocity = Projectile.Velocity.GetSafeNormal() * Config.MaxSpeed;
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

			if (Config.bRotationFollowsVelocity)
			{
				Transform.GetMutableTransform().SetRotation(MoveDir.ToOrientationQuat());
			}

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

			auto CollectHitNotification = [&](FHitResult&& InHitResult, bool bInExpired)
			{
				FPendingHitNotification& N = PendingHits.AddDefaulted_GetRef();
				N.HitResult = MoveTemp(InHitResult);
				N.Instigator = Projectile.Instigator;
				N.Velocity = Projectile.Velocity;
				N.bExpired = bInExpired;
				if (bHasActors)
				{
					N.VisualizationActor = ActorFragments[EntityIt].GetMutable();
				}
			};

			// Collision detection
			BlockFilterCallback.SetIgnoredActors(&CollisionFilters[EntityIt].IgnoredActors);

			ChaosInterface::FSQHitBuffer<ChaosInterface::FRaycastHit> HitBuffer(/*bSingle=*/ true);
			const Chaos::Filter::FQueryFilterData FilterData;
			const ChaosInterface::FSceneQueryCommonParams CommonParams(BlockFilterCallback, FilterData, QueryFlags);

			SQAccelerator.Raycast(CurrentPos, MoveDir, MoveDist, HitBuffer, OutputFlags, CommonParams);

			if (HitBuffer.HasBlockingHit())
			{
				const ChaosInterface::FRaycastHit* BlockHit = HitBuffer.GetBlock();
				if (BlockHit)
				{
					const FVector HitPos = BlockHit->WorldPosition;
					const FVector HitNormal = BlockHit->WorldNormal;
					Transform.GetMutableTransform().SetLocation(HitPos);
					Projectile.DistanceTraveled += static_cast<float>(BlockHit->Distance);

					AActor* HitActor = nullptr;
					UPrimitiveComponent* HitComponent = nullptr;
					ResolveHitActorComponent(BlockHit->Actor, HitActor, HitComponent);

					FHitResult HR = BuildRaycastHitResult(
						HitPos, HitNormal,
						static_cast<float>(BlockHit->Distance), BlockHit->FaceIndex,
						HitActor, HitComponent,
						CurrentPos, CurrentPos + MoveDir * MoveDist);

					CollectHitNotification(MoveTemp(HR), false);
					Projectile.bPendingDestroy = true;
					EntitiesToDestroy.Add(Ctx.GetEntity(EntityIt));
				}
			}
			else
			{
				const FVector NewPos = CurrentPos + MoveDir * MoveDist;
				Transform.GetMutableTransform().SetLocation(NewPos);
				Projectile.DistanceTraveled += MoveDist;

				// Sphere overlap
				if (Config.CollisionRadius > 0.f)
				{
					Chaos::FImplicitSphere3 OverlapSphere(Chaos::FVec3(0), Config.CollisionRadius);
					const FTransform OverlapPose(FQuat::Identity, NewPos);

					ChaosInterface::FSQHitBuffer<ChaosInterface::FOverlapHit> OverlapBuffer;
					const Chaos::Filter::FQueryFilterData OverlapFilterData;
					const ChaosInterface::FSceneQueryCommonParams OverlapParams(BlockFilterCallback, OverlapFilterData, QueryFlags);

					SQAccelerator.Overlap(OverlapSphere, OverlapPose, OverlapBuffer, OverlapParams);

					if (OverlapBuffer.GetNumHits() > 0)
					{
						AActor* HitActor = nullptr;
						UPrimitiveComponent* HitComponent = nullptr;
						const ChaosInterface::FOverlapHit* OverlapHits = OverlapBuffer.GetHits();
						if (OverlapHits)
						{
							ResolveHitActorComponent(OverlapHits[0].Actor, HitActor, HitComponent);
						}

						FHitResult HR = BuildOverlapHitResult(NewPos, HitActor, HitComponent, CurrentPos);
						CollectHitNotification(MoveTemp(HR), false);
						Projectile.bPendingDestroy = true;
						EntitiesToDestroy.Add(Ctx.GetEntity(EntityIt));
					}
				}

				// Max distance
				if (!Projectile.bPendingDestroy && Projectile.DistanceTraveled >= Config.MaxDistance)
				{
					FHitResult HR;
					HR.TraceStart = CurrentPos;
					HR.TraceEnd = NewPos;
					HR.Location = NewPos;
					CollectHitNotification(MoveTemp(HR), true);
					Projectile.bPendingDestroy = true;
					EntitiesToDestroy.Add(Ctx.GetEntity(EntityIt));
				}
			}

			SyncActorTransform();
		}
	});

	DispatchHitNotifications(PendingHits);

	if (EntitiesToDestroy.Num() > 0)
	{
		EntityManager.Defer().DestroyEntities(EntitiesToDestroy);
	}
}
