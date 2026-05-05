// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassQuickBarTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "Mass/EntityFragments.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "QuickBar/ArcMassQuickBarAutoSelectProcessor.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarSharedFragment.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Abilities/ArcAbilityHandle.h"

namespace ArcMassItems::QuickBar::TestHelpers
{

UArcMassQuickBarAutoSelectProcessor* RegisterQuickBarProcessor(FActorTestSpawner& Spawner, FMassEntityManager& EntityManager)
{
    UWorld* World = &Spawner.GetWorld();
    UArcMassQuickBarAutoSelectProcessor* Processor = NewObject<UArcMassQuickBarAutoSelectProcessor>(World);
    Processor->CallInitialize(EntityManager.GetOwner(), EntityManager.AsShared());
    return Processor;
}

UArcMassQuickBarConfig* BuildQuickBarConfig(TFunctionRef<void(UArcMassQuickBarConfig&)> Builder)
{
    UArcMassQuickBarConfig* Config = NewObject<UArcMassQuickBarConfig>();
    Builder(*Config);
    return Config;
}

FMassEntityHandle CreateQuickBarEntity(
    FMassEntityManager& EntityManager,
    UArcMassQuickBarConfig* Config,
    TArrayView<const FInstancedStruct> ExtraFragments)
{
    TArray<FInstancedStruct> Fragments;
    Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
    for (const FInstancedStruct& Extra : ExtraFragments)
    {
        Fragments.Add(Extra);
    }

    FMassEntityHandle Entity = EntityManager.CreateEntity(Fragments);
    EntityManager.AddSparseElementToEntity<FArcMassItemStoreFragment>(Entity);
    EntityManager.AddSparseElementToEntity<FArcMassQuickBarStateFragment>(Entity);

    FArcMassQuickBarSharedFragment SharedFrag;
    SharedFrag.Config = Config;
    const FConstSharedStruct& SharedStruct = EntityManager.GetOrCreateConstSharedFragment(SharedFrag);
    EntityManager.AddConstSharedFragmentToEntity(Entity, SharedStruct);

    return Entity;
}

void InjectGrantedAbility(
    FMassEntityManager& EntityManager,
    FMassEntityHandle Entity,
    FArcAbilityHandle Handle)
{
    FMassEntityView View(EntityManager, Entity);
    FArcAbilityCollectionFragment* Collection = View.GetFragmentDataPtr<FArcAbilityCollectionFragment>();
    checkf(Collection, TEXT("InjectGrantedAbility: entity must be created with FArcAbilityCollectionFragment in ExtraFragments"));

    FArcActiveAbility& Ability = Collection->Abilities.AddDefaulted_GetRef();
    Ability.Handle = Handle;
}

}
