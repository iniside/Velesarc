// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcMassAbilitySet.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Events/ArcEffectEventSubsystem.h"
#include "Events/ArcPendingEvents.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassSignalSubsystem.h"
#include "Signals/ArcAbilitySignals.h"

namespace ArcAbilities
{

FArcAbilityHandle GrantAbility(FMassEntityManager& EntityManager,
                               FMassEntityHandle Entity,
                               const UArcAbilityDefinition* Definition,
                               FGameplayTag InputTag,
                               FMassEntityHandle SourceEntity,
                               const FInstancedStruct& SourceData)
{
    check(Definition);
    check(EntityManager.IsEntityValid(Entity));

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityCollectionFragment& Collection = EntityView.GetFragmentData<FArcAbilityCollectionFragment>();

    FArcActiveAbility NewAbility;
    NewAbility.Handle = FArcAbilityHandle::Make(Collection.Abilities.Num(), Collection.NextGeneration++);
    NewAbility.Definition = Definition;
    NewAbility.InputTag = InputTag;
    NewAbility.SourceEntity = SourceEntity;
    NewAbility.SourceData = SourceData;
    NewAbility.RunStatus = EStateTreeRunStatus::Unset;
    NewAbility.bIsActive = false;

    Collection.Abilities.Add(MoveTemp(NewAbility));

    return Collection.Abilities.Last().Handle;
}

void RemoveAbility(FMassEntityManager& EntityManager,
                   FMassEntityHandle Entity,
                   FArcAbilityHandle Handle,
                   UArcAbilityStateTreeSubsystem& Subsystem)
{
    if (!EntityManager.IsEntityValid(Entity) || !Handle.IsValid())
    {
        return;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityCollectionFragment& Collection = EntityView.GetFragmentData<FArcAbilityCollectionFragment>();
    FArcOwnedTagsFragment& OwnedTags = EntityView.GetFragmentData<FArcOwnedTagsFragment>();

    for (int32 Idx = Collection.Abilities.Num() - 1; Idx >= 0; --Idx)
    {
        FArcActiveAbility& Ability = Collection.Abilities[Idx];
        if (Ability.Handle == Handle)
        {
            if (Ability.bIsActive && Ability.Definition)
            {
                TArray<FArcPendingTagEvent> PendingTagEvents;
                OwnedTags.RemoveTags(Ability.Definition->GrantTagsWhileActive, PendingTagEvents, Entity);
                UArcEffectEventSubsystem* Events = UWorld::GetSubsystem<UArcEffectEventSubsystem>(EntityManager.GetWorld());
                if (Events)
                {
                    for (const FArcPendingTagEvent& Evt : PendingTagEvents)
                    {
                        Events->BroadcastTagCountChanged(Evt.Entity, Evt.Tag, Evt.OldCount, Evt.NewCount);
                    }
                }
            }

            if (Ability.InstanceHandle.IsValid())
            {
                Subsystem.FreeInstanceData(Ability.InstanceHandle);
            }

            Collection.Abilities.RemoveAtSwap(Idx);
            return;
        }
    }
}

TArray<FArcAbilityHandle> GrantAbilitySet(FMassEntityManager& EntityManager,
                                          FMassEntityHandle Entity,
                                          const UArcMassAbilitySet* AbilitySet)
{
    check(AbilitySet);

    TArray<FArcAbilityHandle> Handles;
    Handles.Reserve(AbilitySet->Abilities.Num());

    for (const FArcMassAbilitySetEntry& Entry : AbilitySet->Abilities)
    {
        if (Entry.AbilityDefinition)
        {
            Handles.Add(GrantAbility(EntityManager, Entity, Entry.AbilityDefinition, Entry.InputTag));
        }
    }

    return Handles;
}

void RemoveAbilitySet(FMassEntityManager& EntityManager,
                      FMassEntityHandle Entity,
                      const TArray<FArcAbilityHandle>& Handles,
                      UArcAbilityStateTreeSubsystem& Subsystem)
{
    for (const FArcAbilityHandle& Handle : Handles)
    {
        RemoveAbility(EntityManager, Entity, Handle, Subsystem);
    }
}

const FArcActiveAbility* FindAbility(FMassEntityManager& EntityManager,
                                     FMassEntityHandle Entity,
                                     FArcAbilityHandle Handle)
{
    if (!EntityManager.IsEntityValid(Entity) || !Handle.IsValid())
    {
        return nullptr;
    }

    const FMassEntityView EntityView(EntityManager, Entity);
    const FArcAbilityCollectionFragment& Collection = EntityView.GetFragmentData<FArcAbilityCollectionFragment>();

    for (const FArcActiveAbility& Ability : Collection.Abilities)
    {
        if (Ability.Handle == Handle)
        {
            return &Ability;
        }
    }
    return nullptr;
}

bool IsAbilityActive(FMassEntityManager& EntityManager,
                     FMassEntityHandle Entity,
                     FArcAbilityHandle Handle)
{
    const FArcActiveAbility* Ability = FindAbility(EntityManager, Entity, Handle);
    return Ability && Ability->bIsActive;
}

bool TryActivateAbility(FMassEntityManager& EntityManager,
                        FMassEntityHandle Entity,
                        FArcAbilityHandle Handle)
{
    if (!EntityManager.IsEntityValid(Entity) || !Handle.IsValid())
    {
        return false;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityCollectionFragment& Collection = EntityView.GetFragmentData<FArcAbilityCollectionFragment>();

    for (FArcActiveAbility& Ability : Collection.Abilities)
    {
        if (Ability.Handle == Handle && !Ability.bIsActive && Ability.InputTag.IsValid())
        {
            PushInput(EntityManager, Entity, Ability.InputTag);
            return true;
        }
    }
    return false;
}

void CancelAbility(FMassEntityManager& EntityManager,
                   FMassEntityHandle Entity,
                   FArcAbilityHandle Handle,
                   UArcAbilityStateTreeSubsystem& Subsystem)
{
    if (!EntityManager.IsEntityValid(Entity) || !Handle.IsValid())
    {
        return;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityCollectionFragment& Collection = EntityView.GetFragmentData<FArcAbilityCollectionFragment>();
    FArcOwnedTagsFragment& OwnedTags = EntityView.GetFragmentData<FArcOwnedTagsFragment>();

    for (FArcActiveAbility& Ability : Collection.Abilities)
    {
        if (Ability.Handle == Handle && Ability.bIsActive)
        {
            if (Ability.Definition)
            {
                TArray<FArcPendingTagEvent> PendingTagEvents;
                OwnedTags.RemoveTags(Ability.Definition->GrantTagsWhileActive, PendingTagEvents, Entity);
                UArcEffectEventSubsystem* Events = UWorld::GetSubsystem<UArcEffectEventSubsystem>(EntityManager.GetWorld());
                if (Events)
                {
                    for (const FArcPendingTagEvent& Evt : PendingTagEvents)
                    {
                        Events->BroadcastTagCountChanged(Evt.Entity, Evt.Tag, Evt.OldCount, Evt.NewCount);
                    }
                }
            }

            if (Ability.InstanceHandle.IsValid())
            {
                Subsystem.FreeInstanceData(Ability.InstanceHandle);
                Ability.InstanceHandle = FMassStateTreeInstanceHandle();
            }

            Ability.bIsActive = false;
            Ability.RunStatus = EStateTreeRunStatus::Failed;
            return;
        }
    }
}

void PushInput(FMassEntityManager& EntityManager,
               FMassEntityHandle Entity,
               FGameplayTag InputTag)
{
    if (!EntityManager.IsEntityValid(Entity) || !InputTag.IsValid())
    {
        return;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityInputFragment* Input = EntityView.GetFragmentDataPtr<FArcAbilityInputFragment>();
    if (!Input)
    {
        return;
    }
    Input->PressedThisFrame.AddUnique(InputTag);
    Input->HeldInputs.AddTag(InputTag);

    UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
    if (SignalSubsystem)
    {
        SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);
        SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
    }
}

void ReleaseInput(FMassEntityManager& EntityManager,
                  FMassEntityHandle Entity,
                  FGameplayTag InputTag)
{
    if (!EntityManager.IsEntityValid(Entity) || !InputTag.IsValid())
    {
        return;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityInputFragment* Input = EntityView.GetFragmentDataPtr<FArcAbilityInputFragment>();
    if (!Input)
    {
        return;
    }
    Input->ReleasedThisFrame.AddUnique(InputTag);
    Input->HeldInputs.RemoveTag(InputTag);

    UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
    if (SignalSubsystem)
    {
        SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);
        SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
    }
}

TOptional<FArcAbilityHandle> TryGrantAbilitySafe(FMassEntityManager& EntityManager,
                                                  FMassEntityHandle Entity,
                                                  const UArcAbilityDefinition* Definition,
                                                  FGameplayTag InputTag,
                                                  FMassEntityHandle SourceEntity,
                                                  const FInstancedStruct& SourceData)
{
    if (!Definition || !EntityManager.IsEntityValid(Entity))
    {
        return {};
    }

    FMassEntityView EntityView(EntityManager, Entity);
    if (!EntityView.HasFragment<FArcAbilityCollectionFragment>())
    {
        return {};
    }

    return GrantAbility(EntityManager, Entity, Definition, InputTag, SourceEntity, SourceData);
}

bool TryRemoveAbilitySafe(FMassEntityManager& EntityManager,
                          FMassEntityHandle Entity,
                          FArcAbilityHandle Handle)
{
    if (!Handle.IsValid() || !EntityManager.IsEntityValid(Entity))
    {
        return false;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    if (!EntityView.HasFragment<FArcAbilityCollectionFragment>())
    {
        return false;
    }

    UArcAbilityStateTreeSubsystem* Subsystem = UWorld::GetSubsystem<UArcAbilityStateTreeSubsystem>(EntityManager.GetWorld());
    if (!Subsystem)
    {
        return false;
    }

    RemoveAbility(EntityManager, Entity, Handle, *Subsystem);
    return true;
}

} // namespace ArcAbilities
