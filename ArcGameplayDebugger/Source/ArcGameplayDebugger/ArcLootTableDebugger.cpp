// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcLootTableDebugger.h"

#include "imgui.h"
#include "ArcCore/Items/Loot/ArcLootTable.h"
#include "ArcCore/Items/Loot/ArcLootDropSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void FArcLootTableDebugger::Initialize()
{
	RefreshLootTables();
	SelectedTableIndex = -1;
	LoadedTable = nullptr;
	SimulationResults.Empty();
	bSimulationDirty = true;
}

void FArcLootTableDebugger::Uninitialize()
{
	CachedLootTables.Empty();
	CachedLootTableNames.Empty();
	LoadedTable = nullptr;
	SimulationResults.Empty();
}

void FArcLootTableDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Loot Tables", &bShow);

	const float PanelWidth = ImGui::GetContentRegionAvail().x;
	const float LeftWidth = PanelWidth * 0.30f;

	// Left panel: table list
	if (ImGui::BeginChild("LootTableList", ImVec2(LeftWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawTableList();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel: detail
	if (ImGui::BeginChild("LootTableDetail", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		UArcLootTable* Table = LoadedTable.Get();
		if (Table)
		{
			DrawTableDetail(Table);
		}
		else
		{
			ImGui::TextDisabled("Select a loot table from the list.");
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

void FArcLootTableDebugger::DrawTableList()
{
	if (ImGui::Button("Refresh"))
	{
		RefreshLootTables();
	}

	ImGui::SameLine();
	ImGui::Text("%d tables", CachedLootTables.Num());

	ImGui::InputText("##LootFilter", FilterBuf, IM_ARRAYSIZE(FilterBuf));

	const FString Filter = FString(ANSI_TO_TCHAR(FilterBuf)).ToLower();

	for (int32 i = 0; i < CachedLootTableNames.Num(); ++i)
	{
		if (!Filter.IsEmpty() && !CachedLootTableNames[i].ToLower().Contains(Filter))
		{
			continue;
		}

		const bool bSelected = (SelectedTableIndex == i);
		if (ImGui::Selectable(TCHAR_TO_ANSI(*CachedLootTableNames[i]), bSelected))
		{
			if (SelectedTableIndex != i)
			{
				SelectedTableIndex = i;
				bSimulationDirty = true;
				SimulationResults.Empty();

				// Load the asset
				UObject* Loaded = CachedLootTables[i].GetAsset();
				LoadedTable = Cast<UArcLootTable>(Loaded);
			}
		}
	}
}

void FArcLootTableDebugger::DrawTableDetail(UArcLootTable* Table)
{
	ImGui::Text("Asset: %s", TCHAR_TO_ANSI(*Table->GetName()));
	ImGui::Text("Entries: %d", Table->Entries.Num());
	ImGui::Separator();

	DrawStrategyInfo(Table);
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Entries", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawEntriesTable(Table);
	}

	ImGui::Separator();

	if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawSimulation(Table);
	}

	ImGui::Separator();

	if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawSpawnControls(Table);
	}
}

void FArcLootTableDebugger::DrawStrategyInfo(const UArcLootTable* Table)
{
	if (!Table->RollStrategy.IsValid())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No roll strategy assigned!");
		return;
	}

	const UScriptStruct* StrategyType = Table->RollStrategy.GetScriptStruct();
	ImGui::Text("Strategy: %s", TCHAR_TO_ANSI(*StrategyType->GetName()));

	// Show strategy properties via reflection
	const void* StrategyData = Table->RollStrategy.GetMemory();
	for (TFieldIterator<FProperty> It(StrategyType); It; ++It)
	{
		FProperty* Prop = *It;

		// Skip inherited UObject/base properties
		if (Prop->GetOwnerStruct() == FArcLootRollStrategy::StaticStruct())
		{
			continue;
		}

		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(StrategyData);

		if (const FIntProperty* IntProp = CastField<FIntProperty>(Prop))
		{
			ImGui::Text("  %s: %d", TCHAR_TO_ANSI(*Prop->GetName()), IntProp->GetPropertyValue(ValuePtr));
		}
		else if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			ImGui::Text("  %s: %.2f", TCHAR_TO_ANSI(*Prop->GetName()), FloatProp->GetPropertyValue(ValuePtr));
		}
		else
		{
			FString ValueStr;
			Prop->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
			ImGui::Text("  %s: %s", TCHAR_TO_ANSI(*Prop->GetName()), TCHAR_TO_ANSI(*ValueStr));
		}
	}
}

void FArcLootTableDebugger::DrawEntriesTable(const UArcLootTable* Table)
{
	constexpr int32 ColumnCount = 6;
	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;

	if (ImGui::BeginTable("##LootEntries", ColumnCount, TableFlags))
	{
		ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.0f);
		ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Drop %", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Min", ImGuiTableColumnFlags_WidthFixed, 40.0f);
		ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 40.0f);
		ImGui::TableHeadersRow();

		for (int32 Idx = 0; Idx < Table->Entries.Num(); ++Idx)
		{
			const FArcLootTableEntry* Entry = Table->Entries[Idx].GetPtr<FArcLootTableEntry>();
			if (!Entry)
			{
				continue;
			}

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%d", Idx);

			ImGui::TableSetColumnIndex(1);
			FString ItemName;
#if WITH_EDITORONLY_DATA
			ItemName = Entry->ItemDefinitionId.DisplayName;
#endif
			if (ItemName.IsEmpty())
			{
				ItemName = Entry->ItemDefinitionId.AssetId.PrimaryAssetName.ToString();
			}
			ImGui::Text("%s", TCHAR_TO_ANSI(*ItemName));

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%.1f", Entry->GetWeight());

			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%.0f%%", Entry->GetDropProbability() * 100.0f);

			ImGui::TableSetColumnIndex(4);
			ImGui::Text("%d", Entry->MinAmount);

			ImGui::TableSetColumnIndex(5);
			ImGui::Text("%d", Entry->MaxAmount);
		}

		ImGui::EndTable();
	}
}

void FArcLootTableDebugger::DrawSimulation(const UArcLootTable* Table)
{
	ImGui::InputInt("Rolls", &SimulationRolls);
	SimulationRolls = FMath::Max(1, SimulationRolls);

	ImGui::SameLine();
	if (ImGui::Button("Simulate"))
	{
		RunSimulation(Table);
	}

	if (SimulationResults.IsEmpty())
	{
		ImGui::TextDisabled("Click Simulate to run Monte Carlo simulation.");
		return;
	}

	ImGui::Text("Results (%d rolls):", SimulationRolls);

	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;

	if (ImGui::BeginTable("##SimResults", 4, TableFlags))
	{
		ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Drop Rate", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn("Avg Amount", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn("Times Dropped", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableHeadersRow();

		for (const FArcLootSimEntryStats& Stats : SimulationResults)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", TCHAR_TO_ANSI(*Stats.ItemName));

			ImGui::TableSetColumnIndex(1);
			const float DropRate = SimulationRolls > 0
				? (static_cast<float>(Stats.TimesDropped) / static_cast<float>(SimulationRolls)) * 100.0f
				: 0.0f;
			ImGui::Text("%.1f%%", DropRate);

			ImGui::TableSetColumnIndex(2);
			const float AvgAmount = Stats.TimesDropped > 0
				? static_cast<float>(Stats.TotalAmount) / static_cast<float>(Stats.TimesDropped)
				: 0.0f;
			ImGui::Text("%.1f", AvgAmount);

			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%d", Stats.TimesDropped);
		}

		ImGui::EndTable();
	}
}

void FArcLootTableDebugger::DrawSpawnControls(const UArcLootTable* Table)
{
	UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	if (!World)
	{
		ImGui::TextDisabled("No play world.");
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		ImGui::TextDisabled("No local player pawn.");
		return;
	}

	if (ImGui::Button("Drop Loot Near Player"))
	{
		UArcLootDropSubsystem* LootSubsystem = World->GetSubsystem<UArcLootDropSubsystem>();
		if (LootSubsystem)
		{
			APawn* Pawn = PC->GetPawn();
			const FVector Forward = Pawn->GetActorForwardVector();
			const FVector SpawnLocation = Pawn->GetActorLocation() + Forward * 200.0f;

			FArcLootContext Context;
			Context.RequestingActor = Pawn;

			LootSubsystem->DropLoot(
				Table,
				FConstStructView::Make(Context),
				FTransform(SpawnLocation));
		}
	}

	ImGui::SameLine();
	ImGui::TextDisabled("Spawns loot entities 2m ahead of player");
}

void FArcLootTableDebugger::RunSimulation(const UArcLootTable* Table)
{
	SimulationResults.Empty();

	if (!Table->RollStrategy.IsValid() || Table->Entries.IsEmpty())
	{
		return;
	}

	const FArcLootRollStrategy* Strategy = Table->RollStrategy.GetPtr<FArcLootRollStrategy>();
	if (!Strategy)
	{
		return;
	}

	// Initialize per-entry stats
	SimulationResults.SetNum(Table->Entries.Num());
	for (int32 Idx = 0; Idx < Table->Entries.Num(); ++Idx)
	{
		if (const FArcLootTableEntry* Entry = Table->Entries[Idx].GetPtr<FArcLootTableEntry>())
		{
			FString ItemName;
#if WITH_EDITORONLY_DATA
			ItemName = Entry->ItemDefinitionId.DisplayName;
#endif
			if (ItemName.IsEmpty())
			{
				ItemName = Entry->ItemDefinitionId.AssetId.PrimaryAssetName.ToString();
			}
			SimulationResults[Idx].ItemName = ItemName;
		}
	}

	// Run Monte Carlo simulation
	const FConstStructView EmptyContext;

	for (int32 RollIdx = 0; RollIdx < SimulationRolls; ++RollIdx)
	{
		TArray<FArcLootDropResult> Drops;
		Strategy->RollEntries(Table->Entries, EmptyContext, Drops);

		// Map drops back to entry indices
		for (const FArcLootDropResult& Drop : Drops)
		{
			for (int32 EntryIdx = 0; EntryIdx < Table->Entries.Num(); ++EntryIdx)
			{
				if (const FArcLootTableEntry* Entry = Table->Entries[EntryIdx].GetPtr<FArcLootTableEntry>())
				{
					if (Entry->ItemDefinitionId == Drop.ItemSpec.GetItemDefinitionId())
					{
						SimulationResults[EntryIdx].TimesDropped++;
						SimulationResults[EntryIdx].TotalAmount += Drop.ItemSpec.Amount;
						break;
					}
				}
			}
		}
	}

	bSimulationDirty = false;
}

void FArcLootTableDebugger::RefreshLootTables()
{
	CachedLootTables.Empty();
	CachedLootTableNames.Empty();

	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.GetAssetsByClass(UArcLootTable::StaticClass()->GetClassPathName(), CachedLootTables, true);

	CachedLootTableNames.Reserve(CachedLootTables.Num());
	for (const FAssetData& Asset : CachedLootTables)
	{
		CachedLootTableNames.Add(Asset.AssetName.ToString());
	}
}
