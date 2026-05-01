// Copyright Lukasz Baran. All Rights Reserved.

#include "ML/ArcStrategyTrainingSubsystem.h"
#include "ML/ArcStrategyCurriculumAsset.h"
#include "ML/ArcArchetypeRewardWeights.h"
#include "ML/ArcStrategyAgentProxy.h"
#include "ML/ArcStrategyTrainingCoordinator.h"

#if WITH_EDITOR
#include "LearningAgentsManager.h"
#include "LearningAgentsPolicy.h"
#include "LearningAgentsCritic.h"
#include "LearningAgentsPPOTrainer.h"
#include "LearningAgentsTrainer.h"
#include "LearningAgentsCommunicator.h"
#include "LearningAgentsNeuralNetwork.h"
#include "ML/ArcSettlementStrategyInteractor.h"
#include "ML/ArcFactionStrategyInteractor.h"
#include "ML/ArcSettlementTrainingEnvironment.h"
#include "ML/ArcFactionTrainingEnvironment.h"
#endif

#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "Mass/ArcEconomyFragments.h"
#include "Faction/ArcFactionFragments.h"
#include "Strategy/ArcPopulationFragment.h"
#include "Strategy/ArcSettlementStrategySubsystem.h"
#include "Strategy/ArcFactionStrategySubsystem.h"
#include "Strategy/ArcStrategicState.h"
#include "StructUtils/InstancedStruct.h"

bool UArcStrategyTrainingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer))
    {
        return false;
    }
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsEditorWorld();
}

void UArcStrategyTrainingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    SettlementRewardHistory.Reserve(MaxRewardHistory);
    FactionRewardHistory.Reserve(MaxRewardHistory);
}

void UArcStrategyTrainingSubsystem::Deinitialize()
{
    if (bIsTraining)
    {
        StopTraining();
    }
    Super::Deinitialize();
}

void UArcStrategyTrainingSubsystem::Tick(float DeltaTime)
{
    if (!bIsTraining)
    {
        return;
    }

#if WITH_EDITOR
    // Check for trainer failure
    if ((SettlementTrainer && SettlementTrainer->HasTrainingFailed()) ||
        (FactionTrainer && FactionTrainer->HasTrainingFailed()))
    {
        UE_LOG(LogTemp, Error, TEXT("UArcStrategyTrainingSubsystem: Trainer failed. Stopping."));
        StopTraining();
        return;
    }

    // After first tick: gather rewards/completions, process experience
    if (!bFirstTick)
    {
        if (SettlementEnvironment)
        {
            SettlementEnvironment->GatherRewards();
            SettlementEnvironment->GatherCompletions();
        }
        if (SettlementTrainer)
        {
            SettlementTrainer->ProcessExperience(false);
        }

        if (FactionEnvironment)
        {
            FactionEnvironment->GatherRewards();
            FactionEnvironment->GatherCompletions();
        }
        if (FactionTrainer)
        {
            FactionTrainer->ProcessExperience(false);
        }
    }
    bFirstTick = false;

    // Compute strategic state from entity fragments -> write to proxies
    UpdateProxyStates();

    // Gather observations -> inference -> perform actions
    if (SettlementInteractor)
    {
        SettlementInteractor->GatherObservations();
    }
    if (SettlementPolicy)
    {
        SettlementPolicy->RunInference();
    }
    if (SettlementInteractor)
    {
        SettlementInteractor->PerformActions();
    }
    SimulateSettlementActions();

    if (FactionInteractor)
    {
        FactionInteractor->GatherObservations();
    }
    if (FactionPolicy)
    {
        FactionPolicy->RunInference();
    }
    if (FactionInteractor)
    {
        FactionInteractor->PerformActions();
    }
    SimulateFactionActions();

    ++CurrentEpisodeStep;
    RecordTelemetry();

    // Log progress every 50 steps
    if (CurrentEpisodeStep % 50 == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Training: Stage %d | Episode %d | Step %d | SettlementReward=%.3f FactionReward=%.3f"),
            CurrentStageIndex, TotalEpisodes, CurrentEpisodeStep,
            AccumulatedSettlementReward, AccumulatedFactionReward);
    }

    CheckCurriculumAdvancement();
#endif
}

TStatId UArcStrategyTrainingSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UArcStrategyTrainingSubsystem, STATGROUP_Tickables);
}

void UArcStrategyTrainingSubsystem::StartTraining(
    UArcStrategyCurriculumAsset* Curriculum, UArcArchetypeRewardWeights* RewardWeights)
{
    if (!Curriculum || Curriculum->Stages.IsEmpty() || !RewardWeights)
    {
        UE_LOG(LogTemp, Warning, TEXT("UArcStrategyTrainingSubsystem::StartTraining — invalid parameters."));
        return;
    }

    if (bIsTraining)
    {
        StopTraining();
    }

    ActiveCurriculum = Curriculum;
    ActiveRewardWeights = RewardWeights;
    CurrentStageIndex = 0;
    TotalEpisodes = 0;
    SettlementRewardHistory.Empty();
    FactionRewardHistory.Empty();
    bIsTraining = true;
    bFirstTick = true;

    UMassEntitySubsystem* MassEntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
    if (MassEntitySubsystem)
    {
        CachedEntityManager = &MassEntitySubsystem->GetMutableEntityManager();
    }

    SetupWorld(Curriculum->Stages[0]);
    SetupLearningAgents();

    UE_LOG(LogTemp, Log, TEXT("UArcStrategyTrainingSubsystem: Training started. Stages=%d"), Curriculum->Stages.Num());
}

void UArcStrategyTrainingSubsystem::StopTraining()
{
#if WITH_EDITOR
    if (SettlementTrainer && SettlementTrainer->IsTraining())
    {
        SettlementTrainer->EndTraining();
    }
    if (FactionTrainer && FactionTrainer->IsTraining())
    {
        FactionTrainer->EndTraining();
    }
#endif

    SavePoliciesNow();
    TeardownLearningAgents();
    TeardownWorld();

    bIsTraining = false;
    CachedEntityManager = nullptr;
    UE_LOG(LogTemp, Log, TEXT("UArcStrategyTrainingSubsystem: Training stopped. TotalEpisodes=%d"), TotalEpisodes);
}

void UArcStrategyTrainingSubsystem::SavePoliciesNow()
{
#if WITH_EDITOR
    if (SettlementPolicy)
    {
        ULearningAgentsNeuralNetwork* Network = SettlementPolicy->GetPolicyNetworkAsset();
        if (Network)
        {
            const FString Path = SavePathPrefix + TEXT("SettlementPolicy");
            Network->SaveNetworkToSnapshot(FFilePath{Path});
            UE_LOG(LogTemp, Log, TEXT("UArcStrategyTrainingSubsystem: Saved settlement policy to %s"), *Path);
        }
    }
    if (FactionPolicy)
    {
        ULearningAgentsNeuralNetwork* Network = FactionPolicy->GetPolicyNetworkAsset();
        if (Network)
        {
            const FString Path = SavePathPrefix + TEXT("FactionPolicy");
            Network->SaveNetworkToSnapshot(FFilePath{Path});
            UE_LOG(LogTemp, Log, TEXT("UArcStrategyTrainingSubsystem: Saved faction policy to %s"), *Path);
        }
    }
#endif
}

void UArcStrategyTrainingSubsystem::SetupWorld(const FArcCurriculumStage& Stage)
{
    if (!CachedEntityManager)
    {
        return;
    }

    // Compute action masks from curriculum stage
    SettlementActionMask = 0xFFFFFFFF;
    FactionActionMask = 0xFFFFFFFF;
    if (!Stage.bEnableMilitary)
    {
        // Mask out: Recruit (index 1), Defend (index 5) for settlement
        SettlementActionMask &= ~(1u << 1); // Recruit
        SettlementActionMask &= ~(1u << 5); // Defend
        // Mask out: LaunchAttack (index 4), DeclareWar (index 0) for faction
        FactionActionMask &= ~(1u << 4); // LaunchAttack
        FactionActionMask &= ~(1u << 0); // DeclareWar
    }
    if (!Stage.bEnableDiplomacy)
    {
        FactionActionMask &= ~(1u << 0); // DeclareWar
        FactionActionMask &= ~(1u << 1); // MakePeace
        FactionActionMask &= ~(1u << 2); // ProposeAlliance
    }

    // Spawn faction entities evenly spaced in a circle
    const float AngleStep = (Stage.NumFactions > 0) ? (2.0f * UE_PI / static_cast<float>(Stage.NumFactions)) : 0.0f;

    for (int32 FactionIdx = 0; FactionIdx < Stage.NumFactions; ++FactionIdx)
    {
        const float Angle = AngleStep * static_cast<float>(FactionIdx);
        const FVector FactionPos(
            FMath::Cos(Angle) * Stage.WorldRadius * 0.5f,
            FMath::Sin(Angle) * Stage.WorldRadius * 0.5f,
            0.0f);

        // Pick archetype
        EArcFactionArchetype Archetype = EArcFactionArchetype::Economic;
        if (!Stage.AllowedArchetypes.IsEmpty())
        {
            Archetype = Stage.AllowedArchetypes[FMath::RandRange(0, Stage.AllowedArchetypes.Num() - 1)];
        }

        // Create faction entity with all required fragments
        TArray<FInstancedStruct> FactionFragments;
        FactionFragments.Emplace(FInstancedStruct::Make<FArcFactionFragment>());
        FactionFragments.Emplace(FInstancedStruct::Make<FArcFactionDiplomacyFragment>());
        FactionFragments.Emplace(FInstancedStruct::Make<FArcFactionSettlementsFragment>());
        const FMassEntityHandle FactionHandle = CachedEntityManager->CreateEntity(FactionFragments);

        // Initialize faction data
        FArcFactionFragment& FactionFrag = CachedEntityManager->GetFragmentDataChecked<FArcFactionFragment>(FactionHandle);
        FactionFrag.Archetype = Archetype;
        FactionFrag.FactionName = *FString::Printf(TEXT("Faction_%d"), FactionIdx);

        FArcFactionSettlementsFragment& SettlementsFrag = CachedEntityManager->GetFragmentDataChecked<FArcFactionSettlementsFragment>(FactionHandle);

        SpawnedFactions.Add(FactionHandle);

        // Set neutral diplomacy to all other existing factions
        FArcFactionDiplomacyFragment& DiplomacyFrag = CachedEntityManager->GetFragmentDataChecked<FArcFactionDiplomacyFragment>(FactionHandle);
        for (const FMassEntityHandle& OtherFaction : SpawnedFactions)
        {
            if (OtherFaction != FactionHandle)
            {
                DiplomacyFrag.Stances.Add(OtherFaction, EArcDiplomaticStance::Neutral);
                // Also add reverse
                FArcFactionDiplomacyFragment& OtherDip = CachedEntityManager->GetFragmentDataChecked<FArcFactionDiplomacyFragment>(OtherFaction);
                OtherDip.Stances.Add(FactionHandle, EArcDiplomaticStance::Neutral);
            }
        }

        // Register faction
        UArcFactionStrategySubsystem* FactionStrategySub = GetWorld()->GetSubsystem<UArcFactionStrategySubsystem>();
        if (FactionStrategySub)
        {
            FactionStrategySub->RegisterFaction(FactionHandle);
        }

        // Spawn settlements for this faction
        const int32 NumSettlements = FMath::RandRange(Stage.MinSettlementsPerFaction, Stage.MaxSettlementsPerFaction);
        for (int32 SettlementIdx = 0; SettlementIdx < NumSettlements; ++SettlementIdx)
        {
            const FVector Offset(
                FMath::FRandRange(-1000.0f, 1000.0f),
                FMath::FRandRange(-1000.0f, 1000.0f),
                0.0f);
            const FVector SettlementPos = FactionPos + Offset;

            TArray<FInstancedStruct> SettlementFragments;
            SettlementFragments.Emplace(FInstancedStruct::Make<FArcSettlementFragment>());
            SettlementFragments.Emplace(FInstancedStruct::Make<FArcSettlementFactionFragment>());
            SettlementFragments.Emplace(FInstancedStruct::Make<FArcSettlementMarketFragment>());
            SettlementFragments.Emplace(FInstancedStruct::Make<FArcSettlementWorkforceFragment>());
            SettlementFragments.Emplace(FInstancedStruct::Make<FArcPopulationFragment>());
            const FMassEntityHandle SettlementHandle = CachedEntityManager->CreateEntity(SettlementFragments);

            FArcSettlementFragment& Settlement = CachedEntityManager->GetFragmentDataChecked<FArcSettlementFragment>(SettlementHandle);
            Settlement.bPlayerOwned = false;
            Settlement.SettlementName = *FString::Printf(TEXT("Settlement_%d_%d"), FactionIdx, SettlementIdx);

            FArcSettlementFactionFragment& FactionLink = CachedEntityManager->GetFragmentDataChecked<FArcSettlementFactionFragment>(SettlementHandle);
            FactionLink.OwningFaction = FactionHandle;

            FArcSettlementMarketFragment& Market = CachedEntityManager->GetFragmentDataChecked<FArcSettlementMarketFragment>(SettlementHandle);
            Market.CurrentTotalStorage = FMath::RandRange(
                static_cast<int32>(Stage.StartingResourceRange.X),
                static_cast<int32>(Stage.StartingResourceRange.Y));

            FArcSettlementWorkforceFragment& Workforce = CachedEntityManager->GetFragmentDataChecked<FArcSettlementWorkforceFragment>(SettlementHandle);
            Workforce.WorkerCount = FMath::RandRange(5, 15);

            FArcPopulationFragment& Pop = CachedEntityManager->GetFragmentDataChecked<FArcPopulationFragment>(SettlementHandle);
            Pop.Population = FMath::RandRange(
                static_cast<int32>(Stage.StartingPopulationRange.X),
                static_cast<int32>(Stage.StartingPopulationRange.Y));

            // Back-link to faction
            SettlementsFrag.OwnedSettlements.Add(SettlementHandle);
            SpawnedSettlements.Add(SettlementHandle);

            // Register with settlement strategy subsystem
            UArcSettlementStrategySubsystem* SettlementStrategySub = GetWorld()->GetSubsystem<UArcSettlementStrategySubsystem>();
            if (SettlementStrategySub)
            {
                SettlementStrategySub->RegisterSettlement(SettlementHandle);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("UArcStrategyTrainingSubsystem::SetupWorld — Factions=%d Settlements=%d"),
        SpawnedFactions.Num(), SpawnedSettlements.Num());
}

void UArcStrategyTrainingSubsystem::TeardownWorld()
{
    UArcSettlementStrategySubsystem* SettlementStrategySub = GetWorld()->GetSubsystem<UArcSettlementStrategySubsystem>();
    UArcFactionStrategySubsystem* FactionStrategySub = GetWorld()->GetSubsystem<UArcFactionStrategySubsystem>();

    for (const FMassEntityHandle& Handle : SpawnedSettlements)
    {
        if (SettlementStrategySub)
        {
            SettlementStrategySub->UnregisterSettlement(Handle);
        }
        if (CachedEntityManager && CachedEntityManager->IsEntityValid(Handle))
        {
            CachedEntityManager->DestroyEntity(Handle);
        }
    }

    for (const FMassEntityHandle& Handle : SpawnedFactions)
    {
        if (FactionStrategySub)
        {
            FactionStrategySub->UnregisterFaction(Handle);
        }
        if (CachedEntityManager && CachedEntityManager->IsEntityValid(Handle))
        {
            CachedEntityManager->DestroyEntity(Handle);
        }
    }

    SpawnedSettlements.Empty();
    SpawnedFactions.Empty();
}

void UArcStrategyTrainingSubsystem::SetupLearningAgents()
{
#if WITH_EDITOR
    // Spawn coordinator actor
    Coordinator = GetWorld()->SpawnActor<AArcStrategyTrainingCoordinator>();
    if (!Coordinator)
    {
        UE_LOG(LogTemp, Error, TEXT("UArcStrategyTrainingSubsystem: Failed to spawn coordinator."));
        return;
    }

    ULearningAgentsManager* SettlementMgr = Coordinator->SettlementManager;
    ULearningAgentsManager* FactionMgr = Coordinator->FactionManager;

    // Pre-allocate agent slots with headroom for FoundSettlement spawns
    SettlementMgr->SetMaxAgentNum(FMath::Max(1, SpawnedSettlements.Num() * 3));
    FactionMgr->SetMaxAgentNum(FMath::Max(1, SpawnedFactions.Num()));

    // ----- Settlement pipeline -----
    ULearningAgentsInteractor* SettlementInteractorBase = ULearningAgentsInteractor::MakeInteractor(
        SettlementMgr, UArcSettlementStrategyInteractor::StaticClass(), TEXT("SettlementInteractor"));
    SettlementInteractor = Cast<UArcSettlementStrategyInteractor>(SettlementInteractorBase);

    // Create and register settlement proxies
    for (const FMassEntityHandle& Handle : SpawnedSettlements)
    {
        UArcStrategyAgentProxy* Proxy = NewObject<UArcStrategyAgentProxy>(this);
        Proxy->EntityHandle = Handle;
        Proxy->AgentId = SettlementMgr->AddAgent(Proxy);
        Proxy->SettlementState.Security = 50.0f;
        Proxy->SettlementState.Military = 30.0f;
        SettlementProxies.Add(Proxy);
    }

    ULearningAgentsTrainingEnvironment* SettlementEnvBase = ULearningAgentsTrainingEnvironment::MakeTrainingEnvironment(
        SettlementMgr, UArcSettlementTrainingEnvironment::StaticClass(), TEXT("SettlementEnvironment"));
    SettlementEnvironment = Cast<UArcSettlementTrainingEnvironment>(SettlementEnvBase);
    SettlementEnvironment->SetOwningSubsystem(this);
    SettlementEnvironment->SetRewardWeights(ActiveRewardWeights);
    SettlementEnvironment->SetEntityManager(CachedEntityManager);

    FLearningAgentsPolicySettings PolicySettings;
    PolicySettings.HiddenLayerNum = 2;
    PolicySettings.HiddenLayerSize = 128;

    SettlementPolicy = ULearningAgentsPolicy::MakePolicy(
        SettlementMgr, SettlementInteractorBase, ULearningAgentsPolicy::StaticClass(),
        TEXT("SettlementPolicy"),
        nullptr, nullptr, nullptr,
        true, true, true,
        PolicySettings);

    ULearningAgentsPolicy* SettlementPolicyPtr = SettlementPolicy.Get();
    SettlementCritic = ULearningAgentsCritic::MakeCritic(
        SettlementMgr, SettlementInteractorBase, SettlementPolicyPtr,
        ULearningAgentsCritic::StaticClass(), TEXT("SettlementCritic"));

    FLearningAgentsTrainerProcessSettings ProcessSettings;
    ProcessSettings.TrainerFileName = TEXT("learning_core.train_ppo");

    SettlementCommunicator = ULearningAgentsCommunicatorLibrary::MakeSharedMemoryTrainingProcess(ProcessSettings);

    FLearningAgentsPPOTrainerSettings TrainerSettings;
    TrainerSettings.MaxEpisodeStepNum = 512;

    ULearningAgentsTrainingEnvironment* SettlementEnvPtr = SettlementEnvironment.Get();
    ULearningAgentsCritic* SettlementCriticPtr = SettlementCritic.Get();
    SettlementTrainer = ULearningAgentsPPOTrainer::MakePPOTrainer(
        SettlementMgr, SettlementInteractorBase, SettlementEnvPtr,
        SettlementPolicyPtr, SettlementCriticPtr,
        SettlementCommunicator,
        ULearningAgentsPPOTrainer::StaticClass(), TEXT("SettlementTrainer"), TrainerSettings);

    // ----- Faction pipeline -----
    ULearningAgentsInteractor* FactionInteractorBase = ULearningAgentsInteractor::MakeInteractor(
        FactionMgr, UArcFactionStrategyInteractor::StaticClass(), TEXT("FactionInteractor"));
    FactionInteractor = Cast<UArcFactionStrategyInteractor>(FactionInteractorBase);

    for (const FMassEntityHandle& Handle : SpawnedFactions)
    {
        UArcStrategyAgentProxy* Proxy = NewObject<UArcStrategyAgentProxy>(this);
        Proxy->EntityHandle = Handle;
        Proxy->AgentId = FactionMgr->AddAgent(Proxy);
        FactionProxies.Add(Proxy);
    }

    ULearningAgentsTrainingEnvironment* FactionEnvBase = ULearningAgentsTrainingEnvironment::MakeTrainingEnvironment(
        FactionMgr, UArcFactionTrainingEnvironment::StaticClass(), TEXT("FactionEnvironment"));
    FactionEnvironment = Cast<UArcFactionTrainingEnvironment>(FactionEnvBase);
    FactionEnvironment->SetOwningSubsystem(this);
    FactionEnvironment->SetRewardWeights(ActiveRewardWeights);
    FactionEnvironment->SetEntityManager(CachedEntityManager);

    FactionPolicy = ULearningAgentsPolicy::MakePolicy(
        FactionMgr, FactionInteractorBase, ULearningAgentsPolicy::StaticClass(),
        TEXT("FactionPolicy"),
        nullptr, nullptr, nullptr,
        true, true, true,
        PolicySettings);

    ULearningAgentsPolicy* FactionPolicyPtr = FactionPolicy.Get();
    FactionCritic = ULearningAgentsCritic::MakeCritic(
        FactionMgr, FactionInteractorBase, FactionPolicyPtr,
        ULearningAgentsCritic::StaticClass(), TEXT("FactionCritic"));

    FactionCommunicator = ULearningAgentsCommunicatorLibrary::MakeSharedMemoryTrainingProcess(ProcessSettings);

    ULearningAgentsTrainingEnvironment* FactionEnvPtr = FactionEnvironment.Get();
    ULearningAgentsCritic* FactionCriticPtr = FactionCritic.Get();
    FactionTrainer = ULearningAgentsPPOTrainer::MakePPOTrainer(
        FactionMgr, FactionInteractorBase, FactionEnvPtr,
        FactionPolicyPtr, FactionCriticPtr,
        FactionCommunicator,
        ULearningAgentsPPOTrainer::StaticClass(), TEXT("FactionTrainer"), TrainerSettings);

    // Begin training on both
    FLearningAgentsPPOTrainingSettings TrainingSettings;
    TrainingSettings.LearningRatePolicy = 0.0001f;
    TrainingSettings.LearningRateCritic = 0.001f;
    TrainingSettings.PolicyWindowSize = 16;

    FLearningAgentsTrainingGameSettings GameSettings;
    GameSettings.bUseFixedTimeStep = true;
    GameSettings.FixedTimeStepFrequency = 60.0f;
    GameSettings.bDisableVSync = true;
    GameSettings.bDisableMaxFPS = true;

    SettlementTrainer->BeginTraining(TrainingSettings, GameSettings, true);
    FactionTrainer->BeginTraining(TrainingSettings, GameSettings, true);

    UE_LOG(LogTemp, Log, TEXT("UArcStrategyTrainingSubsystem::SetupLearningAgents — pipelines ready. "
        "SettlementAgents=%d FactionAgents=%d"), SettlementProxies.Num(), FactionProxies.Num());
#endif
}

void UArcStrategyTrainingSubsystem::TeardownLearningAgents()
{
    SettlementProxies.Empty();
    FactionProxies.Empty();

    SettlementInteractor = nullptr;
    SettlementEnvironment = nullptr;
    SettlementPolicy = nullptr;
    SettlementCritic = nullptr;
    SettlementTrainer = nullptr;

    FactionInteractor = nullptr;
    FactionEnvironment = nullptr;
    FactionPolicy = nullptr;
    FactionCritic = nullptr;
    FactionTrainer = nullptr;

    if (Coordinator)
    {
        Coordinator->Destroy();
        Coordinator = nullptr;
    }
}

void UArcStrategyTrainingSubsystem::CheckCurriculumAdvancement()
{
    if (!ActiveCurriculum || CurrentStageIndex >= ActiveCurriculum->Stages.Num())
    {
        return;
    }

    const FArcCurriculumStage& Stage = ActiveCurriculum->Stages[CurrentStageIndex];

    if (CurrentEpisodeStep >= Stage.MaxEpisodeSteps)
    {
        // Episode complete
        const float SettlementMean = SettlementProxies.Num() > 0
            ? AccumulatedSettlementReward / static_cast<float>(SettlementProxies.Num())
            : 0.0f;
        const float FactionMean = FactionProxies.Num() > 0
            ? AccumulatedFactionReward / static_cast<float>(FactionProxies.Num())
            : 0.0f;
        const float CombinedMean = (SettlementMean + FactionMean) * 0.5f;

        // Record to history
        if (SettlementRewardHistory.Num() < MaxRewardHistory)
        {
            SettlementRewardHistory.Add(SettlementMean);
            FactionRewardHistory.Add(FactionMean);
        }
        else
        {
            const int32 Idx = TotalEpisodes % MaxRewardHistory;
            SettlementRewardHistory[Idx] = SettlementMean;
            FactionRewardHistory[Idx] = FactionMean;
        }

        ++TotalEpisodes;

        UE_LOG(LogTemp, Log, TEXT("UArcStrategyTrainingSubsystem: Episode %d complete. "
            "Stage=%d SettlementMean=%.3f FactionMean=%.3f"),
            TotalEpisodes, CurrentStageIndex, SettlementMean, FactionMean);

        if (CombinedMean >= Stage.AdvanceRewardThreshold)
        {
            ++ConsecutiveGoodEpisodes;
        }
        else
        {
            ConsecutiveGoodEpisodes = 0;
        }

        if (ConsecutiveGoodEpisodes >= Stage.AdvanceEpisodeCount)
        {
            UE_LOG(LogTemp, Log, TEXT("UArcStrategyTrainingSubsystem: Advancing from stage %d to %d."),
                CurrentStageIndex, CurrentStageIndex + 1);

#if WITH_EDITOR
            if (SettlementTrainer) { SettlementTrainer->EndTraining(); }
            if (FactionTrainer) { FactionTrainer->EndTraining(); }
#endif
            TeardownLearningAgents();
            TeardownWorld();

            ++CurrentStageIndex;
            ConsecutiveGoodEpisodes = 0;

            if (CurrentStageIndex < ActiveCurriculum->Stages.Num())
            {
                SetupWorld(ActiveCurriculum->Stages[CurrentStageIndex]);
                SetupLearningAgents();
                bFirstTick = true;
            }
            else
            {
                StopTraining();
                UE_LOG(LogTemp, Log, TEXT("UArcStrategyTrainingSubsystem: All stages complete."));
            }
            return;
        }

        // Reset for next episode (within same stage)
        CurrentEpisodeStep = 0;
        AccumulatedSettlementReward = 0.0f;
        AccumulatedFactionReward = 0.0f;
    }
}

void UArcStrategyTrainingSubsystem::UpdateProxyStates()
{
    if (!CachedEntityManager)
    {
        return;
    }

    // Settlement proxies: compute simplified state from fragments
    for (UArcStrategyAgentProxy* Proxy : SettlementProxies)
    {
        if (!Proxy || !CachedEntityManager->IsEntityValid(Proxy->EntityHandle))
        {
            continue;
        }

        const FMassEntityView View(*CachedEntityManager, Proxy->EntityHandle);

        const FArcSettlementMarketFragment* Market = View.GetFragmentDataPtr<FArcSettlementMarketFragment>();
        const FArcSettlementWorkforceFragment* Workforce = View.GetFragmentDataPtr<FArcSettlementWorkforceFragment>();
        const FArcPopulationFragment* Pop = View.GetFragmentDataPtr<FArcPopulationFragment>();

        FArcSettlementStrategicState& State = Proxy->SettlementState;

        // Simplified state computation for training (no KnowledgeSub/SettlementSub needed)
        if (Market)
        {
            const float Utilization = Market->TotalStorageCap > 0
                ? static_cast<float>(Market->CurrentTotalStorage) / static_cast<float>(Market->TotalStorageCap)
                : 0.0f;
            State.Prosperity = FMath::Clamp(Utilization * 100.0f, 0.0f, 100.0f);
        }

        if (Workforce)
        {
            const int32 TotalWorkers = Workforce->WorkerCount + Workforce->TransporterCount + Workforce->GathererCount;
            const float IdleRatio = TotalWorkers > 0
                ? static_cast<float>(Workforce->IdleCount) / static_cast<float>(TotalWorkers + Workforce->IdleCount)
                : 1.0f;
            State.Labor = FMath::Clamp(IdleRatio * 100.0f, 0.0f, 100.0f);
        }

        if (Pop)
        {
            const float GrowthRatio = Pop->MaxPopulation > 0
                ? static_cast<float>(Pop->Population) / static_cast<float>(Pop->MaxPopulation)
                : 0.0f;
            State.Growth = FMath::Clamp(GrowthRatio * 100.0f, 0.0f, 100.0f);
        }

        // Decay Security and Military toward baseline each tick — Defend action counteracts this
        State.Security = FMath::Clamp(State.Security - 0.5f, 0.0f, 100.0f);
        State.Military = FMath::Clamp(State.Military - 0.3f, 0.0f, 100.0f);
    }

    // Faction proxies: compute from owned settlement states
    for (UArcStrategyAgentProxy* Proxy : FactionProxies)
    {
        if (!Proxy || !CachedEntityManager->IsEntityValid(Proxy->EntityHandle))
        {
            continue;
        }

        const FMassEntityView FactionView(*CachedEntityManager, Proxy->EntityHandle);
        const FArcFactionSettlementsFragment* Settlements = FactionView.GetFragmentDataPtr<FArcFactionSettlementsFragment>();
        const FArcFactionDiplomacyFragment* Diplomacy = FactionView.GetFragmentDataPtr<FArcFactionDiplomacyFragment>();

        FArcFactionStrategicState& State = Proxy->FactionState;

        if (Settlements)
        {
            const int32 SettlementCount = Settlements->OwnedSettlements.Num();
            State.Dominance = FMath::Clamp(static_cast<float>(SettlementCount) * 20.0f, 0.0f, 100.0f);
            State.Expansion = FMath::Clamp(static_cast<float>(SettlementCount) * 15.0f, 0.0f, 100.0f);

            // Average prosperity from owned settlements
            float TotalProsperity = 0.0f;
            int32 ValidCount = 0;
            for (const FMassEntityHandle& SettlementHandle : Settlements->OwnedSettlements)
            {
                if (!CachedEntityManager->IsEntityValid(SettlementHandle))
                {
                    continue;
                }
                const FMassEntityView SettlementView(*CachedEntityManager, SettlementHandle);
                const FArcSettlementMarketFragment* Market = SettlementView.GetFragmentDataPtr<FArcSettlementMarketFragment>();
                if (Market && Market->TotalStorageCap > 0)
                {
                    const float Utilization = static_cast<float>(Market->CurrentTotalStorage) / static_cast<float>(Market->TotalStorageCap);
                    TotalProsperity += Utilization * 100.0f;
                    ++ValidCount;
                }
            }
            State.Wealth = ValidCount > 0 ? FMath::Clamp(TotalProsperity / static_cast<float>(ValidCount), 0.0f, 100.0f) : 30.0f;
        }

        // Stability from diplomacy — penalty per hostile stance
        int32 HostileCount = 0;
        if (Diplomacy)
        {
            for (const TPair<FMassEntityHandle, EArcDiplomaticStance>& Pair : Diplomacy->Stances)
            {
                if (Pair.Value == EArcDiplomaticStance::Hostile)
                {
                    ++HostileCount;
                }
            }
        }
        State.Stability = FMath::Clamp(80.0f - static_cast<float>(HostileCount) * 20.0f, 0.0f, 100.0f);
    }
}

void UArcStrategyTrainingSubsystem::RecordTelemetry()
{
    // Accumulate per-step rewards from proxies
    for (const UArcStrategyAgentProxy* Proxy : SettlementProxies)
    {
        if (Proxy)
        {
            const float DeltaP = (Proxy->SettlementState.Prosperity - Proxy->PreviousSettlementState.Prosperity) * 0.01f;
            const float DeltaS = (Proxy->SettlementState.Security - Proxy->PreviousSettlementState.Security) * 0.01f;
            ChannelTelemetry.SettlementEconomicMean += DeltaP;
            ChannelTelemetry.SettlementMilitaryMean += DeltaS;
            AccumulatedSettlementReward += (DeltaP + DeltaS) * 0.5f;
        }
    }
    for (const UArcStrategyAgentProxy* Proxy : FactionProxies)
    {
        if (Proxy)
        {
            const float DeltaW = (Proxy->FactionState.Wealth - Proxy->PreviousFactionState.Wealth) * 0.01f;
            const float DeltaD = (Proxy->FactionState.Dominance - Proxy->PreviousFactionState.Dominance) * 0.01f;
            const float DeltaSt = (Proxy->FactionState.Stability - Proxy->PreviousFactionState.Stability) * 0.01f;
            ChannelTelemetry.FactionEconomicMean += DeltaW;
            ChannelTelemetry.FactionMilitaryMean += DeltaD;
            ChannelTelemetry.FactionDiplomaticMean += DeltaSt;
            AccumulatedFactionReward += (DeltaW + DeltaD + DeltaSt) / 3.0f;
        }
    }
}

void UArcStrategyTrainingSubsystem::SimulateSettlementActions()
{
    if (!CachedEntityManager)
    {
        return;
    }

    for (UArcStrategyAgentProxy* Proxy : SettlementProxies)
    {
        if (!Proxy || !CachedEntityManager->IsEntityValid(Proxy->EntityHandle))
        {
            continue;
        }

        FMassEntityView View(*CachedEntityManager, Proxy->EntityHandle);

        FArcSettlementMarketFragment* Market = View.GetFragmentDataPtr<FArcSettlementMarketFragment>();
        FArcSettlementWorkforceFragment* Workforce = View.GetFragmentDataPtr<FArcSettlementWorkforceFragment>();
        FArcPopulationFragment* Pop = View.GetFragmentDataPtr<FArcPopulationFragment>();

        if (!Market || !Workforce || !Pop)
        {
            continue;
        }

        // --- Population drift ---
        // GrowthRate ~0.05 at Growth=50. Net growth = GrowthRate * Pop - 0.3 decay.
        // Pop=10: +0.5-0.3 = +0.2 -> floor = 0 (stable). Pop=20: +1.0-0.3 -> floor = 0.
        // Larger settlements grow; small ones are stable. Only truly neglected (Growth<10) settlements shrink.
        const int32 TotalWorkers = Workforce->WorkerCount + Workforce->TransporterCount + Workforce->GathererCount + Workforce->CaravanCount;
        const float GrowthRate = 0.1f * (Proxy->SettlementState.Growth * 0.01f);
        const int32 PopDelta = FMath::FloorToInt32(GrowthRate * static_cast<float>(Pop->Population) - 0.3f);
        Pop->Population += PopDelta;
        Pop->Population = FMath::Clamp(Pop->Population, 0, Pop->MaxPopulation);

        const int32 IdleFromPop = FMath::Max(0, Pop->Population / 20 - TotalWorkers / 5);
        Workforce->IdleCount += IdleFromPop;
        Workforce->IdleCount = FMath::Clamp(Workforce->IdleCount, 0, Pop->Population / 3);

        // --- Action processing ---
        const FArcStrategyDecision& Decision = Proxy->LastDecision;
        const float I = Decision.Intensity;
        const EArcSettlementAction Action = static_cast<EArcSettlementAction>(Decision.ActionIndex);

        switch (Action)
        {
            case EArcSettlementAction::Build:
            {
                Market->TotalStorageCap += FMath::FloorToInt32(20.0f * I);
                Workforce->IdleCount = FMath::Max(0, Workforce->IdleCount - 2);
                Workforce->WorkerCount += 2;
                break;
            }
            case EArcSettlementAction::Recruit:
            {
                const int32 Recruited = FMath::FloorToInt32(static_cast<float>(Workforce->IdleCount) * I);
                Workforce->WorkerCount += Recruited;
                Workforce->IdleCount = FMath::Max(0, Workforce->IdleCount - Recruited);
                break;
            }
            case EArcSettlementAction::Trade:
            {
                Market->CurrentTotalStorage += FMath::FloorToInt32(15.0f * I);
                if (FMath::FRand() < 0.2f)
                {
                    Market->CurrentTotalStorage -= 3;
                }
                break;
            }
            case EArcSettlementAction::RequestAid:
            {
                // Only grant aid if the owning faction has at least one Allied faction
                const FArcSettlementFactionFragment* FactionFrag = View.GetFragmentDataPtr<FArcSettlementFactionFragment>();
                if (FactionFrag && CachedEntityManager->IsEntityValid(FactionFrag->OwningFaction))
                {
                    const FMassEntityView FactionView(*CachedEntityManager, FactionFrag->OwningFaction);
                    const FArcFactionDiplomacyFragment* Diplomacy = FactionView.GetFragmentDataPtr<FArcFactionDiplomacyFragment>();
                    bool bHasAlly = false;
                    if (Diplomacy)
                    {
                        for (const TPair<FMassEntityHandle, EArcDiplomaticStance>& Pair : Diplomacy->Stances)
                        {
                            if (Pair.Value == EArcDiplomaticStance::Allied)
                            {
                                bHasAlly = true;
                                break;
                            }
                        }
                    }
                    if (bHasAlly)
                    {
                        Pop->Population += FMath::FloorToInt32(3.0f * I);
                        Pop->Population = FMath::Clamp(Pop->Population, 0, Pop->MaxPopulation);
                    }
                }
                break;
            }
            case EArcSettlementAction::Expand:
            {
                Market->TotalStorageCap += FMath::FloorToInt32(10.0f * I);
                Pop->Population -= FMath::FloorToInt32(2.0f * I);
                Pop->Population = FMath::Max(0, Pop->Population);
                break;
            }
            case EArcSettlementAction::Defend:
            {
                Workforce->WorkerCount = FMath::Max(0, Workforce->WorkerCount - 1);
                Proxy->SettlementState.Security += 10.0f * I;
                break;
            }
            case EArcSettlementAction::Scout:
            {
                Proxy->SettlementState.Prosperity += 3.0f * I;
                break;
            }
            default:
                break;
        }

        // --- Post-mutation clamps ---
        Market->CurrentTotalStorage = FMath::Clamp(Market->CurrentTotalStorage, 0, Market->TotalStorageCap);
        Market->TotalStorageCap = FMath::Clamp(Market->TotalStorageCap, 10, 2000);
        Workforce->WorkerCount = FMath::Clamp(Workforce->WorkerCount, 0, Pop->Population);
        Workforce->IdleCount = FMath::Clamp(Workforce->IdleCount, 0, Pop->Population / 3);
        Proxy->SettlementState.Security = FMath::Clamp(Proxy->SettlementState.Security, 0.0f, 100.0f);
        Proxy->SettlementState.Military = FMath::Clamp(Proxy->SettlementState.Military, 0.0f, 100.0f);
        Proxy->SettlementState.Prosperity = FMath::Clamp(Proxy->SettlementState.Prosperity, 0.0f, 100.0f);
    }
}

void UArcStrategyTrainingSubsystem::SimulateFactionActions()
{
    if (!CachedEntityManager)
    {
        return;
    }

    for (UArcStrategyAgentProxy* Proxy : FactionProxies)
    {
        if (!Proxy || !CachedEntityManager->IsEntityValid(Proxy->EntityHandle))
        {
            continue;
        }

        FMassEntityView OwnView(*CachedEntityManager, Proxy->EntityHandle);
        FArcFactionDiplomacyFragment* OwnDiplomacy = OwnView.GetFragmentDataPtr<FArcFactionDiplomacyFragment>();
        FArcFactionSettlementsFragment* OwnSettlements = OwnView.GetFragmentDataPtr<FArcFactionSettlementsFragment>();

        if (!OwnDiplomacy || !OwnSettlements)
        {
            continue;
        }

        const FArcStrategyDecision& Decision = Proxy->LastDecision;
        const float I = Decision.Intensity;
        const EArcFactionAction Action = static_cast<EArcFactionAction>(Decision.ActionIndex);

        // Resolve target faction
        const int32 ClampedTargetIdx = (Proxy->TargetFactionIndex != INDEX_NONE && SpawnedFactions.Num() > 0)
            ? FMath::Clamp(Proxy->TargetFactionIndex, 0, SpawnedFactions.Num() - 1)
            : INDEX_NONE;

        const bool bTargetValid = ClampedTargetIdx != INDEX_NONE
            && SpawnedFactions[ClampedTargetIdx] != Proxy->EntityHandle
            && CachedEntityManager->IsEntityValid(SpawnedFactions[ClampedTargetIdx]);

        const FMassEntityHandle TargetHandle = bTargetValid ? SpawnedFactions[ClampedTargetIdx] : FMassEntityHandle{};

        FArcFactionDiplomacyFragment* TargetDiplomacy = nullptr;
        UArcStrategyAgentProxy* TargetProxy = nullptr;

        if (bTargetValid)
        {
            FMassEntityView TargetView(*CachedEntityManager, TargetHandle);
            TargetDiplomacy = TargetView.GetFragmentDataPtr<FArcFactionDiplomacyFragment>();

            for (UArcStrategyAgentProxy* FP : FactionProxies)
            {
                if (FP && FP->EntityHandle == TargetHandle)
                {
                    TargetProxy = FP;
                    break;
                }
            }
        }

        const EArcDiplomaticStance CurrentStance = OwnDiplomacy->GetStanceToward(TargetHandle);

        switch (Action)
        {
            case EArcFactionAction::DeclareWar:
            {
                if (!bTargetValid)
                {
                    break;
                }

                OwnDiplomacy->Stances.Add(TargetHandle, EArcDiplomaticStance::Hostile);
                if (TargetDiplomacy)
                {
                    TargetDiplomacy->Stances.Add(Proxy->EntityHandle, EArcDiplomaticStance::Hostile);
                }

                Proxy->FactionState.Stability -= 10.0f;
                if (TargetProxy)
                {
                    TargetProxy->FactionState.Stability -= 15.0f;
                }

                break;
            }

            case EArcFactionAction::MakePeace:
            {
                if (!bTargetValid || CurrentStance != EArcDiplomaticStance::Hostile)
                {
                    break;
                }

                OwnDiplomacy->Stances.Add(TargetHandle, EArcDiplomaticStance::Neutral);
                if (TargetDiplomacy)
                {
                    TargetDiplomacy->Stances.Add(Proxy->EntityHandle, EArcDiplomaticStance::Neutral);
                }

                Proxy->FactionState.Stability += 10.0f;
                if (TargetProxy)
                {
                    TargetProxy->FactionState.Stability += 10.0f;
                }

                break;
            }

            case EArcFactionAction::ProposeAlliance:
            {
                if (!bTargetValid || CurrentStance == EArcDiplomaticStance::Hostile)
                {
                    break;
                }

                // 50% rejection if target Dominance > own
                const bool bRejected = TargetProxy
                    && TargetProxy->FactionState.Dominance > Proxy->FactionState.Dominance
                    && FMath::FRand() < 0.5f;

                if (bRejected)
                {
                    break;
                }

                OwnDiplomacy->Stances.Add(TargetHandle, EArcDiplomaticStance::Allied);
                if (TargetDiplomacy)
                {
                    TargetDiplomacy->Stances.Add(Proxy->EntityHandle, EArcDiplomaticStance::Allied);
                }

                Proxy->FactionState.Stability += 5.0f;
                if (TargetProxy)
                {
                    TargetProxy->FactionState.Stability += 5.0f;
                }

                break;
            }

            case EArcFactionAction::FoundSettlement:
            {
                const float WealthCost = 15.0f * I;
                if (OwnSettlements->OwnedSettlements.Num() < 2 || Proxy->FactionState.Wealth < WealthCost)
                {
                    break;
                }
                // No vacant agent slots — skip founding
                if (Coordinator && Coordinator->SettlementManager
                    && SettlementProxies.Num() >= Coordinator->SettlementManager->GetMaxAgentNum())
                {
                    break;
                }

#if WITH_EDITOR
                TArray<FInstancedStruct> NewSettlementFragments;
                NewSettlementFragments.Emplace(FInstancedStruct::Make<FArcSettlementFragment>());
                NewSettlementFragments.Emplace(FInstancedStruct::Make<FArcSettlementFactionFragment>());
                NewSettlementFragments.Emplace(FInstancedStruct::Make<FArcSettlementMarketFragment>());
                NewSettlementFragments.Emplace(FInstancedStruct::Make<FArcSettlementWorkforceFragment>());
                NewSettlementFragments.Emplace(FInstancedStruct::Make<FArcPopulationFragment>());

                const FMassEntityHandle NewHandle = CachedEntityManager->CreateEntity(NewSettlementFragments);

                FArcSettlementFragment& NewSettlement = CachedEntityManager->GetFragmentDataChecked<FArcSettlementFragment>(NewHandle);
                NewSettlement.bPlayerOwned = false;

                FArcSettlementFactionFragment& NewFactionLink = CachedEntityManager->GetFragmentDataChecked<FArcSettlementFactionFragment>(NewHandle);
                NewFactionLink.OwningFaction = Proxy->EntityHandle;

                FArcSettlementMarketFragment& NewMarket = CachedEntityManager->GetFragmentDataChecked<FArcSettlementMarketFragment>(NewHandle);
                NewMarket.TotalStorageCap = 100;
                NewMarket.CurrentTotalStorage = 20;

                FArcSettlementWorkforceFragment& NewWorkforce = CachedEntityManager->GetFragmentDataChecked<FArcSettlementWorkforceFragment>(NewHandle);
                NewWorkforce.WorkerCount = 3;
                NewWorkforce.IdleCount = 2;

                FArcPopulationFragment& NewPop = CachedEntityManager->GetFragmentDataChecked<FArcPopulationFragment>(NewHandle);
                NewPop.Population = 10;

                OwnSettlements->OwnedSettlements.Add(NewHandle);
                SpawnedSettlements.Add(NewHandle);

                UArcStrategyAgentProxy* NewProxy = NewObject<UArcStrategyAgentProxy>(this);
                NewProxy->EntityHandle = NewHandle;
                NewProxy->SettlementState.Security = 50.0f;
                NewProxy->SettlementState.Military = 30.0f;

                if (Coordinator && Coordinator->SettlementManager)
                {
                    NewProxy->AgentId = Coordinator->SettlementManager->AddAgent(NewProxy);
                }

                SettlementProxies.Add(NewProxy);

                Proxy->FactionState.Wealth -= WealthCost;
#endif
                break;
            }

            case EArcFactionAction::LaunchAttack:
            {
                if (!bTargetValid || CurrentStance != EArcDiplomaticStance::Hostile)
                {
                    break;
                }

                FMassEntityView TargetFactionView(*CachedEntityManager, TargetHandle);
                FArcFactionSettlementsFragment* TargetSettlements = TargetFactionView.GetFragmentDataPtr<FArcFactionSettlementsFragment>();

                if (!TargetSettlements || TargetSettlements->OwnedSettlements.Num() < 1)
                {
                    break;
                }

                // Find weakest settlement (lowest population)
                FMassEntityHandle WeakestHandle;
                int32 WeakestPop = TNumericLimits<int32>::Max();

                for (const FMassEntityHandle& SHandle : TargetSettlements->OwnedSettlements)
                {
                    if (!CachedEntityManager->IsEntityValid(SHandle))
                    {
                        continue;
                    }
                    FMassEntityView SView(*CachedEntityManager, SHandle);
                    const FArcPopulationFragment* SPop = SView.GetFragmentDataPtr<FArcPopulationFragment>();
                    if (SPop && SPop->Population < WeakestPop)
                    {
                        WeakestPop = SPop->Population;
                        WeakestHandle = SHandle;
                    }
                }

                if (!CachedEntityManager->IsEntityValid(WeakestHandle))
                {
                    break;
                }

                FMassEntityView WeakestView(*CachedEntityManager, WeakestHandle);
                FArcPopulationFragment* WeakestPopFrag = WeakestView.GetFragmentDataPtr<FArcPopulationFragment>();
                FArcSettlementMarketFragment* WeakestMarket = WeakestView.GetFragmentDataPtr<FArcSettlementMarketFragment>();

                if (!WeakestPopFrag || !WeakestMarket)
                {
                    break;
                }

                WeakestPopFrag->Population -= FMath::FloorToInt32(20.0f * I);
                WeakestPopFrag->Population = FMath::Max(0, WeakestPopFrag->Population);
                WeakestMarket->CurrentTotalStorage -= FMath::FloorToInt32(10.0f * I);
                WeakestMarket->CurrentTotalStorage = FMath::Max(0, WeakestMarket->CurrentTotalStorage);

                Proxy->FactionState.Dominance += 5.0f;

                if (WeakestPopFrag->Population <= 0)
                {
                    // Remove from target faction's owned settlements
                    TargetSettlements->OwnedSettlements.Remove(WeakestHandle);

                    // Find and remove settlement proxy
                    for (int32 ProxyIdx = SettlementProxies.Num() - 1; ProxyIdx >= 0; --ProxyIdx)
                    {
                        UArcStrategyAgentProxy* SP = SettlementProxies[ProxyIdx];
                        if (SP && SP->EntityHandle == WeakestHandle)
                        {
#if WITH_EDITOR
                            if (Coordinator && Coordinator->SettlementManager && SP->AgentId != INDEX_NONE)
                            {
                                Coordinator->SettlementManager->RemoveAgent(SP->AgentId);
                            }
#endif
                            SettlementProxies.RemoveAt(ProxyIdx);
                            break;
                        }
                    }

                    SpawnedSettlements.Remove(WeakestHandle);
                    CachedEntityManager->DestroyEntity(WeakestHandle);
                }

                break;
            }

            case EArcFactionAction::EstablishTrade:
            {
                if (!bTargetValid || CurrentStance == EArcDiplomaticStance::Hostile)
                {
                    break;
                }

                // Pick a random owned settlement for each side and boost storage
                if (OwnSettlements->OwnedSettlements.Num() > 0)
                {
                    const int32 OwnIdx = FMath::RandRange(0, OwnSettlements->OwnedSettlements.Num() - 1);
                    const FMassEntityHandle OwnSettleHandle = OwnSettlements->OwnedSettlements[OwnIdx];
                    if (CachedEntityManager->IsEntityValid(OwnSettleHandle))
                    {
                        FMassEntityView OwnSettleView(*CachedEntityManager, OwnSettleHandle);
                        FArcSettlementMarketFragment* OwnSettleMarket = OwnSettleView.GetFragmentDataPtr<FArcSettlementMarketFragment>();
                        if (OwnSettleMarket)
                        {
                            OwnSettleMarket->CurrentTotalStorage += FMath::FloorToInt32(8.0f * I);
                            OwnSettleMarket->CurrentTotalStorage = FMath::Clamp(OwnSettleMarket->CurrentTotalStorage, 0, OwnSettleMarket->TotalStorageCap);
                        }
                    }
                }

                FMassEntityView TargetFactionView(*CachedEntityManager, TargetHandle);
                FArcFactionSettlementsFragment* TargetSettlements = TargetFactionView.GetFragmentDataPtr<FArcFactionSettlementsFragment>();

                if (TargetSettlements && TargetSettlements->OwnedSettlements.Num() > 0)
                {
                    const int32 TgtIdx = FMath::RandRange(0, TargetSettlements->OwnedSettlements.Num() - 1);
                    const FMassEntityHandle TgtSettleHandle = TargetSettlements->OwnedSettlements[TgtIdx];
                    if (CachedEntityManager->IsEntityValid(TgtSettleHandle))
                    {
                        FMassEntityView TgtSettleView(*CachedEntityManager, TgtSettleHandle);
                        FArcSettlementMarketFragment* TgtSettleMarket = TgtSettleView.GetFragmentDataPtr<FArcSettlementMarketFragment>();
                        if (TgtSettleMarket)
                        {
                            TgtSettleMarket->CurrentTotalStorage += FMath::FloorToInt32(8.0f * I);
                            TgtSettleMarket->CurrentTotalStorage = FMath::Clamp(TgtSettleMarket->CurrentTotalStorage, 0, TgtSettleMarket->TotalStorageCap);
                        }
                    }
                }

                Proxy->FactionState.Wealth += 5.0f * I;
                if (TargetProxy)
                {
                    TargetProxy->FactionState.Wealth += 5.0f * I;
                }

                break;
            }

            case EArcFactionAction::Redistribute:
            {
                if (OwnSettlements->OwnedSettlements.Num() < 2)
                {
                    break;
                }

                // Compute average storage across own settlements
                int32 TotalStorage = 0;
                int32 ValidCount = 0;

                for (const FMassEntityHandle& SHandle : OwnSettlements->OwnedSettlements)
                {
                    if (!CachedEntityManager->IsEntityValid(SHandle))
                    {
                        continue;
                    }
                    FMassEntityView SView(*CachedEntityManager, SHandle);
                    const FArcSettlementMarketFragment* SMarket = SView.GetFragmentDataPtr<FArcSettlementMarketFragment>();
                    if (SMarket)
                    {
                        TotalStorage += SMarket->CurrentTotalStorage;
                        ++ValidCount;
                    }
                }

                if (ValidCount < 2)
                {
                    break;
                }

                const int32 AverageStorage = TotalStorage / ValidCount;

                for (const FMassEntityHandle& SHandle : OwnSettlements->OwnedSettlements)
                {
                    if (!CachedEntityManager->IsEntityValid(SHandle))
                    {
                        continue;
                    }
                    FMassEntityView SView(*CachedEntityManager, SHandle);
                    FArcSettlementMarketFragment* SMarket = SView.GetFragmentDataPtr<FArcSettlementMarketFragment>();
                    if (SMarket)
                    {
                        SMarket->CurrentTotalStorage = FMath::Clamp(AverageStorage, 0, SMarket->TotalStorageCap);
                    }
                }

                break;
            }

            default:
                break;
        }

        // --- Post-mutation clamps ---
        Proxy->FactionState.Dominance = FMath::Clamp(Proxy->FactionState.Dominance, 0.0f, 100.0f);
        Proxy->FactionState.Stability = FMath::Clamp(Proxy->FactionState.Stability, 0.0f, 100.0f);
        Proxy->FactionState.Wealth = FMath::Clamp(Proxy->FactionState.Wealth, 0.0f, 100.0f);
        Proxy->FactionState.Expansion = FMath::Clamp(Proxy->FactionState.Expansion, 0.0f, 100.0f);
    }
}
