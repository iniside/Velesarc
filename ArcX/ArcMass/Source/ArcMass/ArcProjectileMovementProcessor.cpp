// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcProjectileMovementProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"

// Chaos direct access
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/ChaosScene.h"
#include "SQAccelerator.h"
#include "CollisionQueryFilterCallbackCore.h"
#include "SceneQueryCommonParams.h"
#include "ChaosSQTypes.h"
#include "Chaos/Sphere.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcProjectileMovementProcessor)

// ---------------------------------------------------------------------------
// UArcProjectileMovementProcessor
// ---------------------------------------------------------------------------

UArcProjectileMovementProcessor::UArcProjectileMovementProcessor()
	: EntityQuery{*this}
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	// NOTE: Game-thread required because FChaosSQAccelerator queries go through
	// a filter callback that is not thread-safe by default. A future optimization
	// could cache the spatial acceleration pointer and use per-thread callbacks.
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcProjectileMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcProjectileFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FArcProjectileTag>(EMassFragmentPresence::All);
}

void UArcProjectileMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	if (DeltaTime <= 0.f)
	{
		return;
	}

	// -----------------------------------------------------------------------
	// Acquire the Chaos spatial acceleration structure for direct raycasting.
	// We go through FPhysScene (== FPhysScene_Chaos) → FChaosScene base →
	// GetSpacialAcceleration() to get the broadphase structure, then wrap it
	// in FChaosSQAccelerator which handles the visitor setup internally.
	// -----------------------------------------------------------------------

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

	// Raycast filter: block everything (first hit wins).
	FBlockAllCollisionQueryFilterCallback BlockAllCallback;

	// Overlap filter: report all touches.
	FOverlapAllCollisionQueryFilterCallback OverlapAllCallback;

	// Query flags: traverse both static and dynamic geometry, run the pre-filter.
	const FChaosQueryFlags QueryFlags(
		FChaosQueryFlag::eSTATIC | FChaosQueryFlag::eDYNAMIC | FChaosQueryFlag::ePREFILTER
	);

	// We want hit position, normal, and distance back from the narrowphase.
	const EHitFlags OutputFlags = EHitFlags::Position | EHitFlags::Normal | EHitFlags::Distance;

	// Entities whose projectile reached max distance or hit something.
	TArray<FMassEntityHandle> EntitiesToDestroy;

	EntityQuery.ForEachEntityChunk(Context,
		[&](FMassExecutionContext& Ctx)
	{
		TArrayView<FTransformFragment> Transforms = Ctx.GetMutableFragmentView<FTransformFragment>();
		TArrayView<FArcProjectileFragment> Projectiles = Ctx.GetMutableFragmentView<FArcProjectileFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcProjectileFragment& Projectile = Projectiles[EntityIt];

			if (Projectile.bPendingDestroy)
			{
				continue;
			}

			FTransformFragment& Transform = Transforms[EntityIt];
			const FVector CurrentPos = Transform.GetTransform().GetLocation();
			const FVector MoveDir = Projectile.Direction.GetSafeNormal();
			const float MoveDist = Projectile.Speed * DeltaTime;

			// ---------------------------------------------------------------
			// Raycast along the movement vector using Chaos directly.
			// ---------------------------------------------------------------

			ChaosInterface::FSQHitBuffer<ChaosInterface::FRaycastHit> HitBuffer(/*bSingle=*/ true);

			const Chaos::Filter::FQueryFilterData FilterData;
			const ChaosInterface::FSceneQueryCommonParams CommonParams(BlockAllCallback, FilterData, QueryFlags);

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
				// Move to the hit location.
				const ChaosInterface::FRaycastHit* BlockHit = HitBuffer.GetBlock();
				if (BlockHit)
				{
					const FVector HitPos = BlockHit->WorldPosition;
					Transform.GetMutableTransform().SetLocation(HitPos);
					Projectile.DistanceTraveled += static_cast<float>(BlockHit->Distance);
				}

				Projectile.bPendingDestroy = true;
				EntitiesToDestroy.Add(Ctx.GetEntity(EntityIt));
			}
			else
			{
				// No hit — advance position.
				const FVector NewPos = CurrentPos + MoveDir * MoveDist;
				Transform.GetMutableTransform().SetLocation(NewPos);
				Projectile.DistanceTraveled += MoveDist;

				// -----------------------------------------------------------
				// Sphere overlap at the new position using Chaos directly.
				// This demonstrates the Overlap API on FChaosSQAccelerator.
				// Unlike UE's component overlap events (which are also just
				// scene queries internally — no persistent broadphase pairs),
				// we go straight to the spatial acceleration structure.
				//
				// Chaos::TSphere<FReal,3> is FImplicitSphere3, which derives
				// from FImplicitObject — the query geometry type that
				// FChaosSQAccelerator::Overlap expects.
				// -----------------------------------------------------------

				if (Projectile.CollisionRadius > 0.f)
				{
					Chaos::FImplicitSphere3 OverlapSphere(Chaos::FVec3(0), Projectile.CollisionRadius);
					const FTransform OverlapPose(FQuat::Identity, NewPos);

					ChaosInterface::FSQHitBuffer<ChaosInterface::FOverlapHit> OverlapBuffer;

					const Chaos::Filter::FQueryFilterData OverlapFilterData;
					const ChaosInterface::FSceneQueryCommonParams OverlapParams(
						OverlapAllCallback, OverlapFilterData, QueryFlags
					);

					SQAccelerator.Overlap(
						OverlapSphere,
						OverlapPose,
						OverlapBuffer,
						OverlapParams
					);

					if (OverlapBuffer.GetNumHits() > 0)
					{
						// Something overlapped the projectile sphere.
						// FOverlapHit contains Actor (FGeometryParticle*) and Shape
						// (FPerShapeData*) — enough to identify what was hit.
						Projectile.bPendingDestroy = true;
						EntitiesToDestroy.Add(Ctx.GetEntity(EntityIt));
					}
				}

				// Check max distance.
				if (!Projectile.bPendingDestroy && Projectile.DistanceTraveled >= Projectile.MaxDistance)
				{
					Projectile.bPendingDestroy = true;
					EntitiesToDestroy.Add(Ctx.GetEntity(EntityIt));
				}
			}
		}
	});

	// Batch-destroy entities that are done.
	if (EntitiesToDestroy.Num() > 0)
	{
		EntityManager.Defer().DestroyEntities(EntitiesToDestroy);
	}
}
