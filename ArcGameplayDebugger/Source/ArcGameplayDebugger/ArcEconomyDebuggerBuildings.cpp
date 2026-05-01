// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebugger.h"

#include "imgui.h"
#include "MassEntitySubsystem.h"
#include "MassDebugger.h"
#include "Mass/ArcEconomyFragments.h"
#include "Mass/EntityFragments.h"
#include "ArcSettlementSubsystem.h"
#include "Items/ArcItemDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "Items/ArcItemSpec.h"
#include "ArcAreaSubsystem.h"
#include "Mass/ArcAreaFragments.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectDefinition.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Mass/ArcGovernorLogic.h"
#include "Data/ArcGovernorDataAsset.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

namespace Arcx::GameplayDebugger::Economy
{
	extern UWorld* GetDebugWorld();
	extern FMassEntityManager* GetEntityManager();
	extern UArcSettlementSubsystem* GetSettlementSubsystem();
	extern const ImVec4 DimColor;
	extern const ImVec4 HeaderColor;
} // namespace Arcx::GameplayDebugger::Economy

void FArcEconomyDebugger::RefreshBuildingList()
{
#if WITH_MASSENTITY_DEBUG
	// Preserve selection across refresh
	FMassEntityHandle PreviouslySelectedBuilding;
	if (SelectedBuildingIdx != INDEX_NONE && CachedBuildings.IsValidIndex(SelectedBuildingIdx))
	{
		PreviouslySelectedBuilding = CachedBuildings[SelectedBuildingIdx].Entity;
	}

	CachedBuildings.Reset();
	SelectedBuildingIdx = INDEX_NONE;

	if (SelectedSettlementIdx == INDEX_NONE || !CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	UArcSettlementSubsystem* SettlementSub = Arcx::GameplayDebugger::Economy::GetSettlementSubsystem();
	if (!Manager || !SettlementSub)
	{
		return;
	}

	const FSettlementEntry& Settlement = CachedSettlements[SelectedSettlementIdx];
	const TArray<FMassEntityHandle>& BuildingHandles = SettlementSub->GetBuildingsForSettlement(Settlement.Entity);

	for (const FMassEntityHandle& Handle : BuildingHandles)
	{
		if (!Manager->IsEntityActive(Handle))
		{
			continue;
		}

		FBuildingEntry& Entry = CachedBuildings.AddDefaulted_GetRef();
		Entry.Entity = Handle;

		// Building fragment
		FStructView BuildingView = Manager->GetFragmentDataStruct(Handle, FArcBuildingFragment::StaticStruct());
		if (BuildingView.IsValid())
		{
			const FArcBuildingFragment& Building = BuildingView.Get<FArcBuildingFragment>();
			Entry.DefinitionName = Building.BuildingName.IsNone() ? TEXT("(unnamed)") : *Building.BuildingName.ToString();
			Entry.Location = Building.BuildingLocation;
		}

		// Slot count
		FStructView WorkforceView = Manager->GetFragmentDataStruct(Handle, FArcBuildingWorkforceFragment::StaticStruct());
		if (WorkforceView.IsValid())
		{
			const FArcBuildingWorkforceFragment& Workforce = WorkforceView.Get<FArcBuildingWorkforceFragment>();
			Entry.SlotCount = Workforce.Slots.Num();
		}
	}

	// Restore selection by entity handle
	if (PreviouslySelectedBuilding.IsValid())
	{
		for (int32 i = 0; i < CachedBuildings.Num(); ++i)
		{
			if (CachedBuildings[i].Entity == PreviouslySelectedBuilding)
			{
				SelectedBuildingIdx = i;
				break;
			}
		}
	}
#endif
}

void FArcEconomyDebugger::DrawBuildingsTab()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedSettlementIdx == INDEX_NONE || !CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		return;
	}

	ImGui::SeparatorText("Buildings");

	if (CachedBuildings.IsEmpty())
	{
		ImGui::TextDisabled("No buildings in this settlement");
		return;
	}

	// Building list
	float PanelHeight = ImGui::GetContentRegionAvail().y;
	if (ImGui::BeginChild("BuildingList", ImVec2(0, PanelHeight * 0.3f), ImGuiChildFlags_Borders))
	{
		constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("BuildingTable", 3, TableFlags))
		{
			ImGui::TableSetupColumn("Definition", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthFixed, 200.f);
			ImGui::TableSetupColumn("Slots", ImGuiTableColumnFlags_WidthFixed, 50.f);
			ImGui::TableHeadersRow();

			for (int32 i = 0; i < CachedBuildings.Num(); ++i)
			{
				const FBuildingEntry& Entry = CachedBuildings[i];

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				const bool bSelected = (SelectedBuildingIdx == i);
				FString Label = FString::Printf(TEXT("%s##building%d"), *Entry.DefinitionName, i);
				if (ImGui::Selectable(TCHAR_TO_ANSI(*Label), bSelected, ImGuiSelectableFlags_SpanAllColumns))
				{
					SelectedBuildingIdx = i;
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.0f, %.0f, %.0f", Entry.Location.X, Entry.Location.Y, Entry.Location.Z);

				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%d", Entry.SlotCount);
			}

			ImGui::EndTable();
		}
	}
	ImGui::EndChild();

	// Selected building detail
	DrawBuildingDetail();
#endif
}

void FArcEconomyDebugger::DrawBuildingDetail()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedBuildingIdx == INDEX_NONE || !CachedBuildings.IsValidIndex(SelectedBuildingIdx))
	{
		ImGui::TextDisabled("Select a building from the list above");
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FBuildingEntry& Building = CachedBuildings[SelectedBuildingIdx];
	if (!Manager->IsEntityActive(Building.Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Building entity is no longer active");
		return;
	}

	const FSettlementEntry& Settlement = CachedSettlements[SelectedSettlementIdx];

	ImGui::SeparatorText("Building Detail");
	ImGui::TextColored(Arcx::GameplayDebugger::Economy::HeaderColor, "%s", TCHAR_TO_ANSI(*Building.DefinitionName));

	// Production toggle
	FArcBuildingFragment* BuildingFrag = Manager->GetFragmentDataPtr<FArcBuildingFragment>(Building.Entity);
	if (BuildingFrag)
	{
		ImGui::Checkbox("Production Enabled", &BuildingFrag->bProductionEnabled);
	}

	// Economy config (shared fragment)
	const FArcBuildingEconomyConfig* EconConfig = Manager->GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(Building.Entity);

	if (EconConfig)
	{
		ImGui::Text("Max Slots: %d  |  Workers/Slot: %d", EconConfig->MaxProductionSlots, EconConfig->WorkersPerSlot);

		if (EconConfig->IsGatheringBuilding())
		{
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[Gathering Building]");
			ImGui::Text("Gather Output Items:");
			for (const TObjectPtr<UArcItemDefinition>& Item : EconConfig->GatherOutputItems)
			{
				ImGui::BulletText("%s", Item ? TCHAR_TO_ANSI(*Item->GetName()) : "(null)");
			}
			if (EconConfig->GatherSearchTags.IsValid())
			{
				ImGui::Text("Search Tags: %s", TCHAR_TO_ANSI(*EconConfig->GatherSearchTags.ToStringSimple()));
			}
			ImGui::Text("Search Radius: %.0f", EconConfig->GatherSearchRadius);
		}
		else if (!EconConfig->AllowedRecipes.IsEmpty())
		{
			ImGui::Text("Allowed Recipes:");
			for (const TObjectPtr<UArcRecipeDefinition>& Recipe : EconConfig->AllowedRecipes)
			{
				ImGui::BulletText("%s", Recipe ? TCHAR_TO_ANSI(*Recipe->GetName()) : "(null)");
			}
		}
		else if (EconConfig->ProductionStationTags.IsValid())
		{
			ImGui::Text("Station Tags: %s", TCHAR_TO_ANSI(*EconConfig->ProductionStationTags.ToStringSimple()));
		}
	}

	// Slot table
	FStructView WorkforceView = Manager->GetFragmentDataStruct(Building.Entity, FArcBuildingWorkforceFragment::StaticStruct());
	if (!WorkforceView.IsValid())
	{
		ImGui::TextDisabled("No workforce fragment");
		return;
	}

	FArcBuildingWorkforceFragment& Workforce = WorkforceView.Get<FArcBuildingWorkforceFragment>();

	ImGui::SeparatorText("Production Slots");

	constexpr ImGuiTableFlags SlotTableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("SlotTable", 4, SlotTableFlags))
	{
		ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 40.f);
		ImGui::TableSetupColumn("Recipe", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Workers", ImGuiTableColumnFlags_WidthFixed, 60.f);
		ImGui::TableSetupColumn("Halt (s)", ImGuiTableColumnFlags_WidthFixed, 70.f);
		ImGui::TableHeadersRow();

		for (int32 SlotIdx = 0; SlotIdx < Workforce.Slots.Num(); ++SlotIdx)
		{
			FArcBuildingSlotData& Slot = Workforce.Slots[SlotIdx];

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%d", SlotIdx);

			ImGui::TableSetColumnIndex(1);
			if (EconConfig && !EconConfig->AllowedRecipes.IsEmpty())
			{
				// Editable combo box for player-owned
				FString CurrentRecipeName = Slot.DesiredRecipe ? Slot.DesiredRecipe->GetName() : TEXT("(none)");
				FString ComboId = FString::Printf(TEXT("##recipe_%d"), SlotIdx);

				if (ImGui::BeginCombo(TCHAR_TO_ANSI(*ComboId), TCHAR_TO_ANSI(*CurrentRecipeName)))
				{
					// None option
					if (ImGui::Selectable("(none)", Slot.DesiredRecipe == nullptr))
					{
						Slot.DesiredRecipe = nullptr;
					}

					for (const TObjectPtr<UArcRecipeDefinition>& Recipe : EconConfig->AllowedRecipes)
					{
						if (!Recipe)
						{
							continue;
						}
						FString RecipeName = Recipe->GetName();
						const bool bIsSelected = (Slot.DesiredRecipe == Recipe);
						if (ImGui::Selectable(TCHAR_TO_ANSI(*RecipeName), bIsSelected))
						{
							Slot.DesiredRecipe = Recipe;
						}
					}
					ImGui::EndCombo();
				}
			}
			else
			{
				// Read-only
				FString RecipeName = Slot.DesiredRecipe ? Slot.DesiredRecipe->GetName() : TEXT("(none)");
				ImGui::Text("%s", TCHAR_TO_ANSI(*RecipeName));
			}

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%d", Slot.RequiredWorkerCount);

			ImGui::TableSetColumnIndex(3);
			if (Slot.HaltDuration > 0.f)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "%.1f", Slot.HaltDuration);
			}
			else
			{
				ImGui::TextColored(Arcx::GameplayDebugger::Economy::DimColor, "0.0");
			}
		}

		ImGui::EndTable();
	}

	// === Craft Queue ===
	FStructView QueueView = Manager->GetFragmentDataStruct(Building.Entity, FArcCraftQueueFragment::StaticStruct());
	if (QueueView.IsValid())
	{
		const FArcCraftQueueFragment& Queue = QueueView.Get<FArcCraftQueueFragment>();

		ImGui::SeparatorText("Craft Queue");

		if (Queue.Entries.IsEmpty())
		{
			ImGui::TextColored(Arcx::GameplayDebugger::Economy::DimColor, "Queue empty");
		}
		else
		{
			constexpr ImGuiTableFlags QTableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
			if (ImGui::BeginTable("CraftQueue", 4, QTableFlags))
			{
				ImGui::TableSetupColumn("Recipe", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Done", ImGuiTableColumnFlags_WidthFixed, 60.f);
				ImGui::TableSetupColumn("Prio", ImGuiTableColumnFlags_WidthFixed, 40.f);
				ImGui::TableHeadersRow();

				for (int32 QIdx = 0; QIdx < Queue.Entries.Num(); ++QIdx)
				{
					const FArcCraftQueueEntry& Entry = Queue.Entries[QIdx];
					const bool bIsActive = (QIdx == Queue.ActiveEntryIndex);

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					FString RecipeName = Entry.Recipe ? Entry.Recipe->GetName() : TEXT("(null)");
					if (bIsActive)
					{
						ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", TCHAR_TO_ANSI(*RecipeName));
					}
					else
					{
						ImGui::Text("%s", TCHAR_TO_ANSI(*RecipeName));
					}

					ImGui::TableSetColumnIndex(1);
					if (bIsActive && Entry.Recipe)
					{
						float CraftTime = Entry.Recipe->CraftTime;
						float Progress = (CraftTime > 0.f) ? FMath::Clamp(Entry.ElapsedTickTime / CraftTime, 0.f, 1.f) : 0.f;
						FString BarLabel = FString::Printf(TEXT("%.1f/%.1fs"), Entry.ElapsedTickTime, CraftTime);
						ImGui::ProgressBar(Progress, ImVec2(-1.f, 0.f), TCHAR_TO_ANSI(*BarLabel));
					}
					else
					{
						ImGui::TextColored(Arcx::GameplayDebugger::Economy::DimColor, "waiting");
					}

					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%d/%d", Entry.CompletedAmount, Entry.Amount);

					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%d", Entry.Priority);
				}

				ImGui::EndTable();
			}
		}
	}

	// === Input / Output Items ===
	FStructView InputView = Manager->GetFragmentDataStruct(Building.Entity, FArcCraftInputFragment::StaticStruct());
	FStructView OutputView = Manager->GetFragmentDataStruct(Building.Entity, FArcCraftOutputFragment::StaticStruct());

	if (InputView.IsValid() || OutputView.IsValid())
	{
		ImGui::SeparatorText("Items");

		if (InputView.IsValid())
		{
			const FArcCraftInputFragment& Input = InputView.Get<FArcCraftInputFragment>();
			if (ImGui::TreeNodeEx("Input Items", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (Input.InputItems.IsEmpty())
				{
					ImGui::TextColored(Arcx::GameplayDebugger::Economy::DimColor, "Empty");
				}
				else
				{
					for (const FArcItemSpec& Spec : Input.InputItems)
					{
						const UArcItemDefinition* ItemDef = Spec.GetItemDefinition();
						FString ItemName = ItemDef ? ItemDef->GetName() : TEXT("(null)");
						ImGui::BulletText("%s x%d", TCHAR_TO_ANSI(*ItemName), Spec.Amount);
					}
				}
				ImGui::TreePop();
			}
		}

		if (OutputView.IsValid())
		{
			const FArcCraftOutputFragment& Output = OutputView.Get<FArcCraftOutputFragment>();
			if (ImGui::TreeNodeEx("Output Items", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (Output.OutputItems.IsEmpty())
				{
					ImGui::TextColored(Arcx::GameplayDebugger::Economy::DimColor, "Empty");
				}
				else
				{
					for (const FArcItemSpec& Spec : Output.OutputItems)
					{
						const UArcItemDefinition* ItemDef = Spec.GetItemDefinition();
						FString ItemName = ItemDef ? ItemDef->GetName() : TEXT("(null)");
						ImGui::BulletText("%s x%d", TCHAR_TO_ANSI(*ItemName), Spec.Amount);
					}
				}
				ImGui::TreePop();
			}
		}
	}

	// === Idle Reason ===
	const bool bIsGatheringBuilding = EconConfig && EconConfig->IsGatheringBuilding();
	if (!bIsGatheringBuilding && QueueView.IsValid())
	{
		const FArcCraftQueueFragment& Queue = QueueView.Get<FArcCraftQueueFragment>();
		if (Queue.Entries.IsEmpty() && WorkforceView.IsValid())
		{
			ImGui::SeparatorText("Status");

			for (int32 SlotIdx = 0; SlotIdx < Workforce.Slots.Num(); ++SlotIdx)
			{
				const FArcBuildingSlotData& Slot = Workforce.Slots[SlotIdx];

				if (!Slot.DesiredRecipe)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Slot %d: No recipe assigned", SlotIdx);
					continue;
				}

				if (Slot.HaltDuration > 0.f)
				{
					// Check staffing via area subsystem
					UWorld* World = Arcx::GameplayDebugger::Economy::GetDebugWorld();
					UArcAreaSubsystem* AreaSub = World ? World->GetSubsystem<UArcAreaSubsystem>() : nullptr;

					FStructView AreaView = Manager->GetFragmentDataStruct(Building.Entity, FArcAreaFragment::StaticStruct());
					bool bStaffed = true;

					if (AreaSub && AreaView.IsValid())
					{
						const FArcAreaFragment& AreaFrag = AreaView.Get<FArcAreaFragment>();
						const FArcAreaData* AreaData = AreaSub->GetAreaData(AreaFrag.AreaHandle);
						if (AreaData)
						{
							int32 AreaSlotBase = 0;
							for (int32 s = 0; s < SlotIdx; ++s)
							{
								AreaSlotBase += Workforce.Slots[s].RequiredWorkerCount;
							}
							for (int32 w = 0; w < Slot.RequiredWorkerCount; ++w)
							{
								FArcAreaSlotHandle SlotHandle(AreaFrag.AreaHandle, AreaSlotBase + w);
								if (AreaSub->GetSlotState(SlotHandle) != EArcAreaSlotState::Active)
								{
									bStaffed = false;
									break;
								}
							}
						}
					}

					if (!bStaffed)
					{
						ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Slot %d: Not staffed (halted %.0fs)", SlotIdx, Slot.HaltDuration);
					}
					else
					{
						ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Slot %d: Missing ingredients (halted %.0fs)", SlotIdx, Slot.HaltDuration);
					}
				}
			}
		}
	}

	// === Area Slots ===
	DrawAreaSlotSection();

	// === Smart Object Slots ===
	DrawSmartObjectSlotSection();

	// === Production Diagnostics ===
	if (EconConfig)
	{
		const ImVec4 GreenColor(0.3f, 0.9f, 0.3f, 1.0f);
		const ImVec4 RedColor(1.0f, 0.3f, 0.3f, 1.0f);
		const ImVec4 YellowColor(1.0f, 0.8f, 0.2f, 1.0f);

		ImGui::SeparatorText("Production Diagnostics");

		// 1. Production enabled
		if (BuildingFrag)
		{
			if (BuildingFrag->bProductionEnabled)
			{
				ImGui::TextColored(GreenColor, "[OK] Production enabled");
			}
			else
			{
				ImGui::TextColored(RedColor, "[!!] Production DISABLED");
			}
		}
		else
		{
			ImGui::TextColored(YellowColor, "[--] Production enabled: N/A (no building fragment)");
		}

		if (bIsGatheringBuilding)
		{
			// === Gathering building diagnostics ===

			// 2. Output buffer
			if (BuildingFrag)
			{
				const bool bBufferFull = (BuildingFrag->CurrentOutputCount >= EconConfig->OutputBufferSize);
				if (!bBufferFull)
				{
					ImGui::TextColored(GreenColor, "[OK] Output buffer (%d / %d)", BuildingFrag->CurrentOutputCount, EconConfig->OutputBufferSize);
				}
				else
				{
					ImGui::TextColored(RedColor, "[!!] Output buffer FULL (%d / %d)", BuildingFrag->CurrentOutputCount, EconConfig->OutputBufferSize);
				}
			}
			else
			{
				ImGui::TextColored(YellowColor, "[--] Output buffer: N/A (no building fragment)");
			}

			// 3. Gatherer slots
			UArcSettlementSubsystem* SettlementSub = Arcx::GameplayDebugger::Economy::GetSettlementSubsystem();
			const TArray<FMassEntityHandle>& NPCHandles = SettlementSub ? SettlementSub->GetNPCsForSettlement(Settlement.Entity) : TArray<FMassEntityHandle>();

			int32 AssignedGathererCount = 0;
			TSet<FMassEntityHandle> OccupiedResources;
			TMap<FMassEntityHandle, FMassEntityHandle> ResourceToGatherer;
			int32 IdleNPCCount = 0;

			for (const FMassEntityHandle& NPCHandle : NPCHandles)
			{
				if (!Manager->IsEntityActive(NPCHandle))
				{
					continue;
				}
				const FArcEconomyNPCFragment* NPC = Manager->GetFragmentDataPtr<FArcEconomyNPCFragment>(NPCHandle);
				if (!NPC)
				{
					continue;
				}
				if (NPC->Role == EArcEconomyNPCRole::Idle)
				{
					++IdleNPCCount;
				}
				if (NPC->Role == EArcEconomyNPCRole::Gatherer)
				{
					const FArcGathererFragment* Gatherer = Manager->GetFragmentDataPtr<FArcGathererFragment>(NPCHandle);
					if (Gatherer && Gatherer->AssignedBuildingHandle == Building.Entity)
					{
						++AssignedGathererCount;
						if (Gatherer->TargetResourceHandle.IsValid())
						{
							OccupiedResources.Add(Gatherer->TargetResourceHandle);
							ResourceToGatherer.Add(Gatherer->TargetResourceHandle, NPCHandle);
						}
					}
				}
			}

			int32 FreeSlots = EconConfig->MaxProductionSlots - AssignedGathererCount;
			if (AssignedGathererCount >= EconConfig->MaxProductionSlots)
			{
				ImGui::TextColored(GreenColor, "[OK] Gatherer slots full (%d / %d)", AssignedGathererCount, EconConfig->MaxProductionSlots);
			}
			else if (FreeSlots > 0 && IdleNPCCount > 0)
			{
				ImGui::TextColored(YellowColor, "[??] Gatherer slots (%d / %d) — %d idle NPCs available", AssignedGathererCount, EconConfig->MaxProductionSlots, IdleNPCCount);
			}
			else
			{
				ImGui::TextColored(YellowColor, "[??] Gatherer slots (%d / %d) — no idle NPCs", AssignedGathererCount, EconConfig->MaxProductionSlots);
			}

			// 4. Spatial hash results
			UWorld* World = Arcx::GameplayDebugger::Economy::GetDebugWorld();
			UArcMassSpatialHashSubsystem* SpatialSub = World ? World->GetSubsystem<UArcMassSpatialHashSubsystem>() : nullptr;
			TArray<FArcMassEntityInfo> SpatialResults;

			if (SpatialSub && BuildingFrag)
			{
				for (const FGameplayTag& Tag : EconConfig->GatherSearchTags)
				{
					SpatialSub->QuerySphere(ArcSpatialHash::HashKey(Tag), BuildingFrag->BuildingLocation, EconConfig->GatherSearchRadius, SpatialResults);
				}
			}

			if (!SpatialResults.IsEmpty())
			{
				ImGui::TextColored(GreenColor, "[OK] Resources in range (%d found)", SpatialResults.Num());
			}
			else
			{
				ImGui::TextColored(RedColor, "[!!] No resources found in range (radius: %.0f)", EconConfig->GatherSearchRadius);
			}

			// 5. Unoccupied resources
			int32 UnoccupiedCount = 0;
			for (const FArcMassEntityInfo& Result : SpatialResults)
			{
				if (!OccupiedResources.Contains(Result.Entity))
				{
					++UnoccupiedCount;
				}
			}

			if (UnoccupiedCount > 0)
			{
				ImGui::TextColored(GreenColor, "[OK] Available resources (%d / %d unoccupied)", UnoccupiedCount, SpatialResults.Num());
			}
			else if (!SpatialResults.IsEmpty())
			{
				ImGui::TextColored(YellowColor, "[??] All resources occupied (%d / %d)", SpatialResults.Num(), SpatialResults.Num());
			}

			// 6. Idle NPCs
			if (IdleNPCCount > 0)
			{
				ImGui::TextColored(GreenColor, "[OK] Idle NPCs available (%d)", IdleNPCCount);
			}
			else
			{
				ImGui::TextColored(YellowColor, "[??] No idle NPCs in settlement");
			}

			// 7. Settlement storage
			{
				const FArcSettlementMarketFragment* MarketFrag = Manager->GetFragmentDataPtr<FArcSettlementMarketFragment>(Settlement.Entity);
				if (!MarketFrag)
				{
					ImGui::TextColored(YellowColor, "[--] Settlement storage: N/A (no market fragment)");
				}
				else
				{
					const bool bStorageFull = (MarketFrag->CurrentTotalStorage >= MarketFrag->TotalStorageCap);
					if (!bStorageFull)
					{
						ImGui::TextColored(GreenColor, "[OK] Settlement storage (%d / %d)", MarketFrag->CurrentTotalStorage, MarketFrag->TotalStorageCap);
					}
					else
					{
						ImGui::TextColored(RedColor, "[!!] Settlement storage FULL (%d / %d)", MarketFrag->CurrentTotalStorage, MarketFrag->TotalStorageCap);
					}
				}
			}

			// === Nearby Resources ===
			ImGui::SeparatorText("Nearby Resources");

			if (SpatialResults.IsEmpty())
			{
				ImGui::TextDisabled("No resources found within search radius");
			}
			else
			{
				constexpr ImGuiTableFlags ResTableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
				if (ImGui::BeginTable("NearbyResources", 5, ResTableFlags))
				{
					ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.f);
					ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthFixed, 70.f);
					ImGui::TableSetupColumn("Distance", ImGuiTableColumnFlags_WidthFixed, 80.f);
					ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.f);
					ImGui::TableSetupColumn("Gatherer", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableHeadersRow();

					for (int32 ResIdx = 0; ResIdx < SpatialResults.Num(); ++ResIdx)
					{
						const FArcMassEntityInfo& Result = SpatialResults[ResIdx];
						const bool bOccupied = OccupiedResources.Contains(Result.Entity);

						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%d", ResIdx);

						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%d", Result.Entity.Index);

						ImGui::TableSetColumnIndex(2);
						ImGui::Text("%.0f", Result.Distance);

						ImGui::TableSetColumnIndex(3);
						if (bOccupied)
						{
							ImGui::TextColored(YellowColor, "Occupied");
						}
						else
						{
							ImGui::TextColored(GreenColor, "Free");
						}

						ImGui::TableSetColumnIndex(4);
						if (bOccupied)
						{
							const FMassEntityHandle* GathererHandle = ResourceToGatherer.Find(Result.Entity);
							if (GathererHandle)
							{
								ImGui::Text("%d", GathererHandle->Index);
							}
							else
							{
								ImGui::Text("---");
							}
						}
						else
						{
							ImGui::Text("---");
						}
					}

					ImGui::EndTable();
				}
			}

			// === Assigned Gatherers ===
			ImGui::SeparatorText("Assigned Gatherers");

			TArray<TPair<FMassEntityHandle, const FArcGathererFragment*>> AssignedGatherers;
			for (const FMassEntityHandle& NPCHandle : NPCHandles)
			{
				if (!Manager->IsEntityActive(NPCHandle))
				{
					continue;
				}
				const FArcEconomyNPCFragment* NPC = Manager->GetFragmentDataPtr<FArcEconomyNPCFragment>(NPCHandle);
				if (!NPC || NPC->Role != EArcEconomyNPCRole::Gatherer)
				{
					continue;
				}
				const FArcGathererFragment* Gatherer = Manager->GetFragmentDataPtr<FArcGathererFragment>(NPCHandle);
				if (Gatherer && Gatherer->AssignedBuildingHandle == Building.Entity)
				{
					AssignedGatherers.Emplace(NPCHandle, Gatherer);
				}
			}

			if (AssignedGatherers.IsEmpty())
			{
				ImGui::TextDisabled("No gatherers assigned to this building");
			}
			else
			{
				constexpr ImGuiTableFlags GathTableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
				if (ImGui::BeginTable("AssignedGatherers", 3, GathTableFlags))
				{
					ImGui::TableSetupColumn("NPC", ImGuiTableColumnFlags_WidthFixed, 70.f);
					ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 70.f);
					ImGui::TableSetupColumn("Carrying", ImGuiTableColumnFlags_WidthFixed, 70.f);
					ImGui::TableHeadersRow();

					for (const TPair<FMassEntityHandle, const FArcGathererFragment*>& Entry : AssignedGatherers)
					{
						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%d", Entry.Key.Index);

						ImGui::TableSetColumnIndex(1);
						if (Entry.Value->TargetResourceHandle.IsValid())
						{
							ImGui::Text("%d", Entry.Value->TargetResourceHandle.Index);
						}
						else
						{
							ImGui::Text("---");
						}

						ImGui::TableSetColumnIndex(2);
						ImGui::Text("%s", Entry.Value->bCarryingResource ? "Yes" : "No");
					}

					ImGui::EndTable();
				}
			}
		}
		else
		{
			// === Non-gathering building diagnostics ===

			// 2. Recipe assigned
			bool bAnyRecipeAssigned = false;
			for (int32 SlotIdx = 0; SlotIdx < Workforce.Slots.Num(); ++SlotIdx)
			{
				if (Workforce.Slots[SlotIdx].DesiredRecipe != nullptr)
				{
					bAnyRecipeAssigned = true;
					break;
				}
			}
			if (bAnyRecipeAssigned)
			{
				ImGui::TextColored(GreenColor, "[OK] Recipe assigned");
			}
			else
			{
				ImGui::TextColored(RedColor, "[!!] No recipe assigned to any slot");
			}

			// 3. Workers staffed
			{
				UWorld* World = Arcx::GameplayDebugger::Economy::GetDebugWorld();
				UArcAreaSubsystem* AreaSub = World ? World->GetSubsystem<UArcAreaSubsystem>() : nullptr;
				FStructView AreaView = Manager->GetFragmentDataStruct(Building.Entity, FArcAreaFragment::StaticStruct());

				int32 TotalRequiredWorkers = 0;
				for (int32 SlotIdx = 0; SlotIdx < Workforce.Slots.Num(); ++SlotIdx)
				{
					if (Workforce.Slots[SlotIdx].DesiredRecipe != nullptr)
					{
						TotalRequiredWorkers += Workforce.Slots[SlotIdx].RequiredWorkerCount;
					}
				}

				if (!AreaSub || !AreaView.IsValid())
				{
					ImGui::TextColored(YellowColor, "[--] Workers staffed: N/A (no area subsystem or fragment)");
				}
				else
				{
					const FArcAreaFragment& AreaFrag = AreaView.Get<FArcAreaFragment>();
					const FArcAreaData* AreaData = AreaSub->GetAreaData(AreaFrag.AreaHandle);

					int32 ActiveWorkers = 0;
					if (AreaData)
					{
						int32 AreaSlotBase = 0;
						for (int32 SlotIdx = 0; SlotIdx < Workforce.Slots.Num(); ++SlotIdx)
						{
							const FArcBuildingSlotData& Slot = Workforce.Slots[SlotIdx];
							if (Slot.DesiredRecipe != nullptr)
							{
								for (int32 w = 0; w < Slot.RequiredWorkerCount; ++w)
								{
									FArcAreaSlotHandle SlotHandle(AreaFrag.AreaHandle, AreaSlotBase + w);
									if (AreaSub->GetSlotState(SlotHandle) == EArcAreaSlotState::Active)
									{
										++ActiveWorkers;
									}
								}
							}
							AreaSlotBase += Slot.RequiredWorkerCount;
						}
					}

					if (ActiveWorkers >= TotalRequiredWorkers && TotalRequiredWorkers > 0)
					{
						ImGui::TextColored(GreenColor, "[OK] Workers staffed (%d / %d)", ActiveWorkers, TotalRequiredWorkers);
					}
					else if (TotalRequiredWorkers == 0)
					{
						ImGui::TextColored(YellowColor, "[--] Workers staffed: N/A (no recipe requires workers)");
					}
					else
					{
						ImGui::TextColored(RedColor, "[!!] Understaffed (%d / %d active)", ActiveWorkers, TotalRequiredWorkers);
					}
				}
			}

			// 4. Inputs available
			{
				FStructView InputView2 = Manager->GetFragmentDataStruct(Building.Entity, FArcCraftInputFragment::StaticStruct());
				if (!InputView2.IsValid())
				{
					ImGui::TextColored(YellowColor, "[--] Inputs available: N/A (no input fragment)");
				}
				else
				{
					const FArcCraftInputFragment& InputFrag = InputView2.Get<FArcCraftInputFragment>();
					bool bAllInputsOk = true;
					bool bCheckedAnySlot = false;

					for (int32 SlotIdx = 0; SlotIdx < Workforce.Slots.Num(); ++SlotIdx)
					{
						const FArcBuildingSlotData& Slot = Workforce.Slots[SlotIdx];
						if (!Slot.DesiredRecipe)
						{
							continue;
						}

						bCheckedAnySlot = true;

						if (Slot.DesiredRecipe->GetIngredientCount() == 0)
						{
							continue;
						}

						TArray<FArcItemSpec> ItemsCopy = InputFrag.InputItems;
						const bool bCanMatch = FArcCraftItemSource::MatchAndConsumeFromSpecs(ItemsCopy, Slot.DesiredRecipe, false);
						if (!bCanMatch)
						{
							bAllInputsOk = false;
							break;
						}
					}

					if (!bCheckedAnySlot)
					{
						ImGui::TextColored(YellowColor, "[--] Inputs available: N/A (no slot has a recipe)");
					}
					else if (bAllInputsOk)
					{
						ImGui::TextColored(GreenColor, "[OK] Inputs available");
					}
					else
					{
						ImGui::TextColored(RedColor, "[!!] Missing input ingredients");
					}
				}
			}

			// 5. Output buffer
			if (BuildingFrag)
			{
				const bool bBufferFull = (BuildingFrag->CurrentOutputCount >= EconConfig->OutputBufferSize);
				if (!bBufferFull)
				{
					ImGui::TextColored(GreenColor, "[OK] Output buffer (%d / %d)", BuildingFrag->CurrentOutputCount, EconConfig->OutputBufferSize);
				}
				else
				{
					ImGui::TextColored(RedColor, "[!!] Output buffer FULL (%d / %d)", BuildingFrag->CurrentOutputCount, EconConfig->OutputBufferSize);
				}
			}
			else
			{
				ImGui::TextColored(YellowColor, "[--] Output buffer: N/A (no building fragment)");
			}

			// 6. Settlement storage
			{
				const FArcSettlementMarketFragment* MarketFrag = Manager->GetFragmentDataPtr<FArcSettlementMarketFragment>(Settlement.Entity);
				if (!MarketFrag)
				{
					ImGui::TextColored(YellowColor, "[--] Settlement storage: N/A (no market fragment)");
				}
				else
				{
					const bool bStorageFull = (MarketFrag->CurrentTotalStorage >= MarketFrag->TotalStorageCap);
					if (!bStorageFull)
					{
						ImGui::TextColored(GreenColor, "[OK] Settlement storage (%d / %d)", MarketFrag->CurrentTotalStorage, MarketFrag->TotalStorageCap);
					}
					else
					{
						ImGui::TextColored(RedColor, "[!!] Settlement storage FULL (%d / %d)", MarketFrag->CurrentTotalStorage, MarketFrag->TotalStorageCap);
					}
				}
			}
		}
	}
#endif
}

void FArcEconomyDebugger::DrawAreaSlotSection()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedBuildingIdx == INDEX_NONE || !CachedBuildings.IsValidIndex(SelectedBuildingIdx))
	{
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	UWorld* World = Arcx::GameplayDebugger::Economy::GetDebugWorld();
	if (!Manager || !World)
	{
		return;
	}

	const FBuildingEntry& Building = CachedBuildings[SelectedBuildingIdx];
	if (!Manager->IsEntityActive(Building.Entity))
	{
		return;
	}

	UArcAreaSubsystem* AreaSub = World->GetSubsystem<UArcAreaSubsystem>();
	if (!AreaSub)
	{
		return;
	}

	const FArcAreaFragment* AreaFrag = Manager->GetFragmentDataPtr<FArcAreaFragment>(Building.Entity);
	if (!AreaFrag || !AreaFrag->AreaHandle.IsValid())
	{
		return;
	}

	const FArcAreaData* AreaData = AreaSub->GetAreaData(AreaFrag->AreaHandle);
	if (!AreaData || AreaData->Slots.IsEmpty())
	{
		return;
	}

	ImGui::SeparatorText("Area Slots");

	constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("AreaSlotTable", 4, TableFlags))
	{
		ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.f);
		ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.f);
		ImGui::TableSetupColumn("Activity Tags", ImGuiTableColumnFlags_WidthFixed, 200.f);
		ImGui::TableSetupColumn("Assigned NPC", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		for (int32 SlotIdx = 0; SlotIdx < AreaData->Slots.Num(); ++SlotIdx)
		{
			const FArcAreaSlotRuntime& Slot = AreaData->Slots[SlotIdx];

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%d", SlotIdx);

			ImGui::TableSetColumnIndex(1);
			ImVec4 StateColor;
			const char* StateName;
			switch (Slot.State)
			{
			case EArcAreaSlotState::Vacant:   StateColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); StateName = "Vacant"; break;
			case EArcAreaSlotState::Assigned: StateColor = ImVec4(0.8f, 0.6f, 0.2f, 1.0f); StateName = "Assigned"; break;
			case EArcAreaSlotState::Active:   StateColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f); StateName = "Active"; break;
			case EArcAreaSlotState::Disabled: StateColor = ImVec4(0.6f, 0.2f, 0.2f, 1.0f); StateName = "Disabled"; break;
			default:                          StateColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); StateName = "Unknown"; break;
			}
			ImGui::TextColored(StateColor, "%s", StateName);

			ImGui::TableSetColumnIndex(2);
			if (!Slot.ActivityTags.IsEmpty())
			{
				ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s", TCHAR_TO_ANSI(*Slot.ActivityTags.ToStringSimple()));
			}
			else
			{
				ImGui::TextDisabled("(none)");
			}

			ImGui::TableSetColumnIndex(3);
			if (Slot.AssignedEntity.IsValid() && Manager->IsEntityActive(Slot.AssignedEntity))
			{
				const FArcEconomyNPCFragment* NPC = Manager->GetFragmentDataPtr<FArcEconomyNPCFragment>(Slot.AssignedEntity);
				if (NPC)
				{
					const char* RoleName = "?";
					switch (NPC->Role)
					{
					case EArcEconomyNPCRole::Idle:        RoleName = "Idle"; break;
					case EArcEconomyNPCRole::Worker:      RoleName = "Worker"; break;
					case EArcEconomyNPCRole::Transporter: RoleName = "Transporter"; break;
					case EArcEconomyNPCRole::Gatherer:    RoleName = "Gatherer"; break;
					}
					ImGui::Text("Entity %d [%s]", Slot.AssignedEntity.Index, RoleName);
				}
				else
				{
					ImGui::Text("Entity %d", Slot.AssignedEntity.Index);
				}
			}
			else if (Slot.State == EArcAreaSlotState::Vacant)
			{
				ImGui::TextDisabled("---");
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "(invalid entity)");
			}
		}

		ImGui::EndTable();
	}
#endif
}

void FArcEconomyDebugger::RefreshSmartObjectSlots()
{
#if WITH_MASSENTITY_DEBUG
	CachedSOSlots.Reset();

	if (SelectedBuildingIdx == INDEX_NONE || !CachedBuildings.IsValidIndex(SelectedBuildingIdx))
	{
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	UWorld* World = Arcx::GameplayDebugger::Economy::GetDebugWorld();
	if (!Manager || !World)
	{
		return;
	}

	const FBuildingEntry& Building = CachedBuildings[SelectedBuildingIdx];
	if (!Manager->IsEntityActive(Building.Entity))
	{
		return;
	}

	const USmartObjectSubsystem* SOSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
	if (!SOSubsystem)
	{
		return;
	}

	// Try area path first: FArcAreaFragment -> FArcAreaData -> SmartObjectHandle
	FSmartObjectHandle SOHandle;

	FStructView AreaView = Manager->GetFragmentDataStruct(Building.Entity, FArcAreaFragment::StaticStruct());
	if (AreaView.IsValid())
	{
		const FArcAreaFragment& AreaFrag = AreaView.Get<FArcAreaFragment>();
		UArcAreaSubsystem* AreaSub = World->GetSubsystem<UArcAreaSubsystem>();
		if (AreaSub)
		{
			const FArcAreaData* AreaData = AreaSub->GetAreaData(AreaFrag.AreaHandle);
			if (AreaData)
			{
				SOHandle = AreaData->SmartObjectHandle;
			}
		}
	}

	// Fallback: FArcSmartObjectOwnerFragment
	if (!SOHandle.IsValid())
	{
		FStructView OwnerView = Manager->GetFragmentDataStruct(Building.Entity, FArcSmartObjectOwnerFragment::StaticStruct());
		if (OwnerView.IsValid())
		{
			SOHandle = OwnerView.Get<FArcSmartObjectOwnerFragment>().SmartObjectHandle;
		}
	}

	if (!SOHandle.IsValid())
	{
		return;
	}

	TArray<FSmartObjectSlotHandle> AllSlots;
	SOSubsystem->GetAllSlots(SOHandle, AllSlots);

	for (int32 i = 0; i < AllSlots.Num(); ++i)
	{
		const FSmartObjectSlotHandle& SlotHandle = AllSlots[i];

		FSOSlotEntry& Entry = CachedSOSlots.AddDefaulted_GetRef();
		Entry.SlotHandle = SlotHandle;
		Entry.State = static_cast<uint8>(SOSubsystem->GetSlotState(SlotHandle));
		Entry.RuntimeTags = SOSubsystem->GetSlotTags(SlotHandle);

		TOptional<FTransform> SlotTransform = SOSubsystem->GetSlotTransform(SlotHandle);
		if (SlotTransform.IsSet())
		{
			Entry.Location = SlotTransform->GetLocation();
		}

		SOSubsystem->ReadSlotData(SlotHandle, [&Entry](FConstSmartObjectSlotView SlotView)
		{
			SlotView.GetActivityTags(Entry.ActivityTags);
			Entry.bEnabled = SlotView.GetDefinition().bEnabled;
		});
	}
#endif
}

void FArcEconomyDebugger::DrawSmartObjectSlotSection()
{
#if WITH_MASSENTITY_DEBUG
	if (CachedSOSlots.IsEmpty())
	{
		return;
	}

	// Get governor slot role mapping for classification
	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	const TMap<EArcEconomyNPCRole, FGameplayTagContainer>* SlotRoleMappingPtr = nullptr;
	ArcGovernor::FArcBuildingSlotSummary SlotSummary;
	bool bHasClassification = false;

	if (Manager && SelectedSettlementIdx != INDEX_NONE && CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		const FSettlementEntry& Settlement = CachedSettlements[SelectedSettlementIdx];
		FStructView GovernorView = Manager->GetFragmentDataStruct(Settlement.Entity, FArcGovernorFragment::StaticStruct());
		if (GovernorView.IsValid())
		{
			const FArcGovernorFragment& Governor = GovernorView.Get<FArcGovernorFragment>();
			if (Governor.GovernorConfig && !Governor.GovernorConfig->SlotRoleMapping.IsEmpty())
			{
				SlotRoleMappingPtr = &Governor.GovernorConfig->SlotRoleMapping;

				// Classify building slots
				const FBuildingEntry& Building = CachedBuildings[SelectedBuildingIdx];
				const FArcSmartObjectDefinitionSharedFragment* SODef =
					Manager->GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>(Building.Entity);
				if (SODef && SODef->SmartObjectDefinition)
				{
					SlotSummary = ArcGovernor::ClassifyBuildingSlots(*SODef->SmartObjectDefinition, *SlotRoleMappingPtr);

					// Count assigned workers/transporters for this building
					UArcSettlementSubsystem* SettlementSub = Arcx::GameplayDebugger::Economy::GetSettlementSubsystem();
					if (SettlementSub)
					{
						const TArray<FMassEntityHandle>& NPCHandles = SettlementSub->GetNPCsForSettlement(Settlement.Entity);
						int32 AssignedWorkers = 0;
						for (const FMassEntityHandle& NPCHandle : NPCHandles)
						{
							if (!Manager->IsEntityActive(NPCHandle))
							{
								continue;
							}
							const FArcEconomyNPCFragment* NPC = Manager->GetFragmentDataPtr<FArcEconomyNPCFragment>(NPCHandle);
							if (!NPC || NPC->Role != EArcEconomyNPCRole::Worker)
							{
								continue;
							}
							const FArcWorkerFragment* Worker = Manager->GetFragmentDataPtr<FArcWorkerFragment>(NPCHandle);
							if (Worker && Worker->AssignedBuildingHandle == Building.Entity)
							{
								++AssignedWorkers;
							}
						}
						SlotSummary.FreeWorkerSlots = FMath::Max(0, SlotSummary.TotalWorkerSlots - AssignedWorkers);
						// Transport slots: transporters are free-roaming, not assigned per building
						bHasClassification = true;
					}
				}
			}
		}
	}

	ImGui::SeparatorText("Smart Object Slots");

	// Slot classification summary
	if (bHasClassification)
	{
		const int32 AssignedWorkers = SlotSummary.TotalWorkerSlots - SlotSummary.FreeWorkerSlots;

		ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Workers: %d/%d", AssignedWorkers, SlotSummary.TotalWorkerSlots);
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.2f, 1.0f), "  Transport slots: %d", SlotSummary.TotalTransportSlots);
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "  Gatherer slots: %d", SlotSummary.TotalGathererSlots);

		const int32 UnclassifiedSlots = CachedSOSlots.Num() - SlotSummary.TotalWorkerSlots - SlotSummary.TotalTransportSlots - SlotSummary.TotalGathererSlots;
		if (UnclassifiedSlots > 0)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("  Unclassified: %d", UnclassifiedSlots);
		}
	}
	else
	{
		ImGui::TextDisabled("No SlotRoleMapping configured on governor");
	}

	constexpr ImGuiTableFlags SOTableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;
	if (ImGui::BeginTable("SOSlotTable", 6, SOTableFlags))
	{
		ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.f);
		ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 80.f);
		ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 70.f);
		ImGui::TableSetupColumn("Activity Tags", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Runtime Tags", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthFixed, 200.f);
		ImGui::TableHeadersRow();

		for (int32 i = 0; i < CachedSOSlots.Num(); ++i)
		{
			const FSOSlotEntry& Slot = CachedSOSlots[i];
			ESmartObjectSlotState SOState = static_cast<ESmartObjectSlotState>(Slot.State);

			ImGui::TableNextRow();

			// Index
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%d", i);

			// Role (classify this slot against governor mapping)
			ImGui::TableSetColumnIndex(1);
			if (SlotRoleMappingPtr)
			{
				const char* RoleName = "---";
				ImVec4 RoleColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
				for (const TPair<EArcEconomyNPCRole, FGameplayTagContainer>& Mapping : *SlotRoleMappingPtr)
				{
					if (Mapping.Key == EArcEconomyNPCRole::Idle)
					{
						continue;
					}
					if (Slot.ActivityTags.HasAll(Mapping.Value))
					{
						if (Mapping.Key == EArcEconomyNPCRole::Worker)
						{
							RoleName = "Worker";
							RoleColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
						}
						else if (Mapping.Key == EArcEconomyNPCRole::Transporter)
						{
							RoleName = "Transport";
							RoleColor = ImVec4(1.0f, 1.0f, 0.2f, 1.0f);
						}
						else if (Mapping.Key == EArcEconomyNPCRole::Gatherer)
						{
							RoleName = "Gatherer";
							RoleColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
						}
						break;
					}
				}
				ImGui::TextColored(RoleColor, "%s", RoleName);
			}
			else
			{
				ImGui::TextDisabled("---");
			}

			// State
			ImGui::TableSetColumnIndex(2);
			ImVec4 StateColor;
			const char* StateName;
			switch (SOState)
			{
			case ESmartObjectSlotState::Free:     StateColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); StateName = "Free"; break;
			case ESmartObjectSlotState::Claimed:  StateColor = ImVec4(0.8f, 0.6f, 0.2f, 1.0f); StateName = "Claimed"; break;
			case ESmartObjectSlotState::Occupied: StateColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f); StateName = "Occupied"; break;
			default:                              StateColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); StateName = "Invalid"; break;
			}
			if (!Slot.bEnabled)
			{
				StateColor = ImVec4(0.6f, 0.2f, 0.2f, 1.0f);
				StateName = "Disabled";
			}
			ImGui::TextColored(StateColor, "%s", StateName);

			// Activity Tags
			ImGui::TableSetColumnIndex(3);
			if (!Slot.ActivityTags.IsEmpty())
			{
				ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s", TCHAR_TO_ANSI(*Slot.ActivityTags.ToStringSimple()));
			}
			else
			{
				ImGui::TextDisabled("(none)");
			}

			// Runtime Tags
			ImGui::TableSetColumnIndex(4);
			if (!Slot.RuntimeTags.IsEmpty())
			{
				ImGui::Text("%s", TCHAR_TO_ANSI(*Slot.RuntimeTags.ToStringSimple()));
			}
			else
			{
				ImGui::TextDisabled("(none)");
			}

			// Location
			ImGui::TableSetColumnIndex(5);
			ImGui::Text("%.0f, %.0f, %.0f", Slot.Location.X, Slot.Location.Y, Slot.Location.Z);
		}

		ImGui::EndTable();
	}
#endif
}
