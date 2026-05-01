// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameDirectorDebugger.h"

#include "imgui.h"
#include "Director/ArcGameDirectorSubsystem.h"
#include "Director/ArcGameDirectorTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "Strategy/ArcPopulationFragment.h"
#include "Strategy/ArcStrategicState.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassDebugger.h"
#include "ArcSettlementSubsystem.h"
#include "ArcKnowledgeSubsystem.h"
#include "Engine/Engine.h"

namespace Arcx::GameplayDebugger::Director
{
    static UWorld* GetDebugWorld()
    {
        return GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
    }

    static FMassEntityManager* GetEntityManager()
    {
        UWorld* World = GetDebugWorld();
        if (!World)
        {
            return nullptr;
        }
        UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
        if (!MassSub)
        {
            return nullptr;
        }
        return &MassSub->GetMutableEntityManager();
    }

    static UArcGameDirectorSubsystem* GetDirector()
    {
        UWorld* World = GetDebugWorld();
        if (!World)
        {
            return nullptr;
        }
        return World->GetSubsystem<UArcGameDirectorSubsystem>();
    }
} // namespace Arcx::GameplayDebugger::Director

// ============================================================================
// Initialize / Uninitialize
// ============================================================================

void FArcGameDirectorDebugger::Initialize()
{
    CachedDirector = Arcx::GameplayDebugger::Director::GetDirector();
    CachedEntityManager = Arcx::GameplayDebugger::Director::GetEntityManager();

    CachedOverview.Reset();
    LastOverviewRefreshTime = 0.0f;
    SelectedSettlementIndex = -1;
    FilteredSettlement = FMassEntityHandle();
    bFilterBySettlement = false;
}

void FArcGameDirectorDebugger::Uninitialize()
{
    CachedDirector = nullptr;
    CachedEntityManager = nullptr;
    CachedOverview.Reset();
    LastOverviewRefreshTime = 0.0f;
    SelectedSettlementIndex = -1;
    FilteredSettlement = FMassEntityHandle();
    bFilterBySettlement = false;
}

// ============================================================================
// Draw
// ============================================================================

void FArcGameDirectorDebugger::Draw()
{
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Director", &bShow))
    {
        ImGui::End();
        return;
    }

    // Re-cache every frame in case world changed
    CachedDirector = Arcx::GameplayDebugger::Director::GetDirector();
    CachedEntityManager = Arcx::GameplayDebugger::Director::GetEntityManager();

    if (!CachedDirector)
    {
        ImGui::TextDisabled("UArcGameDirectorSubsystem not available");
        ImGui::End();
        return;
    }

    if (!CachedEntityManager)
    {
        ImGui::TextDisabled("FMassEntityManager not available");
        ImGui::End();
        return;
    }

    // --- Director config values ---
    ImGui::Text("BudgetMs: %.2f  MinHealthMultiplier: %.2f  ProsperityWeight: %.4f",
        CachedDirector->BudgetMs,
        CachedDirector->MinHealthMultiplier,
        CachedDirector->ProsperityWeight);

    ImGui::Separator();

    // --- Filter controls ---
    if (bFilterBySettlement && FilteredSettlement.IsValid())
    {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Filtering by settlement");
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear Filter"))
        {
            bFilterBySettlement = false;
            FilteredSettlement = FMassEntityHandle();
        }
        ImGui::Separator();
    }

    // --- Tab bar ---
    if (ImGui::BeginTabBar("DirectorTabs"))
    {
        if (ImGui::BeginTabItem("Request Queue"))
        {
            DrawRequestQueue();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Evaluation History"))
        {
            DrawEvaluationHistory();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Settlement Overview"))
        {
            DrawSettlementOverview();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ============================================================================
// DrawRequestQueue
// ============================================================================

void FArcGameDirectorDebugger::DrawRequestQueue()
{
    const TArray<FArcPopulationRequest>& Requests = CachedDirector->GetPendingRequests();
    ImGui::Text("Pending requests: %d", Requests.Num());

    if (Requests.Num() == 0)
    {
        ImGui::TextDisabled("(empty)");
        return;
    }

    const double NowTime = CachedEntityManager
        ? GEngine->GetCurrentPlayWorld()->GetTimeSeconds()
        : 0.0;

    if (ImGui::BeginTable("RequestQueueTable", 3,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Settlement Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Requested",       ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Age (s)",          ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        for (int32 Idx = 0; Idx < Requests.Num(); ++Idx)
        {
            const FArcPopulationRequest& Req = Requests[Idx];

            FName SettlementName = NAME_None;
            if (CachedEntityManager->IsEntityValid(Req.SettlementEntity))
            {
                const UScriptStruct* SettlementType = FArcSettlementFragment::StaticStruct();
                FStructView View = CachedEntityManager->GetFragmentDataStruct(Req.SettlementEntity, SettlementType);
                if (View.IsValid())
                {
                    SettlementName = View.Get<FArcSettlementFragment>().SettlementName;
                }
            }

            const double Age = NowTime - Req.RequestTime;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            const FString NameStr = SettlementName.ToString();
            if (ImGui::Selectable(TCHAR_TO_ANSI(*NameStr), false, ImGuiSelectableFlags_SpanAllColumns))
            {
                FilteredSettlement = Req.SettlementEntity;
                bFilterBySettlement = true;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", Req.RequestedCount);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", static_cast<float>(Age));
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// DrawEvaluationHistory
// ============================================================================

void FArcGameDirectorDebugger::DrawEvaluationHistory()
{
    const TArray<FArcPopulationEvaluation>& History = CachedDirector->GetEvaluationHistory();

    const int32 TotalEntries = History.Num();
    int32 DisplayedCount = 0;

    for (int32 Idx = 0; Idx < TotalEntries; ++Idx)
    {
        const FArcPopulationEvaluation& Eval = History[TotalEntries - 1 - Idx];
        if (bFilterBySettlement && FilteredSettlement.IsValid())
        {
            if (Eval.SettlementEntity != FilteredSettlement)
            {
                continue;
            }
        }
        ++DisplayedCount;
    }

    ImGui::Text("Evaluations: %d (showing %d)", TotalEntries, DisplayedCount);

    if (History.Num() == 0)
    {
        ImGui::TextDisabled("(no evaluations yet)");
        return;
    }

    if (ImGui::BeginTable("EvalHistoryTable", 14,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit,
        ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Settlement",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Req",         ImGuiTableColumnFlags_WidthFixed, 35.0f);
        ImGui::TableSetupColumn("Pop",         ImGuiTableColumnFlags_WidthFixed, 35.0f);
        ImGui::TableSetupColumn("Max",         ImGuiTableColumnFlags_WidthFixed, 35.0f);
        ImGui::TableSetupColumn("Idle",        ImGuiTableColumnFlags_WidthFixed, 35.0f);
        ImGui::TableSetupColumn("Stor",        ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn("StorCap",     ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("Prosp",       ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn("CapAdj",      ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("IdleAdj",     ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("HealthMul",   ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Granted",     ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("Spawned",     ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("Time",        ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableHeadersRow();

        for (int32 Idx = 0; Idx < TotalEntries; ++Idx)
        {
            const FArcPopulationEvaluation& Eval = History[TotalEntries - 1 - Idx];

            if (bFilterBySettlement && FilteredSettlement.IsValid())
            {
                if (Eval.SettlementEntity != FilteredSettlement)
                {
                    continue;
                }
            }

            ImGui::TableNextRow();

            // Row color: green if granted > 0, red if granted == 0
            if (Eval.GrantedCount > 0)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    ImGui::GetColorU32(ImVec4(0.1f, 0.4f, 0.1f, 1.0f)));
            }
            else
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    ImGui::GetColorU32(ImVec4(0.4f, 0.1f, 0.1f, 1.0f)));
            }

            ImGui::TableSetColumnIndex(0);
            const FString NameStr = Eval.SettlementName.ToString();
            if (ImGui::Selectable(TCHAR_TO_ANSI(*NameStr), false, ImGuiSelectableFlags_SpanAllColumns))
            {
                FilteredSettlement = Eval.SettlementEntity;
                bFilterBySettlement = true;
            }

            // Tooltip with formula breakdown
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();

                const float FoodRatio = (Eval.StorageCap > 0)
                    ? (static_cast<float>(Eval.CurrentStorage) / static_cast<float>(Eval.StorageCap))
                    : 0.0f;
                const float ProsperityFactor = Eval.Prosperity * 0.01f;
                const float HealthMulRaw = FoodRatio * ProsperityFactor;

                ImGui::Text("min(%d, %d-%d) = %d  (cap adjusted)",
                    Eval.RequestedCount,
                    Eval.MaxPopulation,
                    Eval.CurrentPopulation,
                    Eval.CapAdjusted);

                ImGui::Text("%d - %d = %d  (idle adjusted)",
                    Eval.CapAdjusted,
                    Eval.IdleCount,
                    Eval.IdleAdjusted);

                ImGui::Text("foodRatio = %d/%d = %.2f",
                    Eval.CurrentStorage,
                    Eval.StorageCap,
                    FoodRatio);

                ImGui::Text("prosperityFactor = %.1f * 0.01 = %.2f",
                    Eval.Prosperity,
                    ProsperityFactor);

                ImGui::Text("healthMul = clamp(%.2f * %.2f, %.2f, 1.0) = %.2f",
                    FoodRatio,
                    ProsperityFactor,
                    0.25f,
                    Eval.HealthMultiplier);

                ImGui::Text("granted = round(%d * %.2f) = %d",
                    Eval.IdleAdjusted,
                    Eval.HealthMultiplier,
                    Eval.GrantedCount);

                ImGui::EndTooltip();
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", Eval.RequestedCount);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", Eval.CurrentPopulation);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", Eval.MaxPopulation);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", Eval.IdleCount);

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%d", Eval.CurrentStorage);

            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%d", Eval.StorageCap);

            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%.1f", Eval.Prosperity);

            ImGui::TableSetColumnIndex(8);
            ImGui::Text("%d", Eval.CapAdjusted);

            ImGui::TableSetColumnIndex(9);
            ImGui::Text("%d", Eval.IdleAdjusted);

            ImGui::TableSetColumnIndex(10);
            ImGui::Text("%.2f", Eval.HealthMultiplier);

            ImGui::TableSetColumnIndex(11);
            ImGui::Text("%d", Eval.GrantedCount);

            ImGui::TableSetColumnIndex(12);
            ImGui::Text("%s", Eval.bSpawned ? "yes" : "no");

            ImGui::TableSetColumnIndex(13);
            ImGui::Text("%.1f", static_cast<float>(Eval.EvalTime));
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// DrawSettlementOverview
// ============================================================================

void FArcGameDirectorDebugger::DrawSettlementOverview()
{
    UWorld* World = Arcx::GameplayDebugger::Director::GetDebugWorld();
    if (!World)
    {
        ImGui::TextDisabled("No world available");
        return;
    }

    const float NowTime = static_cast<float>(World->GetTimeSeconds());

    // Refresh cache every 0.5s
    if (NowTime - LastOverviewRefreshTime >= 0.5f || CachedOverview.Num() == 0)
    {
        LastOverviewRefreshTime = NowTime;
        CachedOverview.Reset();

        UArcSettlementSubsystem* SettlementSub = World->GetSubsystem<UArcSettlementSubsystem>();
        UArcKnowledgeSubsystem* KnowledgeSub = World->GetSubsystem<UArcKnowledgeSubsystem>();

        if (!SettlementSub || !KnowledgeSub || !CachedEntityManager)
        {
            ImGui::TextDisabled("Subsystems not available");
            return;
        }

        const UScriptStruct* SettlementType = FArcSettlementFragment::StaticStruct();
        const UScriptStruct* PopType = FArcPopulationFragment::StaticStruct();
        const UScriptStruct* WorkforceType = FArcSettlementWorkforceFragment::StaticStruct();
        const UScriptStruct* MarketType = FArcSettlementMarketFragment::StaticStruct();

        // Use FMassDebugger pattern (same as EconomyDebugger)
        TArray<FMassArchetypeHandle> Archetypes;
#if WITH_MASSENTITY_DEBUG
        Archetypes = FMassDebugger::GetAllArchetypes(*CachedEntityManager);
        for (const FMassArchetypeHandle& Archetype : Archetypes)
        {
            FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);
            if (!Composition.Contains(SettlementType))
            {
                continue;
            }

            TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
            for (const FMassEntityHandle& Entity : Entities)
            {
                FStructView SettlementView = CachedEntityManager->GetFragmentDataStruct(Entity, SettlementType);
                if (!SettlementView.IsValid())
                {
                    continue;
                }

                const FArcSettlementFragment& Settlement = SettlementView.Get<FArcSettlementFragment>();

                // Skip player-owned settlements
                if (Settlement.bPlayerOwned)
                {
                    continue;
                }

                FSettlementOverviewEntry& Entry = CachedOverview.AddDefaulted_GetRef();
                Entry.Entity = Entity;
                Entry.Name = Settlement.SettlementName;
                Entry.bHasNPCConfig = (Settlement.NPCEntityConfig != nullptr);

                // Population
                FStructView PopView = CachedEntityManager->GetFragmentDataStruct(Entity, PopType);
                if (PopView.IsValid())
                {
                    const FArcPopulationFragment& Pop = PopView.Get<FArcPopulationFragment>();
                    Entry.Population = Pop.Population;
                    Entry.MaxPopulation = Pop.MaxPopulation;
                }

                // Workforce
                FStructView WorkforceView = CachedEntityManager->GetFragmentDataStruct(Entity, WorkforceType);
                if (WorkforceView.IsValid())
                {
                    const FArcSettlementWorkforceFragment& Workforce = WorkforceView.Get<FArcSettlementWorkforceFragment>();
                    Entry.IdleCount = Workforce.IdleCount;
                    Entry.WorkerCount = Workforce.WorkerCount;
                    Entry.TransporterCount = Workforce.TransporterCount;
                    Entry.GathererCount = Workforce.GathererCount;
                }

                // Market (food percent)
                FStructView MarketView = CachedEntityManager->GetFragmentDataStruct(Entity, MarketType);
                if (MarketView.IsValid())
                {
                    const FArcSettlementMarketFragment& Market = MarketView.Get<FArcSettlementMarketFragment>();
                    Entry.FoodPercent = (Market.TotalStorageCap > 0)
                        ? (static_cast<float>(Market.CurrentTotalStorage) / static_cast<float>(Market.TotalStorageCap)) * 100.0f
                        : 0.0f;

                    // Compute prosperity via ArcStrategy
                    FStructView WorkforceView2 = CachedEntityManager->GetFragmentDataStruct(Entity, WorkforceType);
                    if (WorkforceView2.IsValid())
                    {
                        const FArcSettlementWorkforceFragment& Workforce2 = WorkforceView2.Get<FArcSettlementWorkforceFragment>();
                        FArcSettlementStrategicState State = ArcStrategy::StateComputation::ComputeSettlementState(
                            *CachedEntityManager,
                            Entity,
                            Settlement,
                            Market,
                            Workforce2,
                            *KnowledgeSub,
                            *SettlementSub);
                        Entry.Prosperity = State.Prosperity;
                    }
                }
            }
        }
#endif
    }

    ImGui::Text("AI Settlements: %d", CachedOverview.Num());

    // --- Force spawn controls ---
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputInt("##ForceCount", &ForceSpawnCount, 1, 5);
    ForceSpawnCount = FMath::Max(1, ForceSpawnCount);
    ImGui::SameLine();
    if (SelectedSettlementIndex >= 0 && SelectedSettlementIndex < CachedOverview.Num())
    {
        const FSettlementOverviewEntry& Selected = CachedOverview[SelectedSettlementIndex];
        if (ImGui::Button("Force Spawn"))
        {
            CachedDirector->RequestPopulation(Selected.Entity, ForceSpawnCount);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("-> %s", TCHAR_TO_ANSI(*Selected.Name.ToString()));
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button("Force Spawn");
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("(select a settlement)");
    }

    if (CachedOverview.Num() == 0)
    {
        ImGui::TextDisabled("(no AI settlements found)");
        return;
    }

    if (ImGui::BeginTable("SettlementOverviewTable", 9,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit,
        ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Settlement",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Pop/Max",     ImGuiTableColumnFlags_WidthFixed, 65.0f);
        ImGui::TableSetupColumn("Idle",        ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Workers",     ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("Trans",       ImGuiTableColumnFlags_WidthFixed, 45.0f);
        ImGui::TableSetupColumn("Gather",      ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Food%",       ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Prosperity",  ImGuiTableColumnFlags_WidthFixed, 75.0f);
        ImGui::TableSetupColumn("NPC Cfg",     ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableHeadersRow();

        for (int32 Idx = 0; Idx < CachedOverview.Num(); ++Idx)
        {
            const FSettlementOverviewEntry& Entry = CachedOverview[Idx];

            ImGui::TableNextRow();

            // Red highlight if pop >= max or no NPCEntityConfig
            const bool bWarning = (Entry.Population >= Entry.MaxPopulation) || !Entry.bHasNPCConfig;
            if (bWarning)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    ImGui::GetColorU32(ImVec4(0.5f, 0.1f, 0.1f, 1.0f)));
            }

            ImGui::TableSetColumnIndex(0);
            const FString NameStr = Entry.Name.ToString();
            const bool bSelected = (SelectedSettlementIndex == Idx);
            if (ImGui::Selectable(TCHAR_TO_ANSI(*NameStr), bSelected, ImGuiSelectableFlags_SpanAllColumns))
            {
                SelectedSettlementIndex = Idx;
                FilteredSettlement = Entry.Entity;
                bFilterBySettlement = true;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d/%d", Entry.Population, Entry.MaxPopulation);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", Entry.IdleCount);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", Entry.WorkerCount);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", Entry.TransporterCount);

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%d", Entry.GathererCount);

            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%.1f%%", Entry.FoodPercent);

            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%.1f", Entry.Prosperity);

            ImGui::TableSetColumnIndex(8);
            ImGui::Text("%s", Entry.bHasNPCConfig ? "yes" : "NO");
        }

        ImGui::EndTable();
    }
}
