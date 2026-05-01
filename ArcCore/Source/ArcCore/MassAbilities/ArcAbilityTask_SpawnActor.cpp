// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_SpawnActor.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "MassEntityManager.h"
#include "Mass/EntityFragments.h"

EStateTreeRunStatus FArcAbilityTask_SpawnActor::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityTask_SpawnActorInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_SpawnActorInstanceData>(*this);

    if (!InstanceData.ActorClass)
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

    const FRotator SpawnRotation = FRotator::ZeroRotator;

    AActor* NewActor = World->SpawnActor<AActor>(InstanceData.ActorClass, SpawnLocation, SpawnRotation);
    if (!NewActor)
    {
        return EStateTreeRunStatus::Failed;
    }

    InstanceData.SpawnedActor = NewActor;

    return EStateTreeRunStatus::Succeeded;
}
