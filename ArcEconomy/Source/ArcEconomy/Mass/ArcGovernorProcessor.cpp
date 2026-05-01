// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcGovernorProcessor.h"
#include "Mass/ArcEconomyFragments.h"
#include "Mass/ArcGovernorLogic.h"
#include "Mass/ArcDebugFragments.h"
#include "Mass/ArcDemandGraph.h"
#include "Mass/EntityFragments.h"
#include "Data/ArcGovernorDataAsset.h"
#include "ArcSettlementSubsystem.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcAreaSubsystem.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"
#include "MassExecutionContext.h"
#include "MassCommandBuffer.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"
#include "Data/ArcSettlementNeedTypes.h"
#include "Data/ArcSettlementNeedDataAsset.h"
#include "Fragments/ArcNeedFragment.h"

UArcGovernorProcessor::UArcGovernorProcessor()
    : GovernorQuery{*this}
{
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
    ProcessingPhase = EMassProcessingPhase::DuringPhysics;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcGovernorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    GovernorQuery.AddRequirement<FArcSettlementFragment>(EMassFragmentAccess::ReadOnly);
    GovernorQuery.AddRequirement<FArcSettlementMarketFragment>(EMassFragmentAccess::ReadOnly);
    GovernorQuery.AddRequirement<FArcSettlementWorkforceFragment>(EMassFragmentAccess::ReadOnly);
    GovernorQuery.AddRequirement<FArcGovernorFragment>(EMassFragmentAccess::ReadWrite);
    GovernorQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcGovernorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UArcSettlementSubsystem* SettlementSub = Context.GetWorld()->GetSubsystem<UArcSettlementSubsystem>();
    UArcKnowledgeSubsystem* KnowledgeSub = Context.GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();
    UMassSignalSubsystem* SignalSub = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
    UArcAreaSubsystem* AreaSub = Context.GetWorld()->GetSubsystem<UArcAreaSubsystem>();
    UArcMassSpatialHashSubsystem* SpatialHashSub = Context.GetWorld()->GetSubsystem<UArcMassSpatialHashSubsystem>();
    if (!SettlementSub || !KnowledgeSub || !SignalSub)
    {
        return;
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(ArcGovernorProcessor);

    const double WorldTime = Context.GetWorld()->GetTimeSeconds();

    // Accumulate all pending changes across all settlements
    TArray<ArcGovernor::FArcGovernorPendingNPCChange> AllNPCChanges;
    TArray<ArcGovernor::FArcGovernorPendingSlotChange> AllSlotChanges;
    TArray<ArcGovernor::FArcGovernorPendingWorkforceUpdate> AllWorkforceUpdates;

    GovernorQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Ctx)
    {
        const TConstArrayView<FArcSettlementFragment> Settlements = Ctx.GetFragmentView<FArcSettlementFragment>();
        const TConstArrayView<FArcSettlementMarketFragment> Markets = Ctx.GetFragmentView<FArcSettlementMarketFragment>();
        const TConstArrayView<FArcSettlementWorkforceFragment> Workforces = Ctx.GetFragmentView<FArcSettlementWorkforceFragment>();
        const TArrayView<FArcGovernorFragment> Governors = Ctx.GetMutableFragmentView<FArcGovernorFragment>();
        const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
        const int32 NumEntities = Ctx.GetNumEntities();

        for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
        {
            FArcGovernorFragment& Governor = Governors[EntityIndex];
            if (!Governor.GovernorConfig)
            {
                continue;
            }

            const FMassEntityHandle SettlementEntity = Ctx.GetEntity(EntityIndex);
            const FArcSettlementFragment& Settlement = Settlements[EntityIndex];
            const FArcSettlementMarketFragment& Market = Markets[EntityIndex];

            const TArray<FMassEntityHandle>& BuildingHandles = SettlementSub->GetBuildingsForSettlement(SettlementEntity);
            const TArray<FMassEntityHandle>& NPCHandles = SettlementSub->GetNPCsForSettlement(SettlementEntity);

            // Pre-fetch NPC data in a single pass
            TArray<ArcGovernor::FNPCData> NPCData;
            ArcGovernor::PreFetchNPCs(EntityManager, NPCHandles, NPCData);

            ArcGovernor::FGovernorContext GovCtx
            {
                EntityManager,
                SettlementEntity,
                Transforms[EntityIndex].GetTransform().GetLocation(),
                Settlement,
                Market,
                *Governor.GovernorConfig,
                BuildingHandles,
                MoveTemp(NPCData),
                AreaSub,
                SpatialHashSub
            };

            // Read settlement needs (if present)
            const FArcSettlementNeedsConfigFragment* NeedsConfig =
                EntityManager.GetSharedFragmentDataPtr<FArcSettlementNeedsConfigFragment>(SettlementEntity);
            GovCtx.NeedsConfig = NeedsConfig;

            if (NeedsConfig)
            {
                for (const TPair<TObjectPtr<UScriptStruct>, TObjectPtr<UArcSettlementNeedDataAsset>>& ConfigPair : NeedsConfig->NeedConfigs)
                {
                    FStructView NeedView = EntityManager.GetFragmentDataStruct(SettlementEntity, ConfigPair.Key);
                    if (NeedView.IsValid())
                    {
                        const FArcNeedFragment& Need = NeedView.Get<FArcNeedFragment>();
                        GovCtx.NeedValues.Add(ConfigPair.Key, Need.CurrentValue);
                    }
                }
            }

            // Build the demand graph for this settlement
            GovCtx.DemandGraph.Rebuild(EntityManager, SettlementEntity, BuildingHandles, Market);

            // Drain debug command queue (if debugger has injected commands)
            FArcDebugCommandQueueFragment* DebugQueue = EntityManager.GetFragmentDataPtr<FArcDebugCommandQueueFragment>(SettlementEntity);
            if (DebugQueue)
            {
                AllNPCChanges.Append(MoveTemp(DebugQueue->PendingNPCChanges));
                DebugQueue->PendingNPCChanges.Reset();

                // Flush debug slot changes eagerly (same as governor slot changes below)
                for (const ArcGovernor::FArcGovernorPendingSlotChange& DebugSlotChange : DebugQueue->PendingSlotChanges)
                {
                    if (!EntityManager.IsEntityValid(DebugSlotChange.BuildingHandle))
                    {
                        continue;
                    }
                    FArcBuildingWorkforceFragment* DebugWorkforce = EntityManager.GetFragmentDataPtr<FArcBuildingWorkforceFragment>(DebugSlotChange.BuildingHandle);
                    if (DebugWorkforce && DebugWorkforce->Slots.IsValidIndex(DebugSlotChange.SlotIndex))
                    {
                        DebugWorkforce->Slots[DebugSlotChange.SlotIndex].DesiredRecipe = DebugSlotChange.NewDesiredRecipe;
                    }
                }
                AllSlotChanges.Append(MoveTemp(DebugQueue->PendingSlotChanges));
                DebugQueue->PendingSlotChanges.Reset();
            }

            // Phase 1: Production — activate recipes driven by demand, deactivate idle ones
            const int32 SlotChangeBase = AllSlotChanges.Num();
            ArcGovernor::ActivateRecipesFromDemand(GovCtx, AllSlotChanges);
            ArcGovernor::DeactivateIdleRecipes(GovCtx, AllSlotChanges);

            // Flush slot changes eagerly so AssignWorkers sees updated DesiredRecipe this frame
            for (int32 Idx = SlotChangeBase; Idx < AllSlotChanges.Num(); ++Idx)
            {
                const ArcGovernor::FArcGovernorPendingSlotChange& Change = AllSlotChanges[Idx];
                if (!EntityManager.IsEntityValid(Change.BuildingHandle))
                {
                    continue;
                }
                FArcBuildingWorkforceFragment* Workforce = EntityManager.GetFragmentDataPtr<FArcBuildingWorkforceFragment>(Change.BuildingHandle);
                if (Workforce && Workforce->Slots.IsValidIndex(Change.SlotIndex))
                {
                    Workforce->Slots[Change.SlotIndex].DesiredRecipe = Change.NewDesiredRecipe;
                }
            }

            // Phase 2: Workforce — assign workers to slots that have active recipes
            ArcGovernor::AssignWorkers(GovCtx, AllNPCChanges, AllSlotChanges);
            ArcGovernor::AssignGatherers(GovCtx, AllNPCChanges);
            ArcGovernor::RebalanceTransporters(GovCtx, AllNPCChanges, AllWorkforceUpdates);

            // Phase 3: Trade (throttled — expensive knowledge spatial query)
            if (WorldTime >= Governor.NextTradeEvalTime)
            {
               // ArcGovernor::EstablishTradeRoutes(GovCtx, Workforce, *KnowledgeSub, AllNPCChanges);
                Governor.NextTradeEvalTime = WorldTime + Governor.GovernorConfig->TradeEvalInterval;
            }
        }
    });

    // Nothing to flush
    if (AllNPCChanges.Num() == 0 && AllSlotChanges.Num() == 0 && AllWorkforceUpdates.Num() == 0)
    {
        return;
    }

    // Single deferred command for all settlements
    Context.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::None>>(
        [NPCChanges = MoveTemp(AllNPCChanges),
         SlotChanges = MoveTemp(AllSlotChanges),
         WorkforceUpdates = MoveTemp(AllWorkforceUpdates),
         SignalSub, SettlementSub](FMassEntityManager& Mgr)
        {
            TArray<FMassEntityHandle> NPCsToSignal;
            NPCsToSignal.Reserve(NPCChanges.Num());

            TArray<FMassEntityHandle> SettlementsWithNewTransporters;

            // Write NPC fragment changes
            for (const ArcGovernor::FArcGovernorPendingNPCChange& Change : NPCChanges)
            {
                if (!Mgr.IsEntityValid(Change.NPCHandle))
                {
                    continue;
                }

                FArcEconomyNPCFragment* NPC = Mgr.GetFragmentDataPtr<FArcEconomyNPCFragment>(Change.NPCHandle);
                if (NPC)
                {
                    NPC->Role = Change.NewRole;
                }

                if (Change.NewRole == EArcEconomyNPCRole::Worker)
                {
                    FArcWorkerFragment* Worker = Mgr.GetFragmentDataPtr<FArcWorkerFragment>(Change.NPCHandle);
                    if (Worker)
                    {
                        Worker->AssignedBuildingHandle = Change.AssignedBuildingHandle;
                        Worker->bBackpressured = Change.bBackpressured;
                    }
                }
                else if (Change.NewRole == EArcEconomyNPCRole::Transporter)
                {
                    FArcTransporterFragment* Transporter = Mgr.GetFragmentDataPtr<FArcTransporterFragment>(Change.NPCHandle);
                    if (Transporter)
                    {
                        Transporter->TaskState = Change.TransporterState;
                    }

                    if (NPC && NPC->SettlementHandle.IsValid())
                    {
                        SettlementsWithNewTransporters.AddUnique(NPC->SettlementHandle);
                    }
                }
                else if (Change.NewRole == EArcEconomyNPCRole::Gatherer)
                {
                    FArcGathererFragment* Gatherer = Mgr.GetFragmentDataPtr<FArcGathererFragment>(Change.NPCHandle);
                    if (Gatherer)
                    {
                        Gatherer->AssignedBuildingHandle = Change.AssignedBuildingHandle;
                        Gatherer->TargetResourceHandle = Change.GathererTargetResource;
                        Gatherer->bCarryingResource = false;
                    }
                }
                else if (Change.NewRole == EArcEconomyNPCRole::Idle)
                {
                    FArcWorkerFragment* Worker = Mgr.GetFragmentDataPtr<FArcWorkerFragment>(Change.NPCHandle);
                    if (Worker)
                    {
                        Worker->AssignedBuildingHandle = FMassEntityHandle();
                    }
                    FArcGathererFragment* Gatherer = Mgr.GetFragmentDataPtr<FArcGathererFragment>(Change.NPCHandle);
                    if (Gatherer)
                    {
                        Gatherer->AssignedBuildingHandle = FMassEntityHandle();
                        Gatherer->TargetResourceHandle = FMassEntityHandle();
                        Gatherer->bCarryingResource = false;
                    }
                }

                if (FArcEconomyNPCDelegates* NPCDelegates = SettlementSub->FindNPCDelegates(Change.NPCHandle))
                {
                    NPCDelegates->OnRoleChanged.Broadcast();
                }

                NPCsToSignal.Add(Change.NPCHandle);
            }

            // Batch-signal all changed NPCs to re-evaluate their StateTree
            if (NPCsToSignal.Num() > 0)
            {
                SignalSub->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, NPCsToSignal);
            }

            // Signal settlements that have new idle transporters
            if (SettlementsWithNewTransporters.Num() > 0)
            {
                SignalSub->SignalEntities(ArcEconomy::Signals::TransporterIdle, SettlementsWithNewTransporters);
            }

            // Write slot changes
            for (const ArcGovernor::FArcGovernorPendingSlotChange& Change : SlotChanges)
            {
                if (!Mgr.IsEntityValid(Change.BuildingHandle))
                {
                    continue;
                }

                FArcBuildingWorkforceFragment* Workforce = Mgr.GetFragmentDataPtr<FArcBuildingWorkforceFragment>(Change.BuildingHandle);
                if (Workforce && Workforce->Slots.IsValidIndex(Change.SlotIndex))
                {
                    Workforce->Slots[Change.SlotIndex].DesiredRecipe = Change.NewDesiredRecipe;
                }
            }

            // Write workforce counts
            for (const ArcGovernor::FArcGovernorPendingWorkforceUpdate& WorkforceUpdate : WorkforceUpdates)
            {
                if (!Mgr.IsEntityValid(WorkforceUpdate.SettlementHandle))
                {
                    continue;
                }

                FArcSettlementWorkforceFragment* WF = Mgr.GetFragmentDataPtr<FArcSettlementWorkforceFragment>(WorkforceUpdate.SettlementHandle);
                if (WF)
                {
                    WF->WorkerCount = WorkforceUpdate.WorkerCount;
                    WF->TransporterCount = WorkforceUpdate.TransporterCount;
                    WF->GathererCount = WorkforceUpdate.GathererCount;
                    WF->CaravanCount = WorkforceUpdate.CaravanCount;
                    WF->IdleCount = WorkforceUpdate.IdleCount;
                }
            }
        });
}
