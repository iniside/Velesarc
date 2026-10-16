// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcCooldownProcessor.h"
#include "Mass/ArcMassAbilitiesProcessorGroups.h"
#include "ArcMassAbilitiesLogs.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Engine/World.h"

UArcCooldownProcessor::UArcCooldownProcessor()
    : EntityQuery{*this}
{
    bAutoRegisterWithProcessingPhases = true;
    // GT-pinned: OnExpired dispatch may mutate game-thread-only state (e.g. cascade's FArcItemData
    // ResetStacks/IncrementVersion), matching the prior GT-pinned UArcRPGCascadeResetProcessor.
    bRequiresGameThreadExecution = true;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
    ExecutionOrder.ExecuteInGroup = UE::ArcMassAbilities::ProcessorGroups::Tasks;
}

void UArcCooldownProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.AddRequirement<FArcAbilityCooldownFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcCooldownProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = Context.GetDeltaTimeSeconds();

    EntityQuery.ForEachEntityChunk(Context,
        [DeltaTime, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        TArrayView<FArcAbilityCooldownFragment> Cooldowns = ChunkContext.GetMutableFragmentView<FArcAbilityCooldownFragment>();

        UWorld* World = ChunkContext.GetWorld();
        UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr;

        for (int32 Idx = 0; Idx < NumEntities; ++Idx)
        {
            FArcAbilityCooldownFragment& Frag = Cooldowns[Idx];

            for (int32 CdIdx = Frag.ActiveCooldowns.Num() - 1; CdIdx >= 0; --CdIdx)
            {
                FArcCooldownEntry& Entry = Frag.ActiveCooldowns[CdIdx];
                Entry.RemainingTime -= DeltaTime;
                if (Entry.RemainingTime <= 0.f)
                {
                    const FMassEntityHandle EntityHandle = ChunkContext.GetEntity(Idx);
                    ARCMA_VLOG(World, Log,
                        TEXT("[Cooldown] Expired entry ability=%s entity=%s"),
                        *Entry.AbilityHandle.ToString(),
                        *EntityHandle.DebugGetDescription());

                    const UArcAbilityDefinition* Def = nullptr;
                    const FArcActiveAbility* Active = ArcAbilities::FindAbility(EntityManager, EntityHandle, Entry.AbilityHandle);
                    if (Active)
                    {
                        Def = Active->Definition;
                    }

                    if (Def && Def->Cooldown.IsValid())
                    {
                        const FArcAbilityCooldown* CdPtr = Def->Cooldown.GetPtr<FArcAbilityCooldown>();
                        if (CdPtr)
                        {
                            CdPtr->OnExpired(EntityManager, EntityHandle, Entry.AbilityHandle);
                        }
                    }

                    if (EventBus)
                    {
                        ArcAbilities::Private::QueueCooldownExpired(*EventBus, EntityManager, EntityHandle, Entry.AbilityHandle);
                    }

                    Frag.ActiveCooldowns.RemoveAtSwap(CdIdx);
                }
            }
        }
    });
}
