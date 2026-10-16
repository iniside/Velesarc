// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_SpawnMassEntity.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "MassEntityConfigAsset.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityFragments.h"
#include "ArcMassAbilitiesLogs.h"

#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"

EStateTreeRunStatus FArcAbilityTask_SpawnMassEntity::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityTask_SpawnMassEntityInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_SpawnMassEntityInstanceData>(*this);

    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();
    UWorld* World = AbilityContext.GetMassContext().GetWorld();

    ARCMA_VLOG(World, Log,
        TEXT("[Ability] SpawnMassEntity enter entity=[%d:%d]"),
        Entity.Index, Entity.SerialNumber);

    if (!InstanceData.EntityConfig)
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Ability] SpawnMassEntity FAILED: no EntityConfig entity=[%d:%d]"),
            Entity.Index, Entity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    if (!World)
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Ability] SpawnMassEntity FAILED: no World entity=[%d:%d]"),
            Entity.Index, Entity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    UArcMassEntityConfigAsset* ArcCfg = Cast<UArcMassEntityConfigAsset>(InstanceData.EntityConfig);
    if (!ArcCfg)
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Ability] SpawnMassEntity FAILED: EntityConfig is not a UArcMassEntityConfigAsset (got %s) entity=[%d:%d]"),
            *GetNameSafe(InstanceData.EntityConfig),
            Entity.Index, Entity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    UArcMassSpawnerSubsystem* Spawner = World->GetSubsystem<UArcMassSpawnerSubsystem>();
    if (!Spawner)
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Ability] SpawnMassEntity FAILED: no UArcMassSpawnerSubsystem entity=[%d:%d]"),
            Entity.Index, Entity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

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

    const FTransform SpawnTransform(FQuat::Identity, SpawnLocation);
    const FMassEntityHandle SpawnedHandle = Spawner->SpawnEntity(ArcCfg, SpawnTransform);

    if (!SpawnedHandle.IsValid())
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Ability] SpawnMassEntity FAILED: spawner returned invalid handle config=%s entity=[%d:%d]"),
            *ArcCfg->GetName(),
            Entity.Index, Entity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    ARCMA_VLOG_LOCATION(World, SpawnLocation, 20.f, FColor::Green,
        TEXT("[Ability] SpawnMassEntity %s instigator=[%d:%d]"),
        *ArcCfg->GetName(),
        Entity.Index, Entity.SerialNumber);
    ARCMA_VLOG(World, Log,
        TEXT("[Ability] SpawnMassEntity: spawned new_entity=[%d:%d] instigator=[%d:%d] config=%s location=%s"),
        SpawnedHandle.Index, SpawnedHandle.SerialNumber,
        Entity.Index, Entity.SerialNumber,
        *ArcCfg->GetName(),
        *SpawnLocation.ToString());

    InstanceData.SpawnedEntity = SpawnedHandle;

    return EStateTreeRunStatus::Succeeded;
}
