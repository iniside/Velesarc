// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebugger.h"

#include "imgui.h"
#include "MassEntitySubsystem.h"
#include "MassDebugger.h"
#include "MassActorSubsystem.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcSettlementSubsystem.h"
#include "ArcEconomyTypes.h"
#include "Items/ArcItemDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Mass/ArcDebugFragments.h"

namespace Arcx::GameplayDebugger::Economy
{
	extern UWorld* GetDebugWorld();
	extern FMassEntityManager* GetEntityManager();
	extern UArcSettlementSubsystem* GetSettlementSubsystem();
	extern const ImVec4 DimColor;

	const char* NPCRoleToString(EArcEconomyNPCRole Role)
	{
		switch (Role)
		{
		case EArcEconomyNPCRole::Idle:        return "Idle";
		case EArcEconomyNPCRole::Worker:      return "Worker";
		case EArcEconomyNPCRole::Transporter: return "Transporter";
		case EArcEconomyNPCRole::Gatherer:    return "Gatherer";
		default:                              return "Unknown";
		}
	}

	const char* TransporterStateToString(EArcTransporterTaskState State)
	{
		switch (State)
		{
		case EArcTransporterTaskState::Idle:       return "Idle";
		case EArcTransporterTaskState::SeekingTask: return "Seeking";
		case EArcTransporterTaskState::PickingUp:  return "PickingUp";
		case EArcTransporterTaskState::Delivering: return "Delivering";
		default:                                   return "Unknown";
		}
	}

	ImVec4 NPCRoleColor(EArcEconomyNPCRole Role)
	{
		switch (Role)
		{
		case EArcEconomyNPCRole::Worker:      return ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
		case EArcEconomyNPCRole::Transporter: return ImVec4(1.0f, 1.0f, 0.2f, 1.0f);
		case EArcEconomyNPCRole::Gatherer:    return ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
		default:                              return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
		}
	}
} // namespace Arcx::GameplayDebugger::Economy

void FArcEconomyDebugger::RefreshNPCList()
{
#if WITH_MASSENTITY_DEBUG
	// Preserve selection across refresh
	FMassEntityHandle PreviouslySelectedNPC;
	if (SelectedNPCIdx != INDEX_NONE && CachedNPCs.IsValidIndex(SelectedNPCIdx))
	{
		PreviouslySelectedNPC = CachedNPCs[SelectedNPCIdx].Entity;
	}

	CachedNPCs.Reset();
	SelectedNPCIdx = INDEX_NONE;

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
	const TArray<FMassEntityHandle>& NPCHandles = SettlementSub->GetNPCsForSettlement(Settlement.Entity);

	for (const FMassEntityHandle& Handle : NPCHandles)
	{
		if (!Manager->IsEntityActive(Handle))
		{
			continue;
		}

		FNPCEntry& Entry = CachedNPCs.AddDefaulted_GetRef();
		Entry.Entity = Handle;

		// Economy NPC fragment
		FStructView NPCView = Manager->GetFragmentDataStruct(Handle, FArcEconomyNPCFragment::StaticStruct());
		if (NPCView.IsValid())
		{
			const FArcEconomyNPCFragment& NPCFrag = NPCView.Get<FArcEconomyNPCFragment>();
			Entry.Role = static_cast<uint8>(NPCFrag.Role);
		}

		// Worker fragment
		FStructView WorkerView = Manager->GetFragmentDataStruct(Handle, FArcWorkerFragment::StaticStruct());
		if (WorkerView.IsValid())
		{
			const FArcWorkerFragment& WorkerFrag = WorkerView.Get<FArcWorkerFragment>();
			Entry.AssignedBuilding = WorkerFrag.AssignedBuildingHandle;
		}

		// Gatherer fragment (only read when role is Gatherer to avoid clobbering worker's AssignedBuilding)
		if (Entry.Role == static_cast<uint8>(EArcEconomyNPCRole::Gatherer))
		{
			FStructView GathererView = Manager->GetFragmentDataStruct(Handle, FArcGathererFragment::StaticStruct());
			if (GathererView.IsValid())
			{
				const FArcGathererFragment& GathererFrag = GathererView.Get<FArcGathererFragment>();
				Entry.AssignedBuilding = GathererFrag.AssignedBuildingHandle;
				Entry.GathererTargetResource = GathererFrag.TargetResourceHandle;
				Entry.bCarryingResource = GathererFrag.bCarryingResource;
			}
		}

		// Transporter fragment
		FStructView TransporterView = Manager->GetFragmentDataStruct(Handle, FArcTransporterFragment::StaticStruct());
		if (TransporterView.IsValid())
		{
			const FArcTransporterFragment& TransFrag = TransporterView.Get<FArcTransporterFragment>();
			Entry.TransporterState = static_cast<uint8>(TransFrag.TaskState);
			Entry.TargetItemName = TransFrag.TargetItemDefinition ? TransFrag.TargetItemDefinition->GetName() : TEXT("");
			Entry.TargetQuantity = TransFrag.TargetQuantity;
		}

		// Try actor name
		FStructView ActorView = Manager->GetFragmentDataStruct(Handle, FMassActorFragment::StaticStruct());
		if (ActorView.IsValid())
		{
			const FMassActorFragment& ActorFrag = ActorView.Get<FMassActorFragment>();
			if (const AActor* Actor = ActorFrag.Get())
			{
				Entry.Label = Actor->GetActorNameOrLabel();
			}
		}

		if (Entry.Label.IsEmpty())
		{
			Entry.Label = Handle.DebugGetDescription();
		}
	}

	// Restore selection by entity handle
	if (PreviouslySelectedNPC.IsValid())
	{
		for (int32 i = 0; i < CachedNPCs.Num(); ++i)
		{
			if (CachedNPCs[i].Entity == PreviouslySelectedNPC)
			{
				SelectedNPCIdx = i;
				break;
			}
		}
	}
#endif
}

void FArcEconomyDebugger::DrawWorkforceTab()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedSettlementIdx == INDEX_NONE || !CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FSettlementEntry& Settlement = CachedSettlements[SelectedSettlementIdx];

	ImGui::SeparatorText("Workforce");
	ImGui::Text("Total: %d  |  W:%d  T:%d  G:%d  C:%d  I:%d",
		CachedNPCs.Num(), Settlement.WorkerCount, Settlement.TransporterCount,
		Settlement.GathererCount, Settlement.CaravanCount, Settlement.IdleCount);

	if (CachedNPCs.IsEmpty())
	{
		ImGui::TextDisabled("No NPCs in this settlement");
		return;
	}

	const float AvailableWidth = ImGui::GetContentRegionAvail().x;
	const float AvailableHeight = ImGui::GetContentRegionAvail().y;
	const float TableWidth = AvailableWidth * 0.55f;
	const float DetailWidth = AvailableWidth - TableWidth - ImGui::GetStyle().ItemSpacing.x;

	// --- Left column: NPC table in a scrollable child ---
	if (ImGui::BeginChild("NPCTableChild", ImVec2(TableWidth, AvailableHeight), ImGuiChildFlags_None))
	{
		constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

		if (ImGui::BeginTable("NPCTable", 4, TableFlags))
		{
			ImGui::TableSetupColumn("NPC", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 120.f);
			ImGui::TableSetupColumn("Assignment", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 180.f);
			ImGui::TableHeadersRow();

			for (int32 i = 0; i < CachedNPCs.Num(); ++i)
			{
				FNPCEntry& NPC = CachedNPCs[i];
				if (!Manager->IsEntityActive(NPC.Entity))
				{
					continue;
				}

				EArcEconomyNPCRole Role = static_cast<EArcEconomyNPCRole>(NPC.Role);

				ImGui::TableNextRow();

				// Name
				ImGui::TableSetColumnIndex(0);
				const bool bNPCSelected = (SelectedNPCIdx == i);
				FString NPCSelectLabel = FString::Printf(TEXT("%s##npc%d"), *NPC.Label, i);
				if (ImGui::Selectable(TCHAR_TO_ANSI(*NPCSelectLabel), bNPCSelected))
				{
					SelectedNPCIdx = i;
					SelectedStateFrameIdx = INDEX_NONE;
					SelectedStateIdx = MAX_uint16;
				}
				ImGui::SameLine();
				ImGui::TextColored(Arcx::GameplayDebugger::Economy::DimColor, "[%s]", TCHAR_TO_ANSI(*NPC.Entity.DebugGetDescription()));

				// Role
				ImGui::TableSetColumnIndex(1);
				{
					FString ComboId = FString::Printf(TEXT("##role_%d"), i);
					ImGui::PushItemWidth(-1);
					if (ImGui::BeginCombo(TCHAR_TO_ANSI(*ComboId), Arcx::GameplayDebugger::Economy::NPCRoleToString(Role)))
					{
						for (uint8 RoleIdx = 0; RoleIdx < 4; ++RoleIdx)
						{
							EArcEconomyNPCRole OptionRole = static_cast<EArcEconomyNPCRole>(RoleIdx);
							const bool bIsSelected = (Role == OptionRole);
							if (ImGui::Selectable(Arcx::GameplayDebugger::Economy::NPCRoleToString(OptionRole), bIsSelected))
							{
								// Push role change through debug command queue
								FArcDebugCommandQueueFragment* DebugQueue = Manager->GetFragmentDataPtr<FArcDebugCommandQueueFragment>(Settlement.Entity);
								if (!DebugQueue)
								{
									// Lazily add the fragment
									Manager->AddFragmentToEntity(Settlement.Entity, FArcDebugCommandQueueFragment::StaticStruct());
									DebugQueue = Manager->GetFragmentDataPtr<FArcDebugCommandQueueFragment>(Settlement.Entity);
								}
								if (DebugQueue)
								{
									ArcGovernor::FArcGovernorPendingNPCChange& Change = DebugQueue->PendingNPCChanges.AddDefaulted_GetRef();
									Change.NPCHandle = NPC.Entity;
									Change.NewRole = OptionRole;
									NPC.Role = RoleIdx; // Update cached display
								}
							}
						}
						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();
				}

				// Assignment detail
				ImGui::TableSetColumnIndex(2);
				switch (Role)
				{
				case EArcEconomyNPCRole::Worker:
				{
					if (Manager->IsEntityActive(NPC.AssignedBuilding))
					{
						// Resolve building name
						FStructView BuildView = Manager->GetFragmentDataStruct(NPC.AssignedBuilding, FArcBuildingFragment::StaticStruct());
						if (BuildView.IsValid())
						{
							const FArcBuildingFragment& BuildFrag = BuildView.Get<FArcBuildingFragment>();
							FString BuildName = BuildFrag.BuildingName.IsNone() ? TEXT("(unknown)") : BuildFrag.BuildingName.ToString();
							ImGui::Text("-> %s", TCHAR_TO_ANSI(*BuildName));
						}
					}
					else
					{
						ImGui::TextColored(Arcx::GameplayDebugger::Economy::DimColor, "Unassigned");
					}
					break;
				}
				case EArcEconomyNPCRole::Transporter:
				{
					EArcTransporterTaskState TaskState = static_cast<EArcTransporterTaskState>(NPC.TransporterState);
					FString StateStr = FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(Arcx::GameplayDebugger::Economy::TransporterStateToString(TaskState)));
					if (!NPC.TargetItemName.IsEmpty())
					{
						StateStr += FString::Printf(TEXT(" | %s x%d"), *NPC.TargetItemName, NPC.TargetQuantity);
					}
					ImGui::Text("%s", TCHAR_TO_ANSI(*StateStr));
					break;
				}
				case EArcEconomyNPCRole::Gatherer:
				{
					if (Manager->IsEntityActive(NPC.AssignedBuilding))
					{
						FStructView BuildView = Manager->GetFragmentDataStruct(NPC.AssignedBuilding, FArcBuildingFragment::StaticStruct());
						if (BuildView.IsValid())
						{
							const FArcBuildingFragment& BuildFrag = BuildView.Get<FArcBuildingFragment>();
							FString BuildName = BuildFrag.BuildingName.IsNone() ? TEXT("(unknown)") : BuildFrag.BuildingName.ToString();
							FString GatherInfo = FString::Printf(TEXT("-> %s %s"),
								*BuildName,
								NPC.bCarryingResource ? TEXT("[carrying]") : (NPC.GathererTargetResource.IsValid() ? TEXT("[gathering]") : TEXT("[no target]")));
							ImGui::Text("%s", TCHAR_TO_ANSI(*GatherInfo));
						}
					}
					else
					{
						ImGui::TextColored(Arcx::GameplayDebugger::Economy::DimColor, "Unassigned");
					}
					break;
				}
				default:
					ImGui::TextColored(Arcx::GameplayDebugger::Economy::DimColor, "---");
					break;
				}

				// Actions
				ImGui::TableSetColumnIndex(3);

				if (Role == EArcEconomyNPCRole::Worker)
				{
					// Building assignment combo
					FString AssignComboId = FString::Printf(TEXT("##assign_%d"), i);
					FString CurrentBuilding = TEXT("(none)");
					if (Manager->IsEntityActive(NPC.AssignedBuilding))
					{
						FStructView BView = Manager->GetFragmentDataStruct(NPC.AssignedBuilding, FArcBuildingFragment::StaticStruct());
						if (BView.IsValid())
						{
							const FArcBuildingFragment& BFrag = BView.Get<FArcBuildingFragment>();
							CurrentBuilding = BFrag.BuildingName.IsNone() ? TEXT("(unknown)") : BFrag.BuildingName.ToString();
						}
					}

					ImGui::PushItemWidth(-1);
					if (ImGui::BeginCombo(TCHAR_TO_ANSI(*AssignComboId), TCHAR_TO_ANSI(*CurrentBuilding)))
					{
						// Unassign option
						if (ImGui::Selectable("(unassign)"))
						{
							FStructView WView = Manager->GetFragmentDataStruct(NPC.Entity, FArcWorkerFragment::StaticStruct());
							if (WView.IsValid())
							{
								FArcWorkerFragment& WFrag = WView.Get<FArcWorkerFragment>();
								WFrag.AssignedBuildingHandle = FMassEntityHandle();
								NPC.AssignedBuilding = FMassEntityHandle();
							}
						}

						for (const FBuildingEntry& Building : CachedBuildings)
						{
							if (!Manager->IsEntityActive(Building.Entity))
							{
								continue;
							}
							const bool bIsSelected = (NPC.AssignedBuilding == Building.Entity);
							if (ImGui::Selectable(TCHAR_TO_ANSI(*Building.DefinitionName), bIsSelected))
							{
								FStructView WView = Manager->GetFragmentDataStruct(NPC.Entity, FArcWorkerFragment::StaticStruct());
								if (WView.IsValid())
								{
									FArcWorkerFragment& WFrag = WView.Get<FArcWorkerFragment>();
									WFrag.AssignedBuildingHandle = Building.Entity;
									NPC.AssignedBuilding = Building.Entity;
								}
							}
						}
						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();
				}
			}

			ImGui::EndTable();
		}
	}
	ImGui::EndChild();

	// --- Right column: NPC detail (state tree, inventory) ---
	ImGui::SameLine();
	if (ImGui::BeginChild("NPCDetailChild", ImVec2(DetailWidth, AvailableHeight), ImGuiChildFlags_None))
	{
		DrawNPCDetailPanel();
	}
	ImGui::EndChild();
#endif
}
