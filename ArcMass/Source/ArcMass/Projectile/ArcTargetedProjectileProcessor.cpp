// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTargetedProjectileProcessor.h"

#include "ArcMass/Actions/ArcMassAction.h"
#include "ArcMass/Actions/ArcMassActionFragment.h"
#include "ArcProjectileFragments.h"
#include "ArcProjectileUtils.h"

#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "Mass/EntityManager.h"
#include "Mass/ExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTargetedProjectileProcessor)

UArcTargetedProjectileProcessor::UArcTargetedProjectileProcessor()
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcTargetedProjectileProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcProjectileFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcTargetedProjectileFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcProjectileConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcTargetedProjectileConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcProjectileTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcTargetedProjectileTag>(EMassFragmentPresence::All);
}

void UArcTargetedProjectileProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	using namespace UE::ArcMass::Projectile;

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	if (DeltaTime <= 0.f)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcTargetedProjectile);

	TArray<FMassEntityHandle> EntitiesToDestroy;
	TArray<FPendingHitNotification> PendingHits;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, DeltaTime, &EntitiesToDestroy, &PendingHits](FMassExecutionContext& Ctx)
		{
			TArrayView<FTransformFragment> Transforms = Ctx.GetMutableFragmentView<FTransformFragment>();
			TArrayView<FArcProjectileFragment> Projectiles = Ctx.GetMutableFragmentView<FArcProjectileFragment>();
			TArrayView<FArcTargetedProjectileFragment> TargetedProjectiles = Ctx.GetMutableFragmentView<FArcTargetedProjectileFragment>();
			TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const FArcProjectileConfigFragment& ProjectileConfig = Ctx.GetConstSharedFragment<FArcProjectileConfigFragment>();
			const FArcTargetedProjectileConfigFragment& TargetedConfig = Ctx.GetConstSharedFragment<FArcTargetedProjectileConfigFragment>();
			const bool bHasActors = !ActorFragments.IsEmpty();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcProjectileFragment& Projectile = Projectiles[EntityIt];
				if (Projectile.bPendingDestroy)
				{
					continue;
				}

				FArcTargetedProjectileFragment& TargetedProjectile = TargetedProjectiles[EntityIt];
				FTransformFragment& Transform = Transforms[EntityIt];
				const FVector CurrentLocation = Transform.GetTransform().GetLocation();
				const FMassEntityHandle ProjectileEntity = Ctx.GetEntity(EntityIt);

				FPendingHitNotification* PendingHit = nullptr;
				const bool bTargetValid = EntityManager.IsEntityValid(TargetedProjectile.TargetEntity);
				const FTransformFragment* TargetTransform = bTargetValid
					? EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetedProjectile.TargetEntity)
					: nullptr;
				bool bSettingValid = false;
				switch (TargetedConfig.MovementMode)
				{
				case EArcTargetedProjectileMovementMode::FixedDuration:
					bSettingValid = FMath::IsFinite(TargetedConfig.FixedDuration) && TargetedConfig.FixedDuration > 0.f
						&& FMath::IsFinite(TargetedProjectile.RemainingDuration) && TargetedProjectile.RemainingDuration > 0.f;
					break;
				case EArcTargetedProjectileMovementMode::ConstantSpeed:
					bSettingValid = FMath::IsFinite(TargetedConfig.ConstantSpeed) && TargetedConfig.ConstantSpeed > 0.f;
					break;
				}

				if (!TargetTransform || !bSettingValid)
				{
					PendingHit = &PendingHits.AddDefaulted_GetRef();
					PendingHit->HitResult.TraceStart = CurrentLocation;
					PendingHit->HitResult.TraceEnd = CurrentLocation;
					PendingHit->HitResult.Location = CurrentLocation;
					PendingHit->bExpired = true;
				}
				else
				{
					const FVector TargetLocation = TargetTransform->GetTransform().GetLocation();
					const FVector ToTarget = TargetLocation - CurrentLocation;
					const float DistanceToTarget = ToTarget.Size();
					FVector NewLocation = CurrentLocation;
					bool bArrived = TargetedConfig.MovementMode == EArcTargetedProjectileMovementMode::ConstantSpeed
						&& DistanceToTarget <= UE_KINDA_SMALL_NUMBER;

					if (!bArrived && TargetedConfig.MovementMode == EArcTargetedProjectileMovementMode::ConstantSpeed)
					{
						const FVector MoveDirection = ToTarget / DistanceToTarget;
						const float MoveDistance = FMath::Min(TargetedConfig.ConstantSpeed * DeltaTime, DistanceToTarget);
						NewLocation += MoveDirection * MoveDistance;
						Projectile.Velocity = MoveDirection * (MoveDistance / DeltaTime);
						bArrived = MoveDistance >= DistanceToTarget;
					}
					else if (!bArrived)
					{
						const float StepDuration = FMath::Min(DeltaTime, TargetedProjectile.RemainingDuration);
						const float StepAlpha = StepDuration / TargetedProjectile.RemainingDuration;
						NewLocation = FMath::Lerp(CurrentLocation, TargetLocation, StepAlpha);
						Projectile.Velocity = (NewLocation - CurrentLocation) / DeltaTime;
						TargetedProjectile.RemainingDuration -= StepDuration;
						bArrived = TargetedProjectile.RemainingDuration <= 0.f;
					}
					else
					{
						Projectile.Velocity = FVector::ZeroVector;
					}

					const FVector MoveDelta = NewLocation - CurrentLocation;
					Transform.GetMutableTransform().SetLocation(NewLocation);
					Projectile.DistanceTraveled += MoveDelta.Size();
					if (ProjectileConfig.bRotationFollowsVelocity && !MoveDelta.IsNearlyZero())
					{
						Transform.GetMutableTransform().SetRotation(MoveDelta.ToOrientationQuat());
					}

					if (bArrived)
					{
						AActor* TargetActor = nullptr;
						FMassActorFragment* TargetActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(TargetedProjectile.TargetEntity);
						if (TargetActorFragment)
						{
							TargetActor = TargetActorFragment->GetMutable();
						}

						PendingHit = &PendingHits.AddDefaulted_GetRef();
						PendingHit->HitResult = BuildOverlapHitResult(TargetLocation, TargetActor, nullptr, CurrentLocation);
					}
				}

				if (bHasActors)
				{
					if (AActor* VisualizationActor = ActorFragments[EntityIt].GetMutable())
					{
						VisualizationActor->SetActorTransform(Transform.GetTransform());
					}
				}

				if (PendingHit)
				{
					PendingHit->ProjectileEntity = ProjectileEntity;
					PendingHit->Instigator = Projectile.Instigator;
					PendingHit->Velocity = Projectile.Velocity;
					PendingHit->TargetEntity = TargetedProjectile.TargetEntity;
					if (bHasActors)
					{
						PendingHit->VisualizationActor = ActorFragments[EntityIt].GetMutable();
					}
					Projectile.bPendingDestroy = true;
					EntitiesToDestroy.Add(ProjectileEntity);
				}
			}
		});

	for (FPendingHitNotification& PendingHit : PendingHits)
	{
		if (!PendingHit.bExpired)
		{
			const bool bTargetValid = EntityManager.IsEntityValid(PendingHit.TargetEntity);
			const FTransformFragment* TargetTransform = bTargetValid
				? EntityManager.GetFragmentDataPtr<FTransformFragment>(PendingHit.TargetEntity)
				: nullptr;
			if (!TargetTransform)
			{
				PendingHit.HitResult = FHitResult();
				PendingHit.bExpired = true;
			}
		}

		if (!PendingHit.bExpired)
		{
			FStructView ActionView = EntityManager.GetMutableSparseElementDataForEntity(
				FArcMassActionFragment::StaticStruct(),
				PendingHit.ProjectileEntity);
			UWorld* World = EntityManager.GetWorld();
			if (ActionView.IsValid() && World)
			{
				FArcCollisionHitContext HitContext;
				HitContext.HitResult = PendingHit.HitResult;
				HitContext.Instigator = PendingHit.Instigator;
				HitContext.Velocity = PendingHit.Velocity;
				HitContext.TargetEntity = PendingHit.TargetEntity;
				FArcMassActionFragment& ActionFragment = ActionView.Get<FArcMassActionFragment>();
				FArcMassActionContext ActionContext{
					EntityManager,
					Context,
					*World,
					PendingHit.ProjectileEntity,
					FConstStructView::Make(HitContext)};
				ActionFragment.Execute(Events::ProjectileHit, ActionContext);
				ActionFragment.PostExecute(Events::ProjectileHit, ActionContext);
			}
		}

		TArray<FPendingHitNotification> SingleHit;
		SingleHit.Add(MoveTemp(PendingHit));
		DispatchHitNotifications(SingleHit);
	}

	if (!EntitiesToDestroy.IsEmpty())
	{
		Context.Defer().DestroyEntities(EntitiesToDestroy);
	}
}
