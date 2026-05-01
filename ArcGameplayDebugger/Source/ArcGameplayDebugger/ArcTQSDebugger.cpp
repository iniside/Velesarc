// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSDebugger.h"
#include "ArcTQSDebuggerDrawComponent.h"
#include "ArcTQSQuerySubsystem.h"
#include "ArcTQSTypes.h"
#include "imgui.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

// ====================================================================
// Helpers
// ====================================================================

namespace ArcTQSDebuggerLocal
{
	UWorld* GetDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	const TCHAR* StatusToString(EArcTQSQueryStatus Status)
	{
		switch (Status)
		{
		case EArcTQSQueryStatus::Pending:		return TEXT("Pending");
		case EArcTQSQueryStatus::Generating:	return TEXT("Generating");
		case EArcTQSQueryStatus::Processing:	return TEXT("Processing");
		case EArcTQSQueryStatus::Selecting:		return TEXT("Selecting");
		case EArcTQSQueryStatus::Completed:		return TEXT("Completed");
		case EArcTQSQueryStatus::Failed:		return TEXT("Failed");
		case EArcTQSQueryStatus::Aborted:		return TEXT("Aborted");
		default:								return TEXT("Unknown");
		}
	}

	const TCHAR* SelectionModeToString(EArcTQSSelectionMode Mode)
	{
		switch (Mode)
		{
		case EArcTQSSelectionMode::HighestScore:					return TEXT("HighestScore");
		case EArcTQSSelectionMode::TopN:							return TEXT("TopN");
		case EArcTQSSelectionMode::AllPassing:						return TEXT("AllPassing");
		case EArcTQSSelectionMode::RandomWeighted:					return TEXT("RandomWeighted");
		case EArcTQSSelectionMode::RandomFromTop5Percent:			return TEXT("RandomFromTop5Percent");
		case EArcTQSSelectionMode::RandomFromTop25Percent:			return TEXT("RandomFromTop25Percent");
		case EArcTQSSelectionMode::RandomFromTopPercent:			return TEXT("RandomFromTopPercent");
		case EArcTQSSelectionMode::WeightedRandomFromTop5Percent:	return TEXT("WeightedRandomFromTop5Percent");
		case EArcTQSSelectionMode::WeightedRandomFromTop25Percent:	return TEXT("WeightedRandomFromTop25Percent");
		case EArcTQSSelectionMode::WeightedRandomFromTopPercent:	return TEXT("WeightedRandomFromTopPercent");
		default:													return TEXT("Unknown");
		}
	}

	const TCHAR* TargetTypeToString(EArcTQSTargetType Type)
	{
		switch (Type)
		{
		case EArcTQSTargetType::None:			return TEXT("None");
		case EArcTQSTargetType::MassEntity:		return TEXT("MassEntity");
		case EArcTQSTargetType::Actor:			return TEXT("Actor");
		case EArcTQSTargetType::Location:		return TEXT("Location");
		case EArcTQSTargetType::Object:			return TEXT("Object");
		case EArcTQSTargetType::SmartObject:	return TEXT("SmartObject");
		default:								return TEXT("Unknown");
		}
	}
}

// ====================================================================
// PIMPL
// ====================================================================

struct FArcTQSDebugger::FImpl
{
	struct FQueryRecord
	{
		FArcTQSDebugQueryData QueryData;
		FMassEntityHandle Entity;
		double CaptureTime = 0.0;
		FString Label;
	};

	TArray<FQueryRecord> History;
	int32 SelectedRecordIndex = INDEX_NONE;
	int32 MaxHistorySize = 100;
	double LastPollTime = 0.0;
	char FilterBuf[256] = {};

	TMap<FMassEntityHandle, double> LastSeenTimestamps;

	TWeakObjectPtr<AActor> DrawActor;
	TWeakObjectPtr<UArcTQSDebuggerDrawComponent> DrawComponent;

	bool bDrawLabels = true;
	bool bDrawFilteredItems = false;
	bool bDrawContextLocations = true;
	int32 HoveredItemIndex = INDEX_NONE;
	int32 SelectedItemIndex = INDEX_NONE;
	bool bDrawAllItems = false;
	bool bDrawScores = false;
	int32 SelectedStepIndex = INDEX_NONE;

	void PollNewQueries();
	void EnsureDrawActor(UWorld* World);
	void DestroyDrawActor();
	void RefreshVisualization();
	void DrawHistoryList();
	void DrawQueryDetail(const FQueryRecord& Record);
	void DrawOverviewTab(const FArcTQSDebugQueryData& Data);
	void DrawAllItemsTab(const FArcTQSDebugQueryData& Data);
	void DrawResultsTab(const FArcTQSDebugQueryData& Data);
	void DrawStepsTab(const FArcTQSDebugQueryData& Data);
};

// ====================================================================
// FArcTQSDebugger public interface
// ====================================================================

FArcTQSDebugger::FArcTQSDebugger()
	: Impl(MakeUnique<FImpl>())
{
}

FArcTQSDebugger::~FArcTQSDebugger() = default;

void FArcTQSDebugger::Initialize()
{
	Impl->History.Reset();
	Impl->SelectedRecordIndex = INDEX_NONE;
	Impl->LastPollTime = 0.0;
	Impl->LastSeenTimestamps.Reset();
	Impl->HoveredItemIndex = INDEX_NONE;
	Impl->SelectedItemIndex = INDEX_NONE;
	Impl->SelectedStepIndex = INDEX_NONE;
	FMemory::Memzero(Impl->FilterBuf, sizeof(Impl->FilterBuf));
}

void FArcTQSDebugger::Uninitialize()
{
	Impl->DestroyDrawActor();
	Impl->History.Reset();
	Impl->LastSeenTimestamps.Reset();
	Impl->SelectedRecordIndex = INDEX_NONE;
	Impl->LastPollTime = 0.0;
	Impl->HoveredItemIndex = INDEX_NONE;
	Impl->SelectedItemIndex = INDEX_NONE;
	Impl->SelectedStepIndex = INDEX_NONE;
}

void FArcTQSDebugger::Draw()
{
	Impl->PollNewQueries();

	ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("TQS Debugger", &bShow))
	{
		ImGui::End();
		if (Impl->DrawComponent.IsValid()) { Impl->DrawComponent->ClearQueryData(); }
		return;
	}

	// Top bar
	ImGui::Text("History: %d entries", Impl->History.Num());
	ImGui::SameLine();

	int32 MaxHistorySizeTmp = Impl->MaxHistorySize;
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::InputInt("Max History", &MaxHistorySizeTmp))
	{
		Impl->MaxHistorySize = FMath::Max(10, MaxHistorySizeTmp);
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear"))
	{
		Impl->History.Reset();
		Impl->LastSeenTimestamps.Reset();
		Impl->SelectedRecordIndex = INDEX_NONE;
		Impl->RefreshVisualization();
	}

	bool bTogglesChanged = false;
	ImGui::SameLine();
	if (ImGui::Checkbox("Labels", &Impl->bDrawLabels))
	{
		bTogglesChanged = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Filtered", &Impl->bDrawFilteredItems))
	{
		bTogglesChanged = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("CtxLocs", &Impl->bDrawContextLocations))
	{
		bTogglesChanged = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Draw All", &Impl->bDrawAllItems))
	{
		bTogglesChanged = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("Scores", &Impl->bDrawScores))
	{
		bTogglesChanged = true;
	}

	if (bTogglesChanged)
	{
		Impl->RefreshVisualization();
	}

	ImGui::Separator();

	// Two-pane layout
	const float LeftWidth = ImGui::GetContentRegionAvail().x * 0.30f;

	ImGui::BeginChild("HistoryPane", ImVec2(LeftWidth, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
	Impl->DrawHistoryList();
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("DetailPane", ImVec2(0.0f, 0.0f), true);
	if (Impl->SelectedRecordIndex != INDEX_NONE && Impl->History.IsValidIndex(Impl->SelectedRecordIndex))
	{
		Impl->DrawQueryDetail(Impl->History[Impl->SelectedRecordIndex]);
	}
	else
	{
		ImGui::TextDisabled("Select a query record on the left.");
	}
	ImGui::EndChild();

	ImGui::End();
}

// ====================================================================
// FImpl — History polling
// ====================================================================

void FArcTQSDebugger::FImpl::PollNewQueries()
{
#if !UE_BUILD_SHIPPING
	UWorld* World = ArcTQSDebuggerLocal::GetDebugWorld();
	if (!World)
	{
		return;
	}

	UArcTQSQuerySubsystem* TQSSub = World->GetSubsystem<UArcTQSQuerySubsystem>();
	if (!TQSSub)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (Now - LastPollTime < 0.5)
	{
		return;
	}
	LastPollTime = Now;

	const TMap<FMassEntityHandle, FArcTQSDebugQueryData>& AllData = TQSSub->GetAllDebugData();
	for (const TPair<FMassEntityHandle, FArcTQSDebugQueryData>& Pair : AllData)
	{
		const FMassEntityHandle Entity = Pair.Key;
		const FArcTQSDebugQueryData& QueryData = Pair.Value;

		const double* PrevTimestamp = LastSeenTimestamps.Find(Entity);
		if (PrevTimestamp && FMath::IsNearlyEqual(*PrevTimestamp, QueryData.Timestamp, SMALL_NUMBER))
		{
			continue;
		}

		LastSeenTimestamps.Add(Entity, QueryData.Timestamp);

		FQueryRecord Record;
		Record.QueryData = QueryData;
		Record.Entity = Entity;
		Record.CaptureTime = Now;
		Record.Label = FString::Printf(TEXT("E_%d @ %.1fs"), Entity.Index, Now);

		History.Add(MoveTemp(Record));

		while (History.Num() > MaxHistorySize)
		{
			History.RemoveAt(0);
			if (SelectedRecordIndex != INDEX_NONE)
			{
				--SelectedRecordIndex;
				if (SelectedRecordIndex < 0)
				{
					SelectedRecordIndex = INDEX_NONE;
				}
			}
		}
	}
#endif
}

// ====================================================================
// FImpl — History list
// ====================================================================

void FArcTQSDebugger::FImpl::DrawHistoryList()
{
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##filter", FilterBuf, sizeof(FilterBuf));
	ImGui::Separator();

	const FString FilterStr = FString(ANSI_TO_TCHAR(FilterBuf));

	for (int32 i = History.Num() - 1; i >= 0; --i)
	{
		const FQueryRecord& Record = History[i];

		if (FilterStr.Len() > 0 && !Record.Label.Contains(FilterStr, ESearchCase::IgnoreCase))
		{
			continue;
		}

		const FString StatusStr = FString(ArcTQSDebuggerLocal::StatusToString(Record.QueryData.Status));
		const FString EntryLabel = FString::Printf(
			TEXT("%s  [%s]  items:%d  %.2fms"),
			*Record.Label,
			*StatusStr,
			Record.QueryData.AllItems.Num(),
			Record.QueryData.ExecutionTimeMs);

		const bool bSelected = (SelectedRecordIndex == i);
		if (ImGui::Selectable(TCHAR_TO_ANSI(*EntryLabel), bSelected))
		{
			if (SelectedRecordIndex != i)
			{
				SelectedRecordIndex = i;
				HoveredItemIndex = INDEX_NONE;
				SelectedItemIndex = INDEX_NONE;
				SelectedStepIndex = INDEX_NONE;
				RefreshVisualization();
			}
		}
	}
}

// ====================================================================
// FImpl — Detail panel
// ====================================================================

void FArcTQSDebugger::FImpl::DrawQueryDetail(const FQueryRecord& Record)
{
	ImGui::Text("Entity Index: %d", Record.Entity.Index);
	ImGui::SameLine();
	ImGui::Text("  Captured: %.2fs", Record.CaptureTime);
	ImGui::Separator();

	if (ImGui::BeginTabBar("QueryDetailTabs"))
	{
		if (ImGui::BeginTabItem("Overview"))
		{
			DrawOverviewTab(Record.QueryData);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("All Items"))
		{
			DrawAllItemsTab(Record.QueryData);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Results"))
		{
			DrawResultsTab(Record.QueryData);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Steps"))
		{
			DrawStepsTab(Record.QueryData);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void FArcTQSDebugger::FImpl::DrawOverviewTab(const FArcTQSDebugQueryData& Data)
{
	ImGui::Text("Status:          %s", TCHAR_TO_ANSI(ArcTQSDebuggerLocal::StatusToString(Data.Status)));
	ImGui::Text("SelectionMode:   %s", TCHAR_TO_ANSI(ArcTQSDebuggerLocal::SelectionModeToString(Data.SelectionMode)));
	ImGui::Text("QuerierLocation: (%.1f, %.1f, %.1f)", Data.QuerierLocation.X, Data.QuerierLocation.Y, Data.QuerierLocation.Z);
	ImGui::Text("ContextLocs:     %d", Data.ContextLocations.Num());
	ImGui::Text("TotalGenerated:  %d", Data.TotalGenerated);
	ImGui::Text("TotalValid:      %d", Data.TotalValid);
	ImGui::Text("Results:         %d", Data.Results.Num());
	ImGui::Text("ExecTime:        %.4f ms", Data.ExecutionTimeMs);
}

void FArcTQSDebugger::FImpl::DrawAllItemsTab(const FArcTQSDebugQueryData& Data)
{
	// Build result lookup set by matching location + type
	TSet<int32> ResultIndices;
	for (const FArcTQSTargetItem& Result : Data.Results)
	{
		for (int32 i = 0; i < Data.AllItems.Num(); ++i)
		{
			if (Data.AllItems[i].Location.Equals(Result.Location, 1.0f) &&
				Data.AllItems[i].TargetType == Result.TargetType)
			{
				ResultIndices.Add(i);
				break;
			}
		}
	}

	constexpr ImGuiTableFlags TableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	if (ImGui::BeginTable("AllItemsTable", 6, TableFlags))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Idx",      ImGuiTableColumnFlags_WidthFixed, 40.0f);
		ImGui::TableSetupColumn("Type",     ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Score",    ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Valid",    ImGuiTableColumnFlags_WidthFixed, 45.0f);
		ImGui::TableSetupColumn("Ctx",      ImGuiTableColumnFlags_WidthFixed, 35.0f);
		ImGui::TableHeadersRow();

		int32 NewHoveredIndex = INDEX_NONE;

		for (int32 i = 0; i < Data.AllItems.Num(); ++i)
		{
			const FArcTQSTargetItem& Item = Data.AllItems[i];
			const bool bIsResult = ResultIndices.Contains(i);

			ImGui::TableNextRow();

			// Row coloring
			if (!Item.bValid)
			{
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(80, 80, 80, 100));
			}
			else if (bIsResult)
			{
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(50, 160, 50, 100));
			}

			ImGui::TableSetColumnIndex(0);
			const FString IdxLabel = FString::Printf(TEXT("%d##row%d"), i, i);
			const bool bIsItemSelected = (SelectedItemIndex == i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*IdxLabel), bIsItemSelected,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
			{
				SelectedItemIndex = bIsItemSelected ? INDEX_NONE : i;
				RefreshVisualization();
			}
			if (ImGui::IsItemHovered())
			{
				NewHoveredIndex = i;
			}

			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(TCHAR_TO_ANSI(ArcTQSDebuggerLocal::TargetTypeToString(Item.TargetType)));

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("(%.0f, %.0f, %.0f)", Item.Location.X, Item.Location.Y, Item.Location.Z);

			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%.3f", Item.Score);

			ImGui::TableSetColumnIndex(4);
			ImGui::TextUnformatted(Item.bValid ? "yes" : "no");

			ImGui::TableSetColumnIndex(5);
			if (Item.ContextIndex != INDEX_NONE)
			{
				ImGui::Text("%d", Item.ContextIndex);
			}
			else
			{
				ImGui::TextDisabled("-");
			}
		}

		ImGui::EndTable();

		if (NewHoveredIndex != HoveredItemIndex)
		{
			HoveredItemIndex = NewHoveredIndex;
			RefreshVisualization();
		}
	}
}

void FArcTQSDebugger::FImpl::DrawResultsTab(const FArcTQSDebugQueryData& Data)
{
	constexpr ImGuiTableFlags TableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingFixedFit;

	if (ImGui::BeginTable("ResultsTable", 4, TableFlags))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Idx",      ImGuiTableColumnFlags_WidthFixed, 40.0f);
		ImGui::TableSetupColumn("Type",     ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Score",    ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableHeadersRow();

		for (int32 i = 0; i < Data.Results.Num(); ++i)
		{
			const FArcTQSTargetItem& Item = Data.Results[i];

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%d", i);

			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(TCHAR_TO_ANSI(ArcTQSDebuggerLocal::TargetTypeToString(Item.TargetType)));

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("(%.0f, %.0f, %.0f)", Item.Location.X, Item.Location.Y, Item.Location.Z);

			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%.3f", Item.Score);
		}

		ImGui::EndTable();
	}
}

void FArcTQSDebugger::FImpl::DrawStepsTab(const FArcTQSDebugQueryData& Data)
{
#if !UE_BUILD_SHIPPING
	if (Data.StepBreakdown.IsEmpty())
	{
		ImGui::TextDisabled("No step breakdown data available.");
		return;
	}

	const float LeftWidth = ImGui::GetContentRegionAvail().x * 0.35f;

	ImGui::BeginChild("StepListPane", ImVec2(LeftWidth, 0.0f), true);

	for (int32 s = 0; s < Data.StepBreakdown.Num(); ++s)
	{
		const FArcTQSDebugStepData& StepData = Data.StepBreakdown[s];

		const FString StepLabel = FString::Printf(TEXT("[%d] %s (%s, w=%.2f) flt:%d"),
			s,
			*StepData.StepName,
			StepData.StepType == EArcTQSStepType::Filter ? TEXT("Filter") : TEXT("Score"),
			StepData.Weight,
			StepData.FilteredCount);

		const bool bSelected = (SelectedStepIndex == s);
		if (ImGui::Selectable(TCHAR_TO_ANSI(*StepLabel), bSelected))
		{
			SelectedStepIndex = s;
		}
	}

	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("StepDetailPane", ImVec2(0.0f, 0.0f), true);

	if (SelectedStepIndex != INDEX_NONE && Data.StepBreakdown.IsValidIndex(SelectedStepIndex))
	{
		const FArcTQSDebugStepData& StepData = Data.StepBreakdown[SelectedStepIndex];

		const TArray<float>* PrevCumulative = nullptr;
		if (SelectedStepIndex > 0)
		{
			PrevCumulative = &Data.StepBreakdown[SelectedStepIndex - 1].CumulativeScores;
		}

		constexpr ImGuiTableFlags TableFlags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit;

		if (ImGui::BeginTable("StepItemsTable", 5, TableFlags))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Item",   ImGuiTableColumnFlags_WidthFixed,   45.0f);
			ImGui::TableSetupColumn("Raw",    ImGuiTableColumnFlags_WidthFixed,   60.0f);
			ImGui::TableSetupColumn("Before", ImGuiTableColumnFlags_WidthFixed,   60.0f);
			ImGui::TableSetupColumn("After",  ImGuiTableColumnFlags_WidthFixed,   60.0f);
			ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			for (int32 i = 0; i < Data.AllItems.Num(); ++i)
			{
				const FArcTQSTargetItem& Item = Data.AllItems[i];
				const float RawScore  = StepData.RawScores.IsValidIndex(i)          ? StepData.RawScores[i]          : 0.0f;
				const float CumAfter  = StepData.CumulativeScores.IsValidIndex(i)   ? StepData.CumulativeScores[i]   : 0.0f;
				const float CumBefore = (PrevCumulative && PrevCumulative->IsValidIndex(i)) ? (*PrevCumulative)[i]   : 1.0f;

				bool bFilteredByThisStep = false;
				bool bAlreadyFiltered    = false;

				if (!Item.bValid)
				{
					if (StepData.StepType == EArcTQSStepType::Filter && RawScore <= 0.0f && CumBefore > 0.0f)
					{
						bFilteredByThisStep = true;
					}
					else
					{
						bAlreadyFiltered = true;
					}
				}

				ImGui::TableNextRow();

				if (bFilteredByThisStep)
				{
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(200, 50, 50, 100));
				}
				else if (bAlreadyFiltered)
				{
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(80, 80, 80, 100));
				}

				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%d", i);

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.3f", RawScore);

				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.3f", CumBefore);

				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%.3f", CumAfter);

				ImGui::TableSetColumnIndex(4);
				if (bFilteredByThisStep)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "FILTERED");
				}
				else if (bAlreadyFiltered)
				{
					ImGui::TextDisabled("filtered");
				}
				else
				{
					ImGui::Text("valid");
				}
			}

			ImGui::EndTable();
		}
	}
	else
	{
		ImGui::TextDisabled("Select a step on the left.");
	}

	ImGui::EndChild();
#else
	ImGui::TextDisabled("Step breakdown not available in shipping builds.");
#endif
}

// ====================================================================
// FImpl — 3D Visualization
// ====================================================================

void FArcTQSDebugger::FImpl::EnsureDrawActor(UWorld* World)
{
	if (DrawActor.IsValid())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transient;
	AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!NewActor)
	{
		return;
	}

	// Note: do NOT call SetActorHiddenInGame — it hides the debug draw proxy too

#if WITH_EDITOR
	NewActor->SetActorLabel(TEXT("TQSDebuggerDraw"));
#endif

	UArcTQSDebuggerDrawComponent* NewComponent = NewObject<UArcTQSDebuggerDrawComponent>(NewActor, UArcTQSDebuggerDrawComponent::StaticClass());
	NewComponent->RegisterComponent();
	NewActor->AddInstanceComponent(NewComponent);

	DrawActor = NewActor;
	DrawComponent = NewComponent;
}

void FArcTQSDebugger::FImpl::DestroyDrawActor()
{
	if (DrawActor.IsValid())
	{
		DrawActor->Destroy();
	}
	DrawActor.Reset();
	DrawComponent.Reset();
}

void FArcTQSDebugger::FImpl::RefreshVisualization()
{
	if (SelectedRecordIndex == INDEX_NONE || !History.IsValidIndex(SelectedRecordIndex))
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearQueryData();
		}
		return;
	}

	UWorld* World = ArcTQSDebuggerLocal::GetDebugWorld();
	if (!World)
	{
		return;
	}

	EnsureDrawActor(World);

	if (!DrawComponent.IsValid())
	{
		return;
	}

	const FQueryRecord& Record = History[SelectedRecordIndex];
	DrawComponent->UpdateQueryData(
		Record.QueryData,
		bDrawAllItems,
		bDrawLabels,
		bDrawScores,
		bDrawFilteredItems,
		SelectedItemIndex,
		HoveredItemIndex);
}
