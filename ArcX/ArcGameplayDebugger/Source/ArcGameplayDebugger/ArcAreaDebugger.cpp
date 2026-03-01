// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaDebugger.h"

#include "imgui.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "ArcAreaSubsystem.h"
#include "ArcAreaDefinition.h"
#include "Mass/ArcAreaAssignmentFragments.h"
#include "MassEntitySubsystem.h"
#include "MassEntityQuery.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"

// ====================================================================
// Helpers
// ====================================================================

namespace
{
	static const FColor AreaColor(80, 200, 120, 200);
	static const FColor SelectedAreaColor = FColor::Yellow;
	static const FColor VacantColor(50, 200, 50, 200);
	static const FColor AssignedColor(200, 150, 50, 200);
	static const FColor ActiveColor(50, 150, 255, 200);
	static const FColor DisabledColor(150, 50, 50, 200);

	static const ImVec4 ImAreaColor(0.3f, 0.8f, 0.5f, 1.0f);
	static const ImVec4 ImSelectedColor(1.0f, 1.0f, 0.0f, 1.0f);
	static const ImVec4 ImVacantColor(0.2f, 0.8f, 0.2f, 1.0f);
	static const ImVec4 ImAssignedColor(0.8f, 0.6f, 0.2f, 1.0f);
	static const ImVec4 ImActiveColor(0.2f, 0.6f, 1.0f, 1.0f);
	static const ImVec4 ImDisabledColor(0.6f, 0.2f, 0.2f, 1.0f);
	static const ImVec4 ImDimColor(0.5f, 0.5f, 0.5f, 1.0f);
	static const ImVec4 ImTagColor(0.5f, 0.8f, 1.0f, 1.0f);

	const FString SlotStateToString(EArcAreaSlotState State)
	{
		switch (State)
		{
		case EArcAreaSlotState::Vacant:   return "Vacant";
		case EArcAreaSlotState::Assigned: return "Assigned";
		case EArcAreaSlotState::Active:   return "Active";
		case EArcAreaSlotState::Disabled: return "Disabled";
		default:                          return "Unknown";
		}
	}

	ImVec4 SlotStateColor(EArcAreaSlotState State)
	{
		switch (State)
		{
		case EArcAreaSlotState::Vacant:   return ImVacantColor;
		case EArcAreaSlotState::Assigned: return ImAssignedColor;
		case EArcAreaSlotState::Active:   return ImActiveColor;
		case EArcAreaSlotState::Disabled: return ImDisabledColor;
		default:                          return ImDimColor;
		}
	}

	FColor SlotStateWorldColor(EArcAreaSlotState State)
	{
		switch (State)
		{
		case EArcAreaSlotState::Vacant:   return VacantColor;
		case EArcAreaSlotState::Assigned: return AssignedColor;
		case EArcAreaSlotState::Active:   return ActiveColor;
		case EArcAreaSlotState::Disabled: return DisabledColor;
		default:                          return FColor::White;
		}
	}
}

// ====================================================================
// World helpers
// ====================================================================

UWorld* FArcAreaDebugger::GetDebugWorld()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}
	return GEngine->GameViewport->GetWorld();
}

UArcAreaSubsystem* FArcAreaDebugger::GetAreaSubsystem() const
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UArcAreaSubsystem>();
}

// ====================================================================
// Lifecycle
// ====================================================================

void FArcAreaDebugger::Initialize()
{
	SelectedAreaIndex = INDEX_NONE;
	AreaFilterBuf[0] = '\0';
	EntityFilterBuf[0] = '\0';
	CachedAreas.Reset();
	CachedAssignableEntities.Reset();
	LastRefreshTime = 0.f;
	bAutoRefresh = true;
	bDrawAllAreas = true;
	bDrawSelectedDetail = true;
	bDrawLabels = true;
	bShowAssignPopup = false;
	AssignTargetSlotIndex = INDEX_NONE;
	RefreshAreaList();
}

void FArcAreaDebugger::Uninitialize()
{
	CachedAreas.Reset();
	CachedAssignableEntities.Reset();
	SelectedAreaIndex = INDEX_NONE;
}

// ====================================================================
// Refresh
// ====================================================================

void FArcAreaDebugger::RefreshAreaList()
{
	CachedAreas.Reset();

	UArcAreaSubsystem* Sub = GetAreaSubsystem();
	if (!Sub)
	{
		return;
	}

	const TMap<FArcAreaHandle, FArcAreaData>& AllAreas = Sub->GetAllAreas();

	for (const auto& Pair : AllAreas)
	{
		const FArcAreaData& Data = Pair.Value;

		FAreaListEntry& Entry = CachedAreas.AddDefaulted_GetRef();
		Entry.Handle = Data.Handle;
		Entry.DisplayName = Data.DisplayName.ToString();
		Entry.Location = Data.Location;
		Entry.AreaTags = Data.AreaTags;
		Entry.TotalSlots = Data.Slots.Num();
		Entry.bHasSmartObject = Data.SmartObjectHandle.IsValid();

		for (const FArcAreaSlotRuntime& Slot : Data.Slots)
		{
			switch (Slot.State)
			{
			case EArcAreaSlotState::Vacant:   Entry.VacantSlots++;   break;
			case EArcAreaSlotState::Assigned: Entry.AssignedSlots++; break;
			case EArcAreaSlotState::Active:   Entry.ActiveSlots++;   break;
			case EArcAreaSlotState::Disabled: Entry.DisabledSlots++; break;
			}
		}

		// Build label
		FString NamePart = Entry.DisplayName.IsEmpty() ? TEXT("Unnamed") : Entry.DisplayName;
		Entry.Label = FString::Printf(TEXT("H%u [%s] (%d slots)"),
			Data.Handle.GetValue(), *NamePart, Entry.TotalSlots);
	}
}

void FArcAreaDebugger::RefreshAssignableEntities()
{
	CachedAssignableEntities.Reset();

	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		return;
	}

	UArcAreaSubsystem* AreaSub = GetAreaSubsystem();

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	FMassExecutionContext ExecContext(EntityManager);
	FMassEntityQuery Query(EntityManager.AsShared());
	Query.AddRequirement<FArcAreaAssignmentFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddConstSharedRequirement<FArcAreaAssignmentConfigFragment>();
	Query.AddTagRequirement<FArcAreaAssignmentTag>(EMassFragmentPresence::All);

	Query.ForEachEntityChunk(ExecContext,
		[this, &EntityManager, AreaSub](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();
			const auto AssignmentList = Context.GetFragmentView<FArcAreaAssignmentFragment>();
			const auto TransformList = Context.GetFragmentView<FTransformFragment>();
			const FArcAreaAssignmentConfigFragment& Config = Context.GetConstSharedFragment<FArcAreaAssignmentConfigFragment>();

			for (int32 i = 0; i < NumEntities; ++i)
			{
				const FMassEntityHandle Entity = Context.GetEntity(i);
				const FArcAreaAssignmentFragment& Assignment = AssignmentList[i];
				const FTransformFragment& Transform = TransformList[i];

				FAssignableEntityEntry& Entry = CachedAssignableEntities.AddDefaulted_GetRef();
				Entry.Entity = Entity;
				Entry.Location = Transform.GetTransform().GetLocation();
				Entry.EligibleRoles = Config.EligibleRoles;
				Entry.bCurrentlyAssigned = Assignment.IsAssigned();
				Entry.CurrentAreaHandle = Assignment.AreaHandle;
				Entry.CurrentSlotIndex = Assignment.SlotIndex;

				// Build label
				FString RolesStr;
				if (Config.EligibleRoles.Num() > 0)
				{
					RolesStr = Config.EligibleRoles.First().ToString();
					if (Config.EligibleRoles.Num() > 1)
					{
						RolesStr += FString::Printf(TEXT(" +%d"), Config.EligibleRoles.Num() - 1);
					}
				}
				else
				{
					RolesStr = TEXT("(any)");
				}

				Entry.Label = FString::Printf(TEXT("E%d [%s]%s"),
					Entity.Index, *RolesStr,
					Entry.bCurrentlyAssigned ? TEXT(" *assigned*") : TEXT(""));
			}
		});
}

// ====================================================================
// Main Draw
// ====================================================================

void FArcAreaDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Area Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	UArcAreaSubsystem* Sub = GetAreaSubsystem();
	if (!Sub)
	{
		ImGui::TextDisabled("No UArcAreaSubsystem available");
		ImGui::End();
		return;
	}

	DrawToolbar();

	// Auto-refresh
	if (bAutoRefresh)
	{
		UWorld* World = GetDebugWorld();
		if (World)
		{
			float CurrentTime = World->GetTimeSeconds();
			if (CurrentTime - LastRefreshTime > 1.0f)
			{
				LastRefreshTime = CurrentTime;
				RefreshAreaList();
			}
		}
	}

	ImGui::Separator();

	// Split into two panes
	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.30f;

	// Left panel: area list
	if (ImGui::BeginChild("AreaListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawAreaListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel: area detail
	if (ImGui::BeginChild("AreaDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawAreaDetailPanel();
	}
	ImGui::EndChild();

	ImGui::End();

	// World visualization
	DrawWorldVisualization();

	// Popups
	DrawAssignEntityPopup();
}

// ====================================================================
// Toolbar
// ====================================================================

void FArcAreaDebugger::DrawToolbar()
{
	if (ImGui::Button("Refresh"))
	{
		RefreshAreaList();
	}
	ImGui::SameLine();
	ImGui::Text("Areas: %d", CachedAreas.Num());
	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();
	ImGui::Checkbox("Auto-Refresh", &bAutoRefresh);
	ImGui::SameLine();
	ImGui::Checkbox("Draw All", &bDrawAllAreas);
	ImGui::SameLine();
	ImGui::Checkbox("Draw Detail", &bDrawSelectedDetail);
	ImGui::SameLine();
	ImGui::Checkbox("Labels", &bDrawLabels);
}

// ====================================================================
// Area List Panel (Left)
// ====================================================================

void FArcAreaDebugger::DrawAreaListPanel()
{
	ImGui::Text("Registered Areas");
	ImGui::Separator();

	ImGui::InputText("Filter", AreaFilterBuf, IM_ARRAYSIZE(AreaFilterBuf));

	FString Filter = FString(ANSI_TO_TCHAR(AreaFilterBuf)).ToLower();

	if (ImGui::BeginChild("AreaScroll", ImVec2(0, 0)))
	{
		for (int32 i = 0; i < CachedAreas.Num(); ++i)
		{
			const FAreaListEntry& Entry = CachedAreas[i];

			if (!Filter.IsEmpty() && !Entry.Label.ToLower().Contains(Filter)
				&& !Entry.AreaTags.ToStringSimple().ToLower().Contains(Filter))
			{
				continue;
			}

			const bool bSelected = (SelectedAreaIndex == i);

			// Color based on occupancy status
			ImVec4 Color = ImAreaColor;
			if (Entry.VacantSlots == Entry.TotalSlots)
			{
				Color = ImVacantColor;
			}
			else if (Entry.VacantSlots == 0 && Entry.DisabledSlots == 0)
			{
				Color = ImActiveColor;
			}

			ImGui::PushStyleColor(ImGuiCol_Text, Color);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Entry.Label), bSelected))
			{
				SelectedAreaIndex = i;
			}
			ImGui::PopStyleColor();

			// Tooltip with quick info
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("Handle: %u", Entry.Handle.GetValue());
				ImGui::Text("Name: %s", TCHAR_TO_ANSI(*Entry.DisplayName));
				ImGui::Text("Location: (%.0f, %.0f, %.0f)", Entry.Location.X, Entry.Location.Y, Entry.Location.Z);
				ImGui::Text("Tags: %s", TCHAR_TO_ANSI(*Entry.AreaTags.ToStringSimple()));
				ImGui::Spacing();
				ImGui::TextColored(ImVacantColor, "Vacant: %d", Entry.VacantSlots);
				ImGui::TextColored(ImAssignedColor, "Assigned: %d", Entry.AssignedSlots);
				ImGui::TextColored(ImActiveColor, "Active: %d", Entry.ActiveSlots);
				if (Entry.DisabledSlots > 0)
				{
					ImGui::TextColored(ImDisabledColor, "Disabled: %d", Entry.DisabledSlots);
				}
				if (Entry.bHasSmartObject)
				{
					ImGui::TextColored(ImTagColor, "Has SmartObject");
				}
				ImGui::EndTooltip();
			}
		}
	}
	ImGui::EndChild();
}

// ====================================================================
// Area Detail Panel (Right)
// ====================================================================

void FArcAreaDebugger::DrawAreaDetailPanel()
{
	if (SelectedAreaIndex == INDEX_NONE || !CachedAreas.IsValidIndex(SelectedAreaIndex))
	{
		ImGui::TextDisabled("Select an area from the list");
		return;
	}

	UArcAreaSubsystem* Sub = GetAreaSubsystem();
	if (!Sub)
	{
		return;
	}

	const FAreaListEntry& ListEntry = CachedAreas[SelectedAreaIndex];
	const FArcAreaData* AreaData = Sub->GetAreaData(ListEntry.Handle);

	if (!AreaData)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Area no longer exists (H%u)", ListEntry.Handle.GetValue());
		return;
	}

	// Header
	ImGui::TextColored(ImSelectedColor, "Area H%u", AreaData->Handle.GetValue());
	ImGui::SameLine();
	if (!AreaData->DisplayName.IsEmpty())
	{
		ImGui::Text("- %s", TCHAR_TO_ANSI(*AreaData->DisplayName.ToString()));
	}
	ImGui::Spacing();

	// Basic info
	ImGui::Text("Location: (%.1f, %.1f, %.1f)", AreaData->Location.X, AreaData->Location.Y, AreaData->Location.Z);

	if (AreaData->DefinitionId.IsValid())
	{
		ImGui::Text("Definition: %s", TCHAR_TO_ANSI(*AreaData->DefinitionId.ToString()));
	}

	if (AreaData->OwnerEntity.IsValid())
	{
		ImGui::Text("Owner Entity: E%d (SN:%d)", AreaData->OwnerEntity.Index, AreaData->OwnerEntity.SerialNumber);
	}
	else
	{
		ImGui::TextDisabled("Owner Entity: None");
	}

	// Occupancy summary bar
	ImGui::Spacing();
	{
		float Total = static_cast<float>(AreaData->Slots.Num());
		if (Total > 0.f)
		{
			int32 Vacant = 0, Assigned = 0, Active = 0, Disabled = 0;
			for (const FArcAreaSlotRuntime& Slot : AreaData->Slots)
			{
				switch (Slot.State)
				{
				case EArcAreaSlotState::Vacant:   Vacant++;   break;
				case EArcAreaSlotState::Assigned: Assigned++; break;
				case EArcAreaSlotState::Active:   Active++;   break;
				case EArcAreaSlotState::Disabled: Disabled++; break;
				}
			}

			ImGui::Text("Slots: %d total", AreaData->Slots.Num());
			ImGui::SameLine();
			ImGui::TextColored(ImVacantColor, "V:%d", Vacant);
			ImGui::SameLine();
			ImGui::TextColored(ImAssignedColor, "A:%d", Assigned);
			ImGui::SameLine();
			ImGui::TextColored(ImActiveColor, "Act:%d", Active);
			if (Disabled > 0)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImDisabledColor, "D:%d", Disabled);
			}
		}
	}

	ImGui::Spacing();
	ImGui::Separator();

	// Tags
	FString TagHeader = FString::Printf(TEXT("Area Tags (%d)"), AreaData->AreaTags.Num());
	if (ImGui::CollapsingHeader(TCHAR_TO_ANSI(*TagHeader), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (AreaData->AreaTags.Num() == 0)
		{
			ImGui::TextDisabled("No tags");
		}
		else
		{
			for (const FGameplayTag& Tag : AreaData->AreaTags)
			{
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
			}
		}
	}

	ImGui::Spacing();
	ImGui::Separator();

	// SmartObject
	DrawSmartObjectSection(*AreaData);

	ImGui::Spacing();
	ImGui::Separator();

	// Slot table
	if (ImGui::CollapsingHeader("Slots", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawSlotTable(AreaData->Handle);
	}

	ImGui::Spacing();
	ImGui::Separator();

	// Danger zone / actions
	if (ImGui::CollapsingHeader("Actions"))
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
		if (ImGui::Button("Unregister Area"))
		{
			Sub->UnregisterArea(ListEntry.Handle);
			SelectedAreaIndex = INDEX_NONE;
			RefreshAreaList();
		}
		ImGui::PopStyleColor();
	}
}

// ====================================================================
// SmartObject Section
// ====================================================================

void FArcAreaDebugger::DrawSmartObjectSection(const FArcAreaData& AreaData)
{
	if (ImGui::CollapsingHeader("SmartObject", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (AreaData.SmartObjectHandle.IsValid())
		{
			ImGui::TextColored(ImTagColor, "SmartObject: Valid");

			// Show which slots map to which SO slots
			if (ImGui::BeginTable("##SOSlotMap", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Area Slot", ImGuiTableColumnFlags_WidthFixed, 80.0f);
				ImGui::TableSetupColumn("SO Slot Index", ImGuiTableColumnFlags_WidthFixed, 100.0f);
				ImGui::TableSetupColumn("Occupant", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();

				for (int32 i = 0; i < AreaData.SlotDefinitions.Num(); ++i)
				{
					const FArcAreaSlotDefinition& SlotDef = AreaData.SlotDefinitions[i];
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%d", i);

					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%d", SlotDef.SmartObjectSlotIndex);

					ImGui::TableSetColumnIndex(2);
					if (AreaData.Slots.IsValidIndex(i))
					{
						const FArcAreaSlotRuntime& SlotRuntime = AreaData.Slots[i];
						if (SlotRuntime.AssignedEntity.IsValid())
						{
							ImVec4 StateCol = SlotStateColor(SlotRuntime.State);
							ImGui::TextColored(StateCol, "E%d [%s]",
								SlotRuntime.AssignedEntity.Index,
								TCHAR_TO_ANSI(*SlotStateToString(SlotRuntime.State)));
						}
						else
						{
							ImGui::TextDisabled("--");
						}
					}
				}

				ImGui::EndTable();
			}
		}
		else
		{
			ImGui::TextDisabled("No SmartObject linked");
		}
	}
}

// ====================================================================
// Slot Table
// ====================================================================

void FArcAreaDebugger::DrawSlotTable(const FArcAreaHandle& AreaHandle)
{
	UArcAreaSubsystem* Sub = GetAreaSubsystem();
	if (!Sub)
	{
		return;
	}

	const FArcAreaData* AreaData = Sub->GetAreaData(AreaHandle);
	if (!AreaData)
	{
		return;
	}

	if (AreaData->Slots.Num() == 0)
	{
		ImGui::TextDisabled("No slots defined");
		return;
	}

	if (ImGui::BeginTable("##SlotTable", 6,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.0f);
		ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Assigned Entity", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("SO Slot", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 180.0f);
		ImGui::TableHeadersRow();

		for (int32 i = 0; i < AreaData->Slots.Num(); ++i)
		{
			const FArcAreaSlotRuntime& SlotRuntime = AreaData->Slots[i];
			const FArcAreaSlotDefinition& SlotDef = AreaData->SlotDefinitions[i];

			ImGui::PushID(i);
			ImGui::TableNextRow();

			// Index
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%d", i);

			// State
			ImGui::TableSetColumnIndex(1);
			ImGui::TextColored(SlotStateColor(SlotRuntime.State), "%s", TCHAR_TO_ANSI(*SlotStateToString(SlotRuntime.State)));

			// Role
			ImGui::TableSetColumnIndex(2);
			if (SlotDef.RoleTag.IsValid())
			{
				ImGui::Text("%s", TCHAR_TO_ANSI(*SlotDef.RoleTag.ToString()));
			}
			else
			{
				ImGui::TextDisabled("(none)");
			}

			// Assigned entity
			ImGui::TableSetColumnIndex(3);
			if (SlotRuntime.AssignedEntity.IsValid())
			{
				ImGui::TextColored(SlotStateColor(SlotRuntime.State), "E%d (SN:%d)",
					SlotRuntime.AssignedEntity.Index, SlotRuntime.AssignedEntity.SerialNumber);
			}
			else
			{
				ImGui::TextDisabled("--");
			}

			// SO Slot mapping
			ImGui::TableSetColumnIndex(4);
			ImGui::Text("%d", SlotDef.SmartObjectSlotIndex);

			// Actions
			ImGui::TableSetColumnIndex(5);
			if (SlotRuntime.State == EArcAreaSlotState::Vacant)
			{
				if (ImGui::SmallButton("Assign"))
				{
					bShowAssignPopup = true;
					AssignTargetSlotIndex = i;
					RefreshAssignableEntities();
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Disable"))
				{
					Sub->DisableSlot(AreaHandle, i);
					RefreshAreaList();
				}
			}
			else if (SlotRuntime.State == EArcAreaSlotState::Assigned || SlotRuntime.State == EArcAreaSlotState::Active)
			{
				if (ImGui::SmallButton("Unassign"))
				{
					Sub->UnassignFromSlot(AreaHandle, i);
					RefreshAreaList();
				}
			}
			else if (SlotRuntime.State == EArcAreaSlotState::Disabled)
			{
				if (ImGui::SmallButton("Enable"))
				{
					Sub->EnableSlot(AreaHandle, i);
					RefreshAreaList();
				}
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	// Slot definition details (collapsible per-slot)
	ImGui::Spacing();
	if (ImGui::TreeNode("Slot Definitions"))
	{
		for (int32 i = 0; i < AreaData->SlotDefinitions.Num(); ++i)
		{
			FString SlotLabel = FString::Printf(TEXT("Slot %d"), i);
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*SlotLabel)))
			{
				DrawSlotDefinitionDetails(AreaData->SlotDefinitions[i], i);
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
}

// ====================================================================
// Slot Definition Details
// ====================================================================

void FArcAreaDebugger::DrawSlotDefinitionDetails(const FArcAreaSlotDefinition& SlotDef, int32 SlotIndex)
{
	ImGui::Text("Role: %s", SlotDef.RoleTag.IsValid()
		? TCHAR_TO_ANSI(*SlotDef.RoleTag.ToString()) : "(none)");
	ImGui::Text("SmartObject Slot Index: %d", SlotDef.SmartObjectSlotIndex);

	// Requirement query
	FString ReqStr = SlotDef.RequirementQuery.GetDescription();
	if (ReqStr.IsEmpty())
	{
		ImGui::TextDisabled("Requirements: None");
	}
	else
	{
		ImGui::Text("Requirements: %s", TCHAR_TO_ANSI(*ReqStr));
	}

	// Vacancy config
	if (ImGui::TreeNode("Vacancy Config"))
	{
		ImGui::Text("Auto-Post: %s", SlotDef.VacancyConfig.bAutoPostVacancy ? "Yes" : "No");
		ImGui::Text("Relevance: %.2f", SlotDef.VacancyConfig.VacancyRelevance);
		if (!SlotDef.VacancyConfig.VacancyTags.IsEmpty())
		{
			ImGui::Text("Custom Tags: %s", TCHAR_TO_ANSI(*SlotDef.VacancyConfig.VacancyTags.ToStringSimple()));
		}
		else
		{
			ImGui::TextDisabled("Tags: Auto-constructed from role");
		}
		ImGui::TreePop();
	}
}

// ====================================================================
// Slot Runtime Details
// ====================================================================

void FArcAreaDebugger::DrawSlotRuntimeDetails(const FArcAreaSlotRuntime& SlotRuntime, int32 SlotIndex, const FArcAreaHandle& AreaHandle)
{
	ImGui::TextColored(SlotStateColor(SlotRuntime.State), "State: %s", TCHAR_TO_ANSI(*SlotStateToString(SlotRuntime.State)));

	if (SlotRuntime.AssignedEntity.IsValid())
	{
		ImGui::Text("Assigned: E%d (SN:%d)", SlotRuntime.AssignedEntity.Index, SlotRuntime.AssignedEntity.SerialNumber);

		// Try to get entity location
		UWorld* World = GetDebugWorld();
		if (World)
		{
			UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
			if (MassSub)
			{
				FMassEntityManager& Manager = MassSub->GetMutableEntityManager();
				if (Manager.IsEntityValid(SlotRuntime.AssignedEntity))
				{
					if (const FTransformFragment* Transform = Manager.GetFragmentDataPtr<FTransformFragment>(SlotRuntime.AssignedEntity))
					{
						FVector Loc = Transform->GetTransform().GetLocation();
						ImGui::Text("Entity Location: (%.0f, %.0f, %.0f)", Loc.X, Loc.Y, Loc.Z);
					}
				}
			}
		}
	}
	else
	{
		ImGui::TextDisabled("No entity assigned");
	}
}

// ====================================================================
// Assign Entity Popup
// ====================================================================

void FArcAreaDebugger::DrawAssignEntityPopup()
{
	if (!bShowAssignPopup)
	{
		return;
	}

	if (SelectedAreaIndex == INDEX_NONE || !CachedAreas.IsValidIndex(SelectedAreaIndex))
	{
		bShowAssignPopup = false;
		return;
	}

	ImGui::OpenPopup("Assign NPC to Slot");

	if (ImGui::BeginPopupModal("Assign NPC to Slot", &bShowAssignPopup, ImGuiWindowFlags_AlwaysAutoResize))
	{
		UArcAreaSubsystem* Sub = GetAreaSubsystem();
		const FAreaListEntry& ListEntry = CachedAreas[SelectedAreaIndex];

		ImGui::Text("Area: %s (H%u)", TCHAR_TO_ANSI(*ListEntry.DisplayName), ListEntry.Handle.GetValue());
		ImGui::Text("Target Slot: %d", AssignTargetSlotIndex);
		ImGui::Separator();

		ImGui::InputText("Search Entities", EntityFilterBuf, IM_ARRAYSIZE(EntityFilterBuf));

		if (ImGui::SmallButton("Refresh Entities"))
		{
			RefreshAssignableEntities();
		}

		ImGui::Spacing();
		ImGui::Text("Assignable NPCs (%d):", CachedAssignableEntities.Num());

		FString EntFilter = FString(ANSI_TO_TCHAR(EntityFilterBuf)).ToLower();

		if (ImGui::BeginChild("AssignEntityList", ImVec2(500, 300), ImGuiChildFlags_Borders))
		{
			for (int32 i = 0; i < CachedAssignableEntities.Num(); ++i)
			{
				const FAssignableEntityEntry& EntEntry = CachedAssignableEntities[i];

				if (!EntFilter.IsEmpty() && !EntEntry.Label.ToLower().Contains(EntFilter))
				{
					continue;
				}

				ImGui::PushID(i);

				ImVec4 Color = EntEntry.bCurrentlyAssigned ? ImAssignedColor : ImVacantColor;
				ImGui::PushStyleColor(ImGuiCol_Text, Color);
				ImGui::Text("%s", TCHAR_TO_ANSI(*EntEntry.Label));
				ImGui::PopStyleColor();

				// Tooltip
				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text("Entity: E%d (SN:%d)", EntEntry.Entity.Index, EntEntry.Entity.SerialNumber);
					ImGui::Text("Location: (%.0f, %.0f, %.0f)", EntEntry.Location.X, EntEntry.Location.Y, EntEntry.Location.Z);
					ImGui::Text("Eligible Roles: %s", TCHAR_TO_ANSI(*EntEntry.EligibleRoles.ToStringSimple()));
					if (EntEntry.bCurrentlyAssigned)
					{
						ImGui::TextColored(ImAssignedColor, "Currently assigned to H%u slot %d",
							EntEntry.CurrentAreaHandle.GetValue(), EntEntry.CurrentSlotIndex);
					}
					ImGui::EndTooltip();
				}

				ImGui::SameLine(420);
				if (ImGui::SmallButton("Assign"))
				{
					if (Sub)
					{
						Sub->AssignToSlot(ListEntry.Handle, AssignTargetSlotIndex, EntEntry.Entity);
						RefreshAreaList();
						RefreshAssignableEntities();
					}
					bShowAssignPopup = false;
				}

				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		ImGui::Spacing();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			bShowAssignPopup = false;
		}

		ImGui::EndPopup();
	}
}

// ====================================================================
// World Visualization
// ====================================================================

void FArcAreaDebugger::DrawWorldVisualization()
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return;
	}

	UArcAreaSubsystem* Sub = GetAreaSubsystem();
	if (!Sub)
	{
		return;
	}

	constexpr float ZOffset = 100.f;

	// Draw all areas as markers
	if (bDrawAllAreas)
	{
		for (int32 i = 0; i < CachedAreas.Num(); ++i)
		{
			const FAreaListEntry& Entry = CachedAreas[i];
			const bool bIsSelected = (SelectedAreaIndex == i);

			if (bIsSelected)
			{
				continue; // Selected drawn separately
			}

			DrawDebugPoint(World, Entry.Location + FVector(0, 0, ZOffset), 12.f, AreaColor, false, -1.f);

			if (bDrawLabels)
			{
				FString ShortLabel = FString::Printf(TEXT("H%u %s"), Entry.Handle.GetValue(),
					*Entry.DisplayName);
				DrawDebugString(World, Entry.Location + FVector(0, 0, ZOffset + 30.f), ShortLabel, nullptr, AreaColor, -1.f, true, 0.8f);
			}
		}
	}

	// Draw selected area with full detail
	if (bDrawSelectedDetail && SelectedAreaIndex != INDEX_NONE && CachedAreas.IsValidIndex(SelectedAreaIndex))
	{
		const FAreaListEntry& SelEntry = CachedAreas[SelectedAreaIndex];
		const FArcAreaData* AreaData = Sub->GetAreaData(SelEntry.Handle);
		if (!AreaData)
		{
			return;
		}

		const FVector& Loc = AreaData->Location;

		// Sphere marker
		DrawDebugSphere(World, Loc + FVector(0, 0, ZOffset), 50.f, 16, SelectedAreaColor, false, -1.f, 0, 2.f);

		// Label
		FString DetailLabel = FString::Printf(TEXT("H%u %s\n%d slots"),
			AreaData->Handle.GetValue(), *AreaData->DisplayName.ToString(), AreaData->Slots.Num());
		DrawDebugString(World, Loc + FVector(0, 0, ZOffset + 70.f), DetailLabel, nullptr, FColor::White, -1.f, true, 1.0f);

		// Vertical line
		DrawDebugLine(World, Loc, Loc + FVector(0, 0, ZOffset + 50.f), SelectedAreaColor, false, -1.f, 0, 2.f);

		// Draw lines to assigned entities
		UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
		if (MassSub)
		{
			FMassEntityManager& Manager = MassSub->GetMutableEntityManager();

			for (int32 SlotIdx = 0; SlotIdx < AreaData->Slots.Num(); ++SlotIdx)
			{
				const FArcAreaSlotRuntime& SlotRuntime = AreaData->Slots[SlotIdx];
				if (!SlotRuntime.AssignedEntity.IsValid())
				{
					continue;
				}

				if (!Manager.IsEntityValid(SlotRuntime.AssignedEntity))
				{
					continue;
				}

				const FTransformFragment* Transform = Manager.GetFragmentDataPtr<FTransformFragment>(SlotRuntime.AssignedEntity);
				if (!Transform)
				{
					continue;
				}

				FVector EntityLoc = Transform->GetTransform().GetLocation();
				FColor LineColor = SlotStateWorldColor(SlotRuntime.State);

				// Line from area to entity
				DrawDebugLine(World, Loc + FVector(0, 0, ZOffset * 0.5f), EntityLoc + FVector(0, 0, 50.f),
					LineColor, false, -1.f, 0, 1.5f);

				// Entity marker
				DrawDebugSphere(World, EntityLoc + FVector(0, 0, 50.f), 20.f, 8, LineColor, false, -1.f, 0, 1.5f);

				// Entity label
				FString EntityLabel = FString::Printf(TEXT("E%d [S%d %s]"),
					SlotRuntime.AssignedEntity.Index, SlotIdx, *SlotStateToString(SlotRuntime.State));
				DrawDebugString(World, EntityLoc + FVector(0, 0, 80.f), EntityLabel, nullptr, LineColor, -1.f, true, 0.7f);
			}
		}
	}
}
