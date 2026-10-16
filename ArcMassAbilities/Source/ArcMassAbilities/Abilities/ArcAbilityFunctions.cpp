// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/ArcAbilityFunctions.h"
#include "ArcMassAbilitiesLogs.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcMassAbilitySet.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Effects/ArcEffectContext.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Events/ArcGameEventFunctions.h"
#include "Events/ArcTagEventPayloads.h"
#include "Debugger/ArcMassAbilityTrace.h"
#include "Events/ArcPendingEvents.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "Signals/ArcAbilitySignals.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "StateTree.h"

namespace ArcAbilities
{

namespace Private
{

// ---------------------------------------------------------------------------
// File-private: shared input-edge classification + signal emission
// ---------------------------------------------------------------------------
namespace ArcAbilityInputPrivate
{

static void ClassifyAndSignalInputEdge(FMassEntityManager& EntityManager,
                                       FMassEntityHandle Entity,
                                       FGameplayTag InputTag,
                                       bool bPressed,
                                       const FGameplayTagContainer& OriginTags)
{
    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityInputFragment* Input = EntityView.GetSparseFragmentDataPtr<FArcAbilityInputFragment>();
    if (!Input)
    {
        return;
    }

    if (bPressed)
    {
        Input->PressedThisFrame.AddUnique(InputTag);
        Input->HeldInputs.AddTag(InputTag);
        if (OriginTags.IsEmpty())
        {
            Input->PressedOriginTags.Remove(InputTag);
        }
        else
        {
            Input->PressedOriginTags.Add(InputTag, OriginTags);
        }
    }
    else
    {
        Input->ReleasedThisFrame.AddUnique(InputTag);
        Input->HeldInputs.RemoveTag(InputTag);
    }

    // Classify the edge against the entity's collection.
    bool bAnyActiveMatch = false;
    bool bAnyInactiveMatch = false;
    if (const FArcAbilityCollectionFragment* Coll = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>())
    {
        for (const FArcActiveAbility& Ability : Coll->Abilities)
        {
            if (Ability.InputTag != InputTag) { continue; }
            if (Ability.bIsActive)
            {
                bAnyActiveMatch = true;
            }
            else
            {
                bAnyInactiveMatch = true;
            }
        }
    }

    UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
    if (SignalSubsystem)
    {
        const TCHAR* const EdgeDir = bPressed ? TEXT("pressed") : TEXT("released");
        if (bAnyInactiveMatch)
        {
            ARCMA_VLOG(EntityManager.GetWorld(), Verbose,
                TEXT("[ArcAbilities] SignalEntity(AbilityInputReceived) entity=[%d:%d] tag=%s edge=%s"),
                Entity.Index, Entity.SerialNumber, *InputTag.ToString(), EdgeDir);
            SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);
            TRACE_ARCMA_SIGNAL_RAISED(UWorld::GetSubsystem<UArcGameEventSubsystem>(EntityManager.GetWorld()), Entity,
                UE::ArcMassAbilities::Signals::AbilityInputReceived);
        }
        if (bAnyActiveMatch)
        {
            ARCMA_VLOG(EntityManager.GetWorld(), Verbose,
                TEXT("[ArcAbilities] SignalEntity(AbilityActiveInputReceived) entity=[%d:%d] tag=%s edge=%s"),
                Entity.Index, Entity.SerialNumber, *InputTag.ToString(), EdgeDir);
            SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityActiveInputReceived, Entity);
            TRACE_ARCMA_SIGNAL_RAISED(UWorld::GetSubsystem<UArcGameEventSubsystem>(EntityManager.GetWorld()), Entity,
                UE::ArcMassAbilities::Signals::AbilityActiveInputReceived);
        }
    }
}

} // namespace ArcAbilityInputPrivate

// ---------------------------------------------------------------------------

namespace ArcOwnedTagsPrivate
{

static void PublishTagChanges(FMassEntityManager& EntityManager,
                              FMassEntityHandle Entity,
                              TConstArrayView<FArcPendingTagEvent> Events)
{
    if (Events.IsEmpty())
    {
        return;
    }

    UWorld* World = EntityManager.GetWorld();
    UArcGameEventSubsystem* EventBus = UWorld::GetSubsystem<UArcGameEventSubsystem>(World);
    if (EventBus)
    {
        FArcGameEvent TagsAfterChange;
        TagsAfterChange.Instigator = Entity;
        TagsAfterChange.Target = Entity;
        ArcGameEvents::GatherEnvelopeTags(EntityManager, TagsAfterChange);

        for (const FArcPendingTagEvent& Evt : Events)
        {
            TRACE_ARCMA_TAG_COUNT_CHANGED(EventBus, Evt.Entity, Evt.Tag, Evt.OldCount, Evt.NewCount);

            const bool bAdded = Evt.OldCount == 0 && Evt.NewCount > 0;
            const bool bRemoved = Evt.OldCount > 0 && Evt.NewCount == 0;
            if (!bAdded && !bRemoved)
            {
                continue;
            }

            FArcTagChangedPayload Payload;
            Payload.Tag = Evt.Tag;
            Payload.OldCount = Evt.OldCount;
            Payload.NewCount = Evt.NewCount;

            FArcGameEvent Event;
            Event.Tag = bAdded ? ArcGameEventTags::TagAdded.GetTag() : ArcGameEventTags::TagRemoved.GetTag();
            Event.Instigator = Evt.Entity;
            Event.Target = Evt.Entity;
            Event.SourceTags = TagsAfterChange.SourceTags;
            Event.TargetTags = TagsAfterChange.TargetTags;
            Event.Payload = FInstancedStruct::Make(Payload);
            EventBus->Queue(MoveTemp(Event));
        }
    }

    UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
    if (SignalSubsystem)
    {
        SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
        TRACE_ARCMA_SIGNAL_RAISED(EventBus, Entity, UE::ArcMassAbilities::Signals::AttributeRecalculate);
    }
}

} // namespace ArcOwnedTagsPrivate

// ---------------------------------------------------------------------------

void StopAndFreeAbilityTree(FArcActiveAbility& Ability,
                            UArcAbilityStateTreeSubsystem& Subsystem,
                            FMassEntityHandle Entity,
                            FMassExecutionContext& MassContext)
{
	if (!Ability.InstanceHandle.IsValid())
	{
		return;
	}

	FStateTreeInstanceData* InstanceData = Subsystem.GetInstanceData(Ability.InstanceHandle);
	if (InstanceData && Ability.Definition && Ability.Definition->StateTree)
	{
		FArcAbilityExecutionContext StopContext(Subsystem, *Ability.Definition->StateTree, *InstanceData, MassContext);
		StopContext.SetEntity(Entity);
		StopContext.Stop();
	}

	Subsystem.FreeInstanceData(Ability.InstanceHandle);
	Ability.InstanceHandle = FMassStateTreeInstanceHandle();
}

void QueueAbilityEnded(FMassEntityManager& EntityManager,
                       FMassEntityHandle Entity,
                       const FArcActiveAbility& Ability,
                       EArcAbilityEndReason Reason,
                       FMassEntityHandle Instigator)
{
    UWorld* World = EntityManager.GetWorld();
    if (UArcGameEventSubsystem* EventBus = UWorld::GetSubsystem<UArcGameEventSubsystem>(World))
    {
        TRACE_ARCMA_ABILITY_ENDED(EventBus, Entity, Ability.Definition.Get(), static_cast<uint8>(Reason), Instigator);

        FArcAbilityEndedPayload Payload;
        Payload.Definition = Ability.Definition.Get();
        Payload.Handle = Ability.Handle;
        Payload.Reason = Reason;

        FArcGameEvent Event;
        Event.Tag = ArcGameEventTags::AbilityEnded;
        Event.Instigator = Instigator.IsSet() ? Instigator : Entity;
        Event.Target = Entity;
        ArcGameEvents::GatherEnvelopeTags(EntityManager, Event, MakeAbilityEffectContext(Entity, Ability));
        Event.Payload = FInstancedStruct::Make(Payload);
        EventBus->Queue(MoveTemp(Event));
    }
}

void QueueAbilityActivated(UArcGameEventSubsystem& EventBus,
                           FMassEntityManager& EntityManager,
                           FMassEntityHandle Entity,
                           FArcAbilityHandle Handle)
{
    const FArcEffectContext AbilityContext = ArcAbilities::MakeAbilityEffectContext(EntityManager, Entity, Handle);
    TRACE_ARCMA_ABILITY_ACTIVATED(&EventBus, Entity, AbilityContext.SourceAbility.Get());

    FArcAbilityActivatedPayload Payload;
    Payload.Definition = AbilityContext.SourceAbility.Get();
    Payload.Handle = Handle;

    FArcGameEvent Event;
    Event.Tag = ArcGameEventTags::AbilityActivated;
    Event.Instigator = Entity;
    Event.Target = Entity;
    ArcGameEvents::GatherEnvelopeTags(EntityManager, Event, AbilityContext);
    Event.Payload = FInstancedStruct::Make(Payload);
    EventBus.Queue(MoveTemp(Event));
}

void QueueCostFailed(UArcGameEventSubsystem& EventBus,
                     FMassEntityManager& EntityManager,
                     FMassEntityHandle Entity,
                     FArcAbilityHandle Handle,
                     const FArcCostCheckResult& Result)
{
    const FArcEffectContext AbilityContext = ArcAbilities::MakeAbilityEffectContext(EntityManager, Entity, Handle);
    TRACE_ARCMA_COST_FAILED(&EventBus, Entity, AbilityContext.SourceAbility.Get(), static_cast<uint8>(Result.Reason),
        Result.MissingAmount);

    FArcAbilityCostFailedPayload Payload;
    Payload.Definition = AbilityContext.SourceAbility.Get();
    Payload.Reason = Result.Reason;
    Payload.MissingAmount = Result.MissingAmount;

    FArcGameEvent Event;
    Event.Tag = ArcGameEventTags::AbilityCostFailed;
    Event.Instigator = Entity;
    Event.Target = Entity;
    Event.Magnitude = Result.MissingAmount;
    ArcGameEvents::GatherEnvelopeTags(EntityManager, Event, AbilityContext);
    Event.Payload = FInstancedStruct::Make(Payload);
    EventBus.Queue(MoveTemp(Event));
}

void QueueCooldownFailed(UArcGameEventSubsystem& EventBus,
                         FMassEntityManager& EntityManager,
                         FMassEntityHandle Entity,
                         FArcAbilityHandle Handle,
                         const FArcCooldownCheckResult& Result)
{
    const FArcEffectContext AbilityContext = ArcAbilities::MakeAbilityEffectContext(EntityManager, Entity, Handle);
    TRACE_ARCMA_COOLDOWN_FAILED(&EventBus, Entity, AbilityContext.SourceAbility.Get(), Result.RemainingSeconds * 1000.f);

    FArcAbilityCooldownFailedPayload Payload;
    Payload.Definition = AbilityContext.SourceAbility.Get();
    Payload.RemainingSeconds = Result.RemainingSeconds;

    FArcGameEvent Event;
    Event.Tag = ArcGameEventTags::AbilityCooldownFailed;
    Event.Instigator = Entity;
    Event.Target = Entity;
    Event.Magnitude = Result.RemainingSeconds;
    ArcGameEvents::GatherEnvelopeTags(EntityManager, Event, AbilityContext);
    Event.Payload = FInstancedStruct::Make(Payload);
    EventBus.Queue(MoveTemp(Event));
}

void QueueCooldownExpired(UArcGameEventSubsystem& EventBus,
                          FMassEntityManager& EntityManager,
                          FMassEntityHandle Entity,
                          FArcAbilityHandle Handle)
{
    const FArcEffectContext AbilityContext = ArcAbilities::MakeAbilityEffectContext(EntityManager, Entity, Handle);

    FArcAbilityCooldownExpiredPayload Payload;
    Payload.Definition = AbilityContext.SourceAbility.Get();
    Payload.Handle = Handle;

    FArcGameEvent Event;
    Event.Tag = ArcGameEventTags::AbilityCooldownExpired;
    Event.Instigator = Entity;
    Event.Target = Entity;
    ArcGameEvents::GatherEnvelopeTags(EntityManager, Event, AbilityContext);
    Event.Payload = FInstancedStruct::Make(Payload);
    EventBus.Queue(MoveTemp(Event));
}

void CancelActiveAbility(FMassEntityManager& EntityManager,
                         FMassEntityHandle Entity,
                         FArcActiveAbility& Ability,
                         UArcAbilityStateTreeSubsystem& Subsystem,
                         FMassExecutionContext& MassContext,
                         EArcAbilityEndReason Reason,
                         FMassEntityHandle Instigator)
{
    if (Ability.Definition)
    {
        RemoveOwnedTags(EntityManager, Entity, Ability.Definition->GrantTagsWhileActive);
    }

    StopAndFreeAbilityTree(Ability, Subsystem, Entity, MassContext);

    Ability.bIsActive = false;
    Ability.RunStatus = EStateTreeRunStatus::Failed;

    QueueAbilityEnded(EntityManager, Entity, Ability, Reason, Instigator);
}

} // namespace Private

void AddOwnedTags(FMassEntityManager& EntityManager,
                  FMassEntityHandle Entity,
                  const FGameplayTagContainer& InTags)
{
    if (InTags.IsEmpty() || !EntityManager.IsEntityValid(Entity))
    {
        return;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcOwnedTagsFragment* OwnedTags = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
    if (!OwnedTags)
    {
        return;
    }

    TArray<FArcPendingTagEvent> PendingTagEvents;
    OwnedTags->AddTags(InTags, PendingTagEvents, Entity);
    Private::ArcOwnedTagsPrivate::PublishTagChanges(EntityManager, Entity, PendingTagEvents);
}

void RemoveOwnedTags(FMassEntityManager& EntityManager,
                     FMassEntityHandle Entity,
                     const FGameplayTagContainer& InTags)
{
    if (InTags.IsEmpty() || !EntityManager.IsEntityValid(Entity))
    {
        return;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcOwnedTagsFragment* OwnedTags = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
    if (!OwnedTags)
    {
        return;
    }

    TArray<FArcPendingTagEvent> PendingTagEvents;
    OwnedTags->RemoveTags(InTags, PendingTagEvents, Entity);
    Private::ArcOwnedTagsPrivate::PublishTagChanges(EntityManager, Entity, PendingTagEvents);
}

FArcAbilityHandle GrantAbility(FMassEntityManager& EntityManager,
                               FMassEntityHandle Entity,
                               const UArcAbilityDefinition* Definition,
                               FGameplayTag InputTag,
                               FMassEntityHandle SourceEntity,
                               FArcAbilityCustomData CustomData,
                               UWorld* World)
{
    check(Definition);
    check(EntityManager.IsEntityValid(Entity));

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityCollectionFragment* CollectionPtr = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
    if (!CollectionPtr)
    {
        ARCMA_VLOG(World, Warning,
            TEXT("[Ability] GrantAbility: entity lacks FArcAbilityCollectionFragment, cannot grant definition=%s entity=%s"),
            *Definition->GetName(),
            *Entity.DebugGetDescription());
        return FArcAbilityHandle();
    }
    FArcAbilityCollectionFragment& Collection = *CollectionPtr;

    FArcActiveAbility NewAbility;
    NewAbility.Handle = FArcAbilityHandle::Make(Collection.Abilities.Num(), Collection.NextGeneration++);
    NewAbility.Definition = Definition;
    NewAbility.InputTag = InputTag;
    NewAbility.SourceEntity = SourceEntity;
    NewAbility.CustomData = MoveTemp(CustomData);
    NewAbility.RunStatus = EStateTreeRunStatus::Unset;
    NewAbility.bIsActive = false;

    ARCMA_VLOG(World, Log,
        TEXT("[Ability] Granted handle=%d.%d definition=%s input=%s entity=%s"),
        NewAbility.Handle.GetIndex(), NewAbility.Handle.GetGeneration(),
        *Definition->GetName(),
        *InputTag.ToString(),
        *Entity.DebugGetDescription());

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
    FArcAbilityCollectionFragment* RemoveCollectionPtr = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
    if (!RemoveCollectionPtr)
    {
        ARCMA_VLOG(EntityManager.GetWorld(), Warning,
            TEXT("[Ability] RemoveAbility: entity lacks FArcAbilityCollectionFragment, cannot remove handle=%d.%d entity=%s"),
            Handle.GetIndex(), Handle.GetGeneration(),
            *Entity.DebugGetDescription());
        return;
    }
    FArcAbilityCollectionFragment& Collection = *RemoveCollectionPtr;

    for (int32 Idx = Collection.Abilities.Num() - 1; Idx >= 0; --Idx)
    {
        FArcActiveAbility& Ability = Collection.Abilities[Idx];
        if (Ability.Handle == Handle)
        {
            if (Ability.bIsActive)
            {
                // Standalone Mass context: gameplay-code path (no processor ChunkContext).
                // Default flush-on-destruct is intentional — ExitState tasks may Context.Defer()
                // cleanup, and FMassEntityManager self-queues the flush when mid-processing.
                FMassExecutionContext ExecCtx(EntityManager, /*DeltaTimeSeconds*/0.f);
                Private::CancelActiveAbility(EntityManager, Entity, Ability, Subsystem, ExecCtx,
                    EArcAbilityEndReason::Cancelled, FMassEntityHandle());
            }
            else if (Ability.InstanceHandle.IsValid())
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
                                          const UArcMassAbilitySet* AbilitySet,
                                          UWorld* World)
{
    check(AbilitySet);

    TArray<FArcAbilityHandle> Handles;
    Handles.Reserve(AbilitySet->Abilities.Num());

    for (const FArcMassAbilitySetEntry& Entry : AbilitySet->Abilities)
    {
        if (Entry.AbilityDefinition)
        {
            Handles.Add(GrantAbility(EntityManager, Entity, Entry.AbilityDefinition, Entry.InputTag,
                                     FMassEntityHandle(), FArcAbilityCustomData(), World));
        }
    }

    ARCMA_VLOG(World, Log,
        TEXT("[Ability] GrantAbilitySet complete set=%s granted=%d entity=%s"),
        *AbilitySet->GetName(),
        Handles.Num(),
        *Entity.DebugGetDescription());

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
    const FArcAbilityCollectionFragment* FindCollectionPtr = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
    if (!FindCollectionPtr)
    {
        ARCMA_VLOG(EntityManager.GetWorld(), Warning,
            TEXT("[Ability] FindAbility: entity lacks FArcAbilityCollectionFragment handle=%d.%d entity=%s"),
            Handle.GetIndex(), Handle.GetGeneration(),
            *Entity.DebugGetDescription());
        return nullptr;
    }
    const FArcAbilityCollectionFragment& Collection = *FindCollectionPtr;

    for (const FArcActiveAbility& Ability : Collection.Abilities)
    {
        if (Ability.Handle == Handle)
        {
            return &Ability;
        }
    }
    return nullptr;
}

FArcEffectContext MakeAbilityEffectContext(FMassEntityManager& EntityManager,
                                           FMassEntityHandle Entity,
                                           FArcAbilityHandle Handle)
{
    if (const FArcActiveAbility* ActiveAbility = FindAbility(EntityManager, Entity, Handle))
    {
        return MakeAbilityEffectContext(Entity, *ActiveAbility);
    }

    FArcEffectContext EffectContext;
    EffectContext.SourceEntity = Entity;
    EffectContext.SourceAbilityHandle = Handle;
    return EffectContext;
}

FArcEffectContext MakeAbilityEffectContext(FMassEntityHandle Entity,
                                           const FArcActiveAbility& Ability)
{
    FArcEffectContext EffectContext;
    EffectContext.SourceEntity = Entity;
    EffectContext.SourceAbilityHandle = Ability.Handle;
    EffectContext.SourceAbility = const_cast<UArcAbilityDefinition*>(Ability.Definition.Get());
    EffectContext.SourceData = Ability.CustomData.GetPrimary();
    EffectContext.OriginTags = Ability.OriginTags;
    return EffectContext;
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
    FArcAbilityCollectionFragment* TryActivateCollectionPtr = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
    if (!TryActivateCollectionPtr)
    {
        ARCMA_VLOG(EntityManager.GetWorld(), Warning,
            TEXT("[Ability] TryActivateAbility: entity lacks FArcAbilityCollectionFragment handle=%d.%d entity=%s"),
            Handle.GetIndex(), Handle.GetGeneration(),
            *Entity.DebugGetDescription());
        return false;
    }
    FArcAbilityCollectionFragment& Collection = *TryActivateCollectionPtr;

    for (FArcActiveAbility& Ability : Collection.Abilities)
    {
        if (Ability.Handle == Handle && !Ability.bIsActive && Ability.InputTag.IsValid())
        {
            // No other path clears RunStatus at activation-request time, so a prior run's terminal status would be read as this activation's result.
            Ability.RunStatus = EStateTreeRunStatus::Unset;
            PushInput(EntityManager, Entity, Ability.InputTag);
            return true;
        }
    }
    return false;
}

void CancelAbility(FMassEntityManager& EntityManager,
                   FMassEntityHandle Entity,
                   FArcAbilityHandle Handle,
                   UArcAbilityStateTreeSubsystem& Subsystem,
                   EArcAbilityEndReason Reason,
                   FMassEntityHandle Instigator)
{
    if (!EntityManager.IsEntityValid(Entity) || !Handle.IsValid())
    {
        return;
    }

    FMassEntityView EntityView(EntityManager, Entity);
    FArcAbilityCollectionFragment* CancelCollectionPtr = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
    if (!CancelCollectionPtr)
    {
        ARCMA_VLOG(EntityManager.GetWorld(), Warning,
            TEXT("[Ability] CancelAbility: entity lacks FArcAbilityCollectionFragment handle=%d.%d entity=%s"),
            Handle.GetIndex(), Handle.GetGeneration(),
            *Entity.DebugGetDescription());
        return;
    }
    FArcAbilityCollectionFragment& Collection = *CancelCollectionPtr;

    for (FArcActiveAbility& Ability : Collection.Abilities)
    {
        if (Ability.Handle == Handle && Ability.bIsActive)
        {
            // Standalone Mass context: gameplay-code path (no processor ChunkContext). Default
            // flush-on-destruct is intentional — ExitState tasks may Context.Defer() cleanup,
            // and FMassEntityManager self-queues the flush when mid-processing.
            FMassExecutionContext ExecCtx(EntityManager, /*DeltaTimeSeconds*/0.f);
            Private::CancelActiveAbility(EntityManager, Entity, Ability, Subsystem, ExecCtx, Reason, Instigator);
            return;
        }
    }
}

void PushInput(FMassEntityManager& EntityManager,
               FMassEntityHandle Entity,
               FGameplayTag InputTag,
               const FGameplayTagContainer& OriginTags)
{
    if (!EntityManager.IsEntityValid(Entity) || !InputTag.IsValid())
    {
        return;
    }
    Private::ArcAbilityInputPrivate::ClassifyAndSignalInputEdge(EntityManager, Entity, InputTag, /*bPressed=*/true, OriginTags);
}

void ReleaseInput(FMassEntityManager& EntityManager,
                  FMassEntityHandle Entity,
                  FGameplayTag InputTag)
{
    if (!EntityManager.IsEntityValid(Entity) || !InputTag.IsValid())
    {
        return;
    }
    Private::ArcAbilityInputPrivate::ClassifyAndSignalInputEdge(EntityManager, Entity, InputTag, /*bPressed=*/false, FGameplayTagContainer());
}


TOptional<FArcAbilityHandle> TryGrantAbilitySafe(FMassEntityManager& EntityManager,
                                                  FMassEntityHandle Entity,
                                                  const UArcAbilityDefinition* Definition,
                                                  FGameplayTag InputTag,
                                                  FMassEntityHandle SourceEntity,
                                                  FArcAbilityCustomData CustomData)
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

    return GrantAbility(EntityManager, Entity, Definition, InputTag, SourceEntity, MoveTemp(CustomData));
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
