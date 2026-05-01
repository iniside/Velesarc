// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_SpawnMassProjectile.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcMass/Projectile/ArcProjectileFragments.h"
#include "MassActorSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassSpawnerSubsystem.h"
#include "Mass/EntityFragments.h"
#include "Engine/World.h"

EStateTreeRunStatus FArcAbilityTask_SpawnMassProjectile::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FArcAbilityTask_SpawnMassProjectileInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_SpawnMassProjectileInstanceData>(*this);

    if (!InstanceData.EntityConfig)
    {
        return EStateTreeRunStatus::Failed;
    }

    if (!InstanceData.OriginTargetingTag.IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    FMassEntityHandle Entity = AbilityContext.GetEntity();

    FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    if (!ActorFragment || !ActorFragment->IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    AActor* Actor = ActorFragment->GetMutable();
    UArcCoreAbilitySystemComponent* ASC = Actor ? Actor->FindComponentByClass<UArcCoreAbilitySystemComponent>() : nullptr;
    if (!ASC)
    {
        return EStateTreeRunStatus::Failed;
    }

    UWorld* World = Actor->GetWorld();
    if (!World)
    {
        return EStateTreeRunStatus::Failed;
    }

    UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
    if (!SpawnerSubsystem)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FMassEntityTemplate& EntityTemplate = InstanceData.EntityConfig->GetOrCreateEntityTemplate(*World);
    if (!EntityTemplate.IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    // Get spawn origin from global targeting
    bool bOriginSuccess = false;
    FHitResult OriginHit = UArcCoreAbilitySystemComponent::GetGlobalHitResult(ASC, InstanceData.OriginTargetingTag, bOriginSuccess);
    if (!bOriginSuccess)
    {
        return EStateTreeRunStatus::Failed;
    }

    // Get target location from global targeting (optional)
    TOptional<FVector> TargetLocation;
    if (InstanceData.TargetTargetingTag.IsValid())
    {
        bool bTargetSuccess = false;
        FVector TargetLoc = UArcCoreAbilitySystemComponent::GetGlobalHitLocation(ASC, InstanceData.TargetTargetingTag, bTargetSuccess);
        if (bTargetSuccess)
        {
            TargetLocation = TargetLoc;
        }
    }

    const FVector SpawnLocation = OriginHit.ImpactPoint;

    // Spawn entity
    TArray<FMassEntityHandle> SpawnedEntities;
    TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
        SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, SpawnedEntities);

    if (SpawnedEntities.IsEmpty())
    {
        return EStateTreeRunStatus::Failed;
    }

    FMassEntityHandle SpawnedEntity = SpawnedEntities[0];

    // Set spawn transform
    FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(SpawnedEntity);
    if (TransformFrag)
    {
        FTransform SpawnTransform = FTransform::Identity;
        SpawnTransform.SetLocation(SpawnLocation);
        TransformFrag->SetTransform(SpawnTransform);
    }

    // Set velocity direction and speed
    FArcProjectileFragment* ProjectileFrag = EntityManager.GetFragmentDataPtr<FArcProjectileFragment>(SpawnedEntity);
    if (ProjectileFrag)
    {
        FVector Direction = FVector::ZeroVector;
        if (TargetLocation.IsSet())
        {
            Direction = (TargetLocation.GetValue() - SpawnLocation).GetSafeNormal();
        }

        if (Direction.IsNearlyZero())
        {
            // No valid target -- use instigator forward direction
            Direction = Actor->GetActorForwardVector();
        }

        if (!Direction.IsNearlyZero())
        {
            ProjectileFrag->Velocity = Direction * InstanceData.InitialSpeed;

            if (TransformFrag)
            {
                TransformFrag->GetMutableTransform().SetRotation(Direction.ToOrientationQuat());
            }
        }
    }

    // Add instigator to collision ignore list
    if (InstanceData.bIgnoreInstigator)
    {
        FArcProjectileCollisionFilterFragment* FilterFrag = EntityManager.GetFragmentDataPtr<FArcProjectileCollisionFilterFragment>(SpawnedEntity);
        if (FilterFrag)
        {
            FilterFrag->IgnoredActors.Add(Actor);
        }
    }

    InstanceData.SpawnedEntity = SpawnedEntity;

    return EStateTreeRunStatus::Succeeded;
}
