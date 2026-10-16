// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_SpawnActorProjectile.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "MassActorSubsystem.h"
#include "Mass/EntityManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_SpawnActorProjectile::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FArcAbilityTask_SpawnActorProjectileInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_SpawnActorProjectileInstanceData>(*this);

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    FMassEntityHandle Entity = AbilityContext.GetEntity();

    FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    if (!ActorFragment || !ActorFragment->IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    AActor* Actor = ActorFragment->GetMutable();

    if (!InstanceData.OriginTargetingTag.IsValid() || !InstanceData.TargetTargetingTag.IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    // GAS retired (Bucket D 7); GetGlobalHitResult/Location and projectile settings fragment pending Mass-side reimplementation.
    UWorld* World = AbilityContext.GetMassContext().GetWorld();
    ARCMA_VLOG(World, Warning,
        TEXT("[Ability] SpawnActorProjectile STUBBED entity=[%d:%d]"),
        Entity.Index, Entity.SerialNumber);
    return EStateTreeRunStatus::Failed;
#if 0
    FHitResult OriginHit;
    FVector TargetLocation = FVector::ZeroVector;

    UWorld* World = Actor->GetWorld();
    if (!World)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FVector SpawnLocation = OriginHit.ImpactPoint;
    const FVector Direction = (TargetLocation - SpawnLocation).GetSafeNormal();
    const FRotator SpawnRotation = Direction.IsNearlyZero() ? Actor->GetActorRotation() : Direction.Rotation();

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Actor;
    SpawnParams.Instigator = Cast<APawn>(Actor);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* SpawnedActor = World->SpawnActor<AActor>(ResolvedClass, SpawnLocation, SpawnRotation, SpawnParams);
    if (!SpawnedActor)
    {
        return EStateTreeRunStatus::Failed;
    }

    InstanceData.SpawnedActor = SpawnedActor;

    return EStateTreeRunStatus::Succeeded;
#endif
}
