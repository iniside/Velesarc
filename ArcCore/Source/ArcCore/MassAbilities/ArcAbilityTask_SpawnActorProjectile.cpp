// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_SpawnActorProjectile.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "MassAbilities/ArcAbilitySourceData_Item.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Items/ArcItemDefinition.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

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
    UArcCoreAbilitySystemComponent* ASC = Actor ? Actor->FindComponentByClass<UArcCoreAbilitySystemComponent>() : nullptr;
    if (!ASC)
    {
        return EStateTreeRunStatus::Failed;
    }

    if (!InstanceData.OriginTargetingTag.IsValid() || !InstanceData.TargetTargetingTag.IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    // Resolve actor class -- direct property or item fragment fallback
    TSubclassOf<AActor> ResolvedClass = InstanceData.ActorClass;
    if (!ResolvedClass)
    {
        const FArcActiveAbility* ActiveAbility = ArcAbilities::FindAbility(EntityManager, Entity, AbilityContext.GetAbilityHandle());
        if (ActiveAbility)
        {
            const FArcAbilitySourceData_Item* ItemSource = ActiveAbility->SourceData.GetPtr<FArcAbilitySourceData_Item>();
            if (ItemSource && ItemSource->ItemDefinition)
            {
                const FArcItemFragment_ProjectileSettings* ProjFragment = ItemSource->ItemDefinition->FindFragment<FArcItemFragment_ProjectileSettings>();
                if (ProjFragment)
                {
                    ResolvedClass = ProjFragment->ActorClass;
                }
            }
        }
    }

    if (!ResolvedClass)
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

    // Get target location from global targeting
    bool bTargetSuccess = false;
    FVector TargetLocation = UArcCoreAbilitySystemComponent::GetGlobalHitLocation(ASC, InstanceData.TargetTargetingTag, bTargetSuccess);
    if (!bTargetSuccess)
    {
        return EStateTreeRunStatus::Failed;
    }

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
}
