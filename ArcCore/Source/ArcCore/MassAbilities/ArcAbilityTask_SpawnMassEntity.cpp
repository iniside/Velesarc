// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_SpawnMassEntity.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntityManager.h"
#include "Mass/EntityFragments.h"

EStateTreeRunStatus FArcAbilityTask_SpawnMassEntity::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityTask_SpawnMassEntityInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_SpawnMassEntityInstanceData>(*this);

    if (!InstanceData.EntityConfig)
    {
        return EStateTreeRunStatus::Failed;
    }

    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();

    UWorld* World = AbilityContext.GetMassContext().GetWorld();
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

    // Resolve spawn location based on SpawnOrigin
    FVector SpawnLocation = FVector::ZeroVector;
    switch (InstanceData.SpawnOrigin)
    {
    case EArcMassSpawnOrigin::HitPoint:
        SpawnLocation = InstanceData.TargetDataHandle.GetFirstImpactPoint();
        break;
    case EArcMassSpawnOrigin::Origin:
        SpawnLocation = InstanceData.TargetDataHandle.GetOrigin();
        break;
    case EArcMassSpawnOrigin::EntityLocation:
        {
            const FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
            if (TransformFragment)
            {
                SpawnLocation = TransformFragment->GetTransform().GetLocation();
            }
        }
        break;
    case EArcMassSpawnOrigin::Custom:
        SpawnLocation = InstanceData.CustomSpawnLocation;
        break;
    }

    TArray<FMassEntityHandle> SpawnedEntities;
    TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
        SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, SpawnedEntities);

    if (SpawnedEntities.Num() == 0)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FMassEntityHandle SpawnedHandle = SpawnedEntities[0];

    // Set transform on spawned entity
    FTransformFragment* SpawnedTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(SpawnedHandle);
    if (SpawnedTransform)
    {
        SpawnedTransform->GetMutableTransform().SetLocation(SpawnLocation);
    }

    InstanceData.SpawnedEntity = SpawnedHandle;

    // Release creation context to finalize spawning (runs observers and deferred commands)
    CreationContext.Reset();

    return EStateTreeRunStatus::Succeeded;
}
