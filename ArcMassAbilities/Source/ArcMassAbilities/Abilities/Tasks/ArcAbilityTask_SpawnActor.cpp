// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_SpawnActor.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityFragments.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_SpawnActor::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityTask_SpawnActorInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_SpawnActorInstanceData>(*this);

    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    const FMassEntityHandle Entity = AbilityContext.GetEntity();
    UWorld* World = AbilityContext.GetMassContext().GetWorld();

    ARCMA_VLOG(World, Log,
        TEXT("[Ability] SpawnActor enter entity=[%d:%d]"),
        Entity.Index, Entity.SerialNumber);

    if (!InstanceData.ActorClass)
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Ability] SpawnActor FAILED: no ActorClass entity=[%d:%d]"),
            Entity.Index, Entity.SerialNumber);
        return EStateTreeRunStatus::Failed;
    }

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();

    if (!World)
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Ability] SpawnActor FAILED: no World entity=[%d:%d]"),
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

    ARCMA_VLOG_LOCATION(World, SpawnLocation, 25.f, FColor::Cyan,
        TEXT("[Ability] SpawnActor %s entity=[%d:%d]"),
        *InstanceData.ActorClass->GetName(),
        Entity.Index, Entity.SerialNumber);
    ARCMA_VLOG(World, Log,
        TEXT("[Ability] SpawnActor: origin=%d resolved location=%s entity=[%d:%d] class=%s"),
        static_cast<int32>(InstanceData.SpawnOrigin),
        *SpawnLocation.ToString(),
        Entity.Index, Entity.SerialNumber,
        *InstanceData.ActorClass->GetName());

    const FRotator SpawnRotation = FRotator::ZeroRotator;

    AActor* NewActor = World->SpawnActor<AActor>(InstanceData.ActorClass, SpawnLocation, SpawnRotation);
    if (!NewActor)
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Ability] SpawnActor FAILED: SpawnActor returned null entity=[%d:%d] class=%s location=%s"),
            Entity.Index, Entity.SerialNumber,
            *InstanceData.ActorClass->GetName(),
            *SpawnLocation.ToString());
        return EStateTreeRunStatus::Failed;
    }

    InstanceData.SpawnedActor = NewActor;

    return EStateTreeRunStatus::Succeeded;
}
