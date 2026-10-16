// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassQuickBarTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/EntityFragments.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "QuickBar/ArcMassQuickBarAutoSelect.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarSharedFragment.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Abilities/ArcAbilityHandle.h"

namespace ArcMassItems::QuickBar::TestHelpers
{

void RunAutoSelect(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
{
    const FArcMassItemEventFragment* EventFrag =
        FMassEntityView(EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassItemEventFragment>();
    if (!EventFrag)
    {
        return;
    }

    // Copy first — downstream quick-bar handlers may commit transactions that
    // mutate the live event array.
    TArray<FArcMassItemEvent> Events = EventFrag->Events;
    for (const FArcMassItemEvent& Event : Events)
    {
        const UScriptStruct* StoreType = Event.StoreType.Get();
        switch (Event.Type)
        {
        case EArcMassItemEventType::SlotAssigned:
            ArcMassItems::QuickBar::AutoSelect::HandleSlotAssigned(EntityManager, Entity, Event.ItemId, Event.SlotTag, StoreType);
            break;
        case EArcMassItemEventType::SlotCleared:
            ArcMassItems::QuickBar::AutoSelect::HandleSlotCleared(EntityManager, Entity, Event.ItemId, StoreType);
            break;
        case EArcMassItemEventType::SlotChanged:
            ArcMassItems::QuickBar::AutoSelect::HandleSlotChanged(EntityManager, Entity, Event.ItemId, Event.SlotTag, StoreType);
            break;
        default:
            break;
        }
    }
}

UArcMassQuickBarConfig* BuildQuickBarConfig(TFunctionRef<void(UArcMassQuickBarConfig&)> Builder)
{
    UArcMassQuickBarConfig* Config = NewObject<UArcMassQuickBarConfig>();
    Config->StoreType = FArcMassItemStoreFragment::StaticStruct();
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

    FArcMassQuickBarStateFragment* State = FMassEntityView(EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
    checkf(State, TEXT("CreateQuickBarEntity: FArcMassQuickBarStateFragment was not added"));
    TArray<TObjectPtr<UArcMassQuickBarConfig>> Configs;
    Configs.Add(Config);
    ArcMassItems::QuickBar::SeedBarStates(Configs, *State);

    FArcMassQuickBarSharedFragment SharedFrag;
    SharedFrag.Configs.Add(Config);
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
    FArcAbilityCollectionFragment* Collection = View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
    checkf(Collection, TEXT("InjectGrantedAbility: entity must be created with FArcAbilityCollectionFragment in ExtraFragments"));

    FArcActiveAbility& Ability = Collection->Abilities.AddDefaulted_GetRef();
    Ability.Handle = Handle;
}

}
