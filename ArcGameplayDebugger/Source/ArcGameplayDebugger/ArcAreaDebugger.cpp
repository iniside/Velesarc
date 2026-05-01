// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaDebugger.h"

#include "imgui.h"
#include "ArcAreaDebuggerDrawComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "ArcAreaSubsystem.h"
#include "ArcAreaDefinition.h"
#include "Mass/ArcAreaAssignmentFragments.h"
#include "MassEntitySubsystem.h"
#include "MassEntityQuery.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Mass/ArcKnowledgeFragments.h"

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
	static const ImVec4 ImKnowledgeColor(0.8f, 0.8f, 0.8f, 1.0f);
	static const ImVec4 ImAdUnclaimedColor(0.8f, 0.6f, 0.2f, 1.0f);
	static const ImVec4 ImAdClaimedColor(0.2f, 0.6f, 1.0f, 1.0f);

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

	// SmartObject slot state helpers
	static const ImVec4 ImSOFreeColor(0.2f, 0.8f, 0.2f, 1.0f);
	static const ImVec4 ImSOClaimedColor(0.8f, 0.6f, 0.2f, 1.0f);
	static const ImVec4 ImSOOccupiedColor(0.2f, 0.6f, 1.0f, 1.0f);
	static const ImVec4 ImSODisabledColor(0.6f, 0.2f, 0.2f, 1.0f);
	static const ImVec4 ImSOInvalidColor(0.4f, 0.4f, 0.4f, 1.0f);

	const FString SOSlotStateToString(ESmartObjectSlotState State)
	{
		switch (State)
		{
		case ESmartObjectSlotState::Free:     return TEXT("Free");
		case ESmartObjectSlotState::Claimed:  return TEXT("Claimed");
		case ESmartObjectSlotState::Occupied: return TEXT("Occupied");
		default:                              return TEXT("Invalid");
		}
	}

	ImVec4 SOSlotStateColor(ESmartObjectSlotState State)
	{
		switch (State)
		{
		case ESmartObjectSlotState::Free:     return ImSOFreeColor;
		case ESmartObjectSlotState::Claimed:  return ImSOClaimedColor;
		case ESmartObjectSlotState::Occupied: return ImSOOccupiedColor;
		default:                              return ImSOInvalidColor;
		}
	}

	FColor SOSlotStateWorldColor(ESmartObjectSlotState State)
	{
		switch (State)
		{
		case ESmartObjectSlotState::Free:     return VacantColor;
		case ESmartObjectSlotState::Claimed:  return AssignedColor;
		case ESmartObjectSlotState::Occupied: return ActiveColor;
		default:                              return DisabledColor;
		}
	}

	const FString TagMergingPolicyToString(ESmartObjectTagMergingPolicy Policy)
	{
		switch (Policy)
		{
		case ESmartObjectTagMergingPolicy::Combine:  return TEXT("Combine");
		case ESmartObjectTagMergingPolicy::Override:  return TEXT("Override");
		default:                                     return TEXT("Unknown");
		}
	}

	const FString TagFilteringPolicyToString(ESmartObjectTagFilteringPolicy Policy)
	{
		switch (Policy)
		{
		case ESmartObjectTagFilteringPolicy::NoFilter: return TEXT("NoFilter");
		case ESmartObjectTagFilteringPolicy::Combine:  return TEXT("Combine");
		case ESmartObjectTagFilteringPolicy::Override:  return TEXT("Override");
		default:                                       return TEXT("Unknown");
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

UArcKnowledgeSubsystem* FArcAreaDebugger::GetKnowledgeSubsystem() const
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UArcKnowledgeSubsystem>();
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
	SelectedSlotIndex = INDEX_NONE;
	bShowAssignPopup = false;
	AssignTargetSlotIndex = INDEX_NONE;
	CachedKnowledgeEntries.Reset();
	CachedClaimableEntities.Reset();
	SelectedKnowledgeIndex = INDEX_NONE;
	KnowledgeOwnerEntity = FMassEntityHandle();
	bShowClaimPopup = false;
	ClaimTargetHandle = FArcKnowledgeHandle();
	ClaimEntityFilterBuf[0] = '\0';
	RefreshAreaList();
}

void FArcAreaDebugger::Uninitialize()
{
	DestroyDrawActor();
	CachedAreas.Reset();
	CachedAssignableEntities.Reset();
	SelectedAreaIndex = INDEX_NONE;
	SelectedSlotIndex = INDEX_NONE;
	CachedKnowledgeEntries.Reset();
	CachedClaimableEntities.Reset();
	SelectedKnowledgeIndex = INDEX_NONE;
	KnowledgeOwnerEntity = FMassEntityHandle();
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
				Entry.CurrentAreaHandle = Assignment.SlotHandle.AreaHandle;
				Entry.CurrentSlotIndex = Assignment.SlotHandle.SlotIndex;

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

void FArcAreaDebugger::RefreshKnowledgeEntries()
{
	// Preserve selection across refresh
	FArcKnowledgeHandle PreviousSelectedHandle;
	if (SelectedKnowledgeIndex != INDEX_NONE && CachedKnowledgeEntries.IsValidIndex(SelectedKnowledgeIndex))
	{
		PreviousSelectedHandle = CachedKnowledgeEntries[SelectedKnowledgeIndex].Handle;
	}

	CachedKnowledgeEntries.Reset();
	SelectedKnowledgeIndex = INDEX_NONE;

	if (!KnowledgeOwnerEntity.IsValid())
	{
		return;
	}

	UArcKnowledgeSubsystem* KnowledgeSub = GetKnowledgeSubsystem();
	if (!KnowledgeSub)
	{
		return;
	}

	TArray<FArcKnowledgeHandle> Handles;
	KnowledgeSub->GetKnowledgeBySource(KnowledgeOwnerEntity, Handles);

	for (const FArcKnowledgeHandle& Handle : Handles)
	{
		const FArcKnowledgeEntry* Entry = KnowledgeSub->GetKnowledgeEntry(Handle);
		if (!Entry)
		{
			continue;
		}

		FKnowledgeEntryCache& Cached = CachedKnowledgeEntries.AddDefaulted_GetRef();
		Cached.Handle = Entry->Handle;
		Cached.Tags = Entry->Tags;
		Cached.Location = Entry->Location;
		Cached.Relevance = Entry->Relevance;
		Cached.Timestamp = Entry->Timestamp;
		Cached.Lifetime = Entry->Lifetime;
		Cached.bIsAdvertisement = Entry->Instruction.IsValid();
		Cached.bClaimed = Entry->bClaimed;
		Cached.ClaimedBy = Entry->ClaimedBy;
		Cached.PayloadTypeName = Entry->Payload.IsValid()
			? Entry->Payload.GetScriptStruct()->GetName()
			: FString();
	}

	// Restore selection by handle
	if (PreviousSelectedHandle.IsValid())
	{
		for (int32 i = 0; i < CachedKnowledgeEntries.Num(); ++i)
		{
			if (CachedKnowledgeEntries[i].Handle == PreviousSelectedHandle)
			{
				SelectedKnowledgeIndex = i;
				break;
			}
		}
	}
}

void FArcAreaDebugger::RefreshClaimableEntities()
{
	CachedClaimableEntities.Reset();

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

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	FMassExecutionContext ExecContext(EntityManager);
	FMassEntityQuery Query(EntityManager.AsShared());
	Query.AddRequirement<FArcKnowledgeMemberFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddTagRequirement<FArcKnowledgeMemberTag>(EMassFragmentPresence::All);

	Query.ForEachEntityChunk(ExecContext,
		[this](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();
			const TArrayView<const FArcKnowledgeMemberFragment> MemberList = Context.GetFragmentView<FArcKnowledgeMemberFragment>();
			const TArrayView<const FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

			for (int32 i = 0; i < NumEntities; ++i)
			{
				const FMassEntityHandle Entity = Context.GetEntity(i);
				const FArcKnowledgeMemberFragment& Member = MemberList[i];
				const FTransformFragment& Transform = TransformList[i];

				FClaimableEntityEntry& CEntry = CachedClaimableEntities.AddDefaulted_GetRef();
				CEntry.Entity = Entity;
				CEntry.Location = Transform.GetTransform().GetLocation();
				CEntry.Role = Member.Role;

				FString RoleStr = Member.Role.IsValid() ? Member.Role.ToString() : TEXT("(none)");
				CEntry.Label = FString::Printf(TEXT("E%d [%s]"), Entity.Index, *RoleStr);
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
		if (DrawComponent.IsValid()) { DrawComponent->ClearShapes(); }
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
				RefreshKnowledgeEntries();
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
	DrawClaimEntityPopup();
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
				if (SelectedAreaIndex != i)
				{
					SelectedSlotIndex = INDEX_NONE;
				}
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

	// Track knowledge owner — refresh if the owner entity changed
	if (AreaData->OwnerEntity != KnowledgeOwnerEntity)
	{
		KnowledgeOwnerEntity = AreaData->OwnerEntity;
		RefreshKnowledgeEntries();
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

	// Knowledge
	DrawKnowledgeSection(AreaData->OwnerEntity);

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
	if (!ImGui::CollapsingHeader("SmartObject", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	if (!AreaData.SmartObjectHandle.IsValid())
	{
		ImGui::TextDisabled("No SmartObject linked");
		return;
	}

	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return;
	}

	const USmartObjectSubsystem* SOSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
	if (!SOSubsystem)
	{
		ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "SmartObject subsystem unavailable");
		return;
	}

	ImGui::TextColored(ImTagColor, "SmartObject: Linked");

	// Gather all SO slot handles
	TArray<FSmartObjectSlotHandle> AllSOSlots;
	SOSubsystem->GetAllSlots(AreaData.SmartObjectHandle, AllSOSlots);

	ImGui::Text("SO Slots: %d", AllSOSlots.Num());

	// --- SO-level definition info (read from the first slot's view) ---
	if (AllSOSlots.Num() > 0)
	{
		SOSubsystem->ReadSlotData(AllSOSlots[0], [](FConstSmartObjectSlotView SlotView)
		{
			const USmartObjectDefinition& Def = SlotView.GetSmartObjectDefinition();

			// Object-level activity tags
			const FGameplayTagContainer& ObjActivityTags = Def.GetActivityTags();
			if (!ObjActivityTags.IsEmpty())
			{
				ImGui::TextColored(ImTagColor, "Object Activity Tags:");
				ImGui::SameLine();
				ImGui::Text("%s", TCHAR_TO_ANSI(*ObjActivityTags.ToStringSimple()));
			}
			else
			{
				ImGui::TextDisabled("Object Activity Tags: (none)");
			}

			// Object-level user tag filter
			const FGameplayTagQuery& ObjUserFilter = Def.GetUserTagFilter();
			if (!ObjUserFilter.IsEmpty())
			{
				ImGui::Text("Object User Tag Filter: %s", TCHAR_TO_ANSI(*ObjUserFilter.GetDescription()));
			}

			// Policies
			ImGui::Text("Activity Tag Merging: %s",
				TCHAR_TO_ANSI(*TagMergingPolicyToString(Def.GetActivityTagsMergingPolicy())));
			ImGui::Text("User Tag Filtering: %s",
				TCHAR_TO_ANSI(*TagFilteringPolicyToString(Def.GetUserTagsFilteringPolicy())));
		});
	}

	ImGui::Spacing();

	// --- Per-slot detail table ---
	if (ImGui::TreeNodeEx("SO Slot Details", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("##SOSlotDetails", 6,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.0f);
			ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 70.0f);
			ImGui::TableSetupColumn("Activity Tags", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Runtime Tags", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("User Tag Filter", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 50.0f);
			ImGui::TableHeadersRow();

			for (int32 i = 0; i < AllSOSlots.Num(); ++i)
			{
				const FSmartObjectSlotHandle& SlotHandle = AllSOSlots[i];
				const ESmartObjectSlotState SOState = SOSubsystem->GetSlotState(SlotHandle);
				const FGameplayTagContainer& RuntimeTags = SOSubsystem->GetSlotTags(SlotHandle);

				ImGui::PushID(i);
				ImGui::TableNextRow();

				// Index
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%d", i);

				// State
				ImGui::TableSetColumnIndex(1);
				ImGui::TextColored(SOSlotStateColor(SOState), "%s", TCHAR_TO_ANSI(*SOSlotStateToString(SOState)));

				// Activity Tags, User Tag Filter, Enabled — need the slot view
				SOSubsystem->ReadSlotData(SlotHandle, [i, &RuntimeTags](FConstSmartObjectSlotView SlotView)
				{
					// Activity Tags (merged per policy)
					ImGui::TableSetColumnIndex(2);
					FGameplayTagContainer MergedActivityTags;
					SlotView.GetActivityTags(MergedActivityTags);
					if (!MergedActivityTags.IsEmpty())
					{
						ImGui::TextColored(ImTagColor, "%s", TCHAR_TO_ANSI(*MergedActivityTags.ToStringSimple()));
					}
					else
					{
						ImGui::TextDisabled("(none)");
					}

					// Runtime Tags
					ImGui::TableSetColumnIndex(3);
					if (!RuntimeTags.IsEmpty())
					{
						ImGui::Text("%s", TCHAR_TO_ANSI(*RuntimeTags.ToStringSimple()));
					}
					else
					{
						ImGui::TextDisabled("(none)");
					}

					// User Tag Filter
					ImGui::TableSetColumnIndex(4);
					const FSmartObjectSlotDefinition& SlotDef = SlotView.GetDefinition();
					if (!SlotDef.UserTagFilter.IsEmpty())
					{
						ImGui::Text("%s", TCHAR_TO_ANSI(*SlotDef.UserTagFilter.GetDescription()));
					}
					else
					{
						ImGui::TextDisabled("(none)");
					}

					// Enabled
					ImGui::TableSetColumnIndex(5);
					ImGui::Text("%s", SlotDef.bEnabled ? "Yes" : "No");
				});

				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		ImGui::TreePop();
	}

	ImGui::Spacing();

	// --- Area Slot ↔ SO Slot matching preview ---
	if (ImGui::TreeNodeEx("Tag Matching Preview", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("Shows which SO slots each area slot would match via tag-based resolution");
		ImGui::Spacing();

		for (int32 AreaSlotIdx = 0; AreaSlotIdx < AreaData.SlotDefinitions.Num(); ++AreaSlotIdx)
		{
			const FArcAreaSlotDefinition& SlotDef = AreaData.SlotDefinitions[AreaSlotIdx];
			FString ReqStr = SlotDef.RequirementQuery.GetDescription();
			FString NodeLabel = FString::Printf(TEXT("Area Slot %d%s"), AreaSlotIdx,
				ReqStr.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" — %s"), *ReqStr));

			ImGui::PushID(AreaSlotIdx);

			if (ImGui::TreeNode(TCHAR_TO_ANSI(*NodeLabel)))
			{
				// Show all SO slots with their activity tags and state
				// (Tag matching now happens at assignment time using entity tags, not slot definition)
				ImGui::TextDisabled("SO slot matching uses entity tags at assignment time");
				ImGui::Spacing();

				int32 TaggedCount = 0;
				int32 UntaggedCount = 0;
				for (int32 s = 0; s < AllSOSlots.Num(); ++s)
				{
					FGameplayTagContainer ActivityTags;
					SOSubsystem->ReadSlotData(AllSOSlots[s], [&ActivityTags](FConstSmartObjectSlotView SlotView)
					{
						SlotView.GetActivityTags(ActivityTags);
					});

					ESmartObjectSlotState SOState = SOSubsystem->GetSlotState(AllSOSlots[s]);

					if (!ActivityTags.IsEmpty())
					{
						ImGui::BulletText("SO Slot %d", s);
						ImGui::SameLine();
						ImGui::TextColored(SOSlotStateColor(SOState), "[%s]", TCHAR_TO_ANSI(*SOSlotStateToString(SOState)));
						ImGui::SameLine();
						ImGui::TextDisabled("Tags: %s", TCHAR_TO_ANSI(*ActivityTags.ToStringSimple()));
						TaggedCount++;
					}
					else
					{
						UntaggedCount++;
					}
				}

				if (UntaggedCount > 0)
				{
					ImGui::TextColored(ImAssignedColor, "%d untagged SO slot(s) (fallback)", UntaggedCount);
				}

				if (TaggedCount == 0 && UntaggedCount == 0)
				{
					ImGui::TextColored(ImDisabledColor, "No SO slots found");
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		ImGui::TreePop();
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

	if (ImGui::BeginTable("##SlotTable", 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.0f);
		ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn("Assigned Entity", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 180.0f);
		ImGui::TableHeadersRow();

		for (int32 i = 0; i < AreaData->Slots.Num(); ++i)
		{
			const FArcAreaSlotRuntime& SlotRuntime = AreaData->Slots[i];
			const FArcAreaSlotDefinition& SlotDef = AreaData->SlotDefinitions[i];

			ImGui::PushID(i);
			ImGui::TableNextRow();

			// Selectable spanning all columns for row selection
			ImGui::TableSetColumnIndex(0);
			const bool bSlotSelected = (SelectedSlotIndex == i);
			if (ImGui::Selectable("##SlotRow", bSlotSelected,
				ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
			{
				SelectedSlotIndex = (SelectedSlotIndex == i) ? INDEX_NONE : i;
			}
			ImGui::SameLine();

			// Index
			ImGui::Text("%d", i);

			// State
			ImGui::TableSetColumnIndex(1);
			ImGui::TextColored(SlotStateColor(SlotRuntime.State), "%s", TCHAR_TO_ANSI(*SlotStateToString(SlotRuntime.State)));

			// Assigned entity
			ImGui::TableSetColumnIndex(2);
			if (SlotRuntime.AssignedEntity.IsValid())
			{
				ImGui::TextColored(SlotStateColor(SlotRuntime.State), "E%d (SN:%d)",
					SlotRuntime.AssignedEntity.Index, SlotRuntime.AssignedEntity.SerialNumber);
			}
			else
			{
				ImGui::TextDisabled("--");
			}

			// Actions
			ImGui::TableSetColumnIndex(3);
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
					Sub->DisableSlot(FArcAreaSlotHandle(AreaHandle, i));
					RefreshAreaList();
				}
			}
			else if (SlotRuntime.State == EArcAreaSlotState::Assigned || SlotRuntime.State == EArcAreaSlotState::Active)
			{
				if (ImGui::SmallButton("Unassign"))
				{
					Sub->UnassignFromSlot(FArcAreaSlotHandle(AreaHandle, i));
					RefreshAreaList();
				}
			}
			else if (SlotRuntime.State == EArcAreaSlotState::Disabled)
			{
				if (ImGui::SmallButton("Enable"))
				{
					Sub->EnableSlot(FArcAreaSlotHandle(AreaHandle, i));
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
			ImGui::TextDisabled("Tags: Auto-constructed from area tags");
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
						Sub->AssignToSlot(FArcAreaSlotHandle(ListEntry.Handle, AssignTargetSlotIndex), EntEntry.Entity);
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
// Knowledge Section
// ====================================================================

void FArcAreaDebugger::DrawKnowledgeSection(const FMassEntityHandle& OwnerEntity)
{
	if (!OwnerEntity.IsValid())
	{
		return;
	}

	FString HeaderLabel = FString::Printf(TEXT("Knowledge (%d)"), CachedKnowledgeEntries.Num());
	if (!ImGui::CollapsingHeader(TCHAR_TO_ANSI(*HeaderLabel)))
	{
		return;
	}

	if (CachedKnowledgeEntries.Num() == 0)
	{
		ImGui::TextDisabled("No knowledge entries for this entity");
		return;
	}

	// Summary bar
	int32 TotalAds = 0;
	int32 UnclaimedAds = 0;
	int32 ClaimedAds = 0;
	for (const FKnowledgeEntryCache& Entry : CachedKnowledgeEntries)
	{
		if (Entry.bIsAdvertisement)
		{
			TotalAds++;
			if (Entry.bClaimed)
			{
				ClaimedAds++;
			}
			else
			{
				UnclaimedAds++;
			}
		}
	}

	ImGui::Text("%d entries | %d ads (", CachedKnowledgeEntries.Num(), TotalAds);
	ImGui::SameLine(0, 0);
	ImGui::TextColored(ImAdUnclaimedColor, "%d unclaimed", UnclaimedAds);
	ImGui::SameLine(0, 0);
	ImGui::Text(", ");
	ImGui::SameLine(0, 0);
	ImGui::TextColored(ImAdClaimedColor, "%d claimed", ClaimedAds);
	ImGui::SameLine(0, 0);
	ImGui::Text(")");

	ImGui::Spacing();

	// Side-by-side layout: table on left, detail on right
	float KnowledgePanelWidth = ImGui::GetContentRegionAvail().x;
	float KnowledgeTableWidth = KnowledgePanelWidth * 0.60f;

	// Left: knowledge entries table
	if (ImGui::BeginChild("KnowledgeTablePanel", ImVec2(KnowledgeTableWidth, 250), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		if (ImGui::BeginTable("KnowledgeTable", 6,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
			ImVec2(0, 0)))
		{
			ImGui::TableSetupColumn("Handle",    ImGuiTableColumnFlags_WidthFixed,   60.0f);
			ImGui::TableSetupColumn("Tags",      ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Relevance", ImGuiTableColumnFlags_WidthFixed,   70.0f);
			ImGui::TableSetupColumn("Age",       ImGuiTableColumnFlags_WidthFixed,   60.0f);
			ImGui::TableSetupColumn("Type",      ImGuiTableColumnFlags_WidthFixed,  100.0f);
			ImGui::TableSetupColumn("Status",    ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			UWorld* World = GetDebugWorld();
			double CurrentTime = World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;

			for (int32 i = 0; i < CachedKnowledgeEntries.Num(); ++i)
			{
				const FKnowledgeEntryCache& Entry = CachedKnowledgeEntries[i];

				ImVec4 RowColor;
				if (!Entry.bIsAdvertisement)
				{
					RowColor = ImKnowledgeColor;
				}
				else if (Entry.bClaimed)
				{
					RowColor = ImAdClaimedColor;
				}
				else
				{
					RowColor = ImAdUnclaimedColor;
				}

				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::PushStyleColor(ImGuiCol_Text, RowColor);

				// Handle column — selectable spanning all columns
				ImGui::TableSetColumnIndex(0);
				FString HandleStr = FString::Printf(TEXT("H%u"), Entry.Handle.GetValue());
				bool bSelected = (SelectedKnowledgeIndex == i);
				if (ImGui::Selectable(TCHAR_TO_ANSI(*HandleStr), bSelected,
					ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
				{
					SelectedKnowledgeIndex = (SelectedKnowledgeIndex == i) ? INDEX_NONE : i;
				}

				// Tags
				ImGui::TableSetColumnIndex(1);
				FString TagsStr = Entry.Tags.ToStringSimple();
				if (TagsStr.Len() > 40)
				{
					TagsStr = TagsStr.Left(37) + TEXT("...");
				}
				ImGui::TextUnformatted(TCHAR_TO_ANSI(*TagsStr));

				// Relevance
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.2f", Entry.Relevance);

				// Age
				ImGui::TableSetColumnIndex(3);
				double AgeSecs = CurrentTime - Entry.Timestamp;
				if (AgeSecs < 60.0)
				{
					ImGui::Text("%.1fs", static_cast<float>(AgeSecs));
				}
				else
				{
					ImGui::Text("%.1fm", static_cast<float>(AgeSecs / 60.0));
				}

				// Type
				ImGui::TableSetColumnIndex(4);
				ImGui::TextUnformatted(Entry.bIsAdvertisement ? "Advertisement" : "Knowledge");

				// Status
				ImGui::TableSetColumnIndex(5);
				if (Entry.bIsAdvertisement)
				{
					if (Entry.bClaimed)
					{
						ImGui::Text("Claimed E%d", Entry.ClaimedBy.Index);
					}
					else
					{
						ImGui::TextUnformatted("Unclaimed");
					}
				}
				else
				{
					ImGui::TextUnformatted("-");
				}

				ImGui::PopStyleColor();
				ImGui::PopID();
			}

			ImGui::EndTable();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right: selected entry detail
	if (ImGui::BeginChild("KnowledgeDetailPanel", ImVec2(0, 250), ImGuiChildFlags_Borders))
	{
		if (SelectedKnowledgeIndex != INDEX_NONE && CachedKnowledgeEntries.IsValidIndex(SelectedKnowledgeIndex))
		{
			const FKnowledgeEntryCache& Sel = CachedKnowledgeEntries[SelectedKnowledgeIndex];

			FString DetailHeader = FString::Printf(TEXT("Entry H%u"), Sel.Handle.GetValue());
			ImGui::TextColored(ImSelectedColor, "%s", TCHAR_TO_ANSI(*DetailHeader));
			ImGui::Separator();

			// Full tags
			ImGui::Text("Tags:");
			for (const FGameplayTag& Tag : Sel.Tags)
			{
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
			}
			if (Sel.Tags.IsEmpty())
			{
				ImGui::BulletText("(none)");
			}

			ImGui::Spacing();

			// Location
			ImGui::Text("Location: (%.1f, %.1f, %.1f)", Sel.Location.X, Sel.Location.Y, Sel.Location.Z);

			// Relevance
			ImGui::Text("Relevance: %.2f", Sel.Relevance);

			// Payload type
			if (!Sel.PayloadTypeName.IsEmpty())
			{
				ImGui::Text("Payload: %s", TCHAR_TO_ANSI(*Sel.PayloadTypeName));
			}
			else
			{
				ImGui::TextDisabled("Payload: (none)");
			}

			// Lifetime remaining
			if (Sel.Lifetime > 0.0f)
			{
				UWorld* World = GetDebugWorld();
				double CurrentTime = World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
				double Elapsed = CurrentTime - Sel.Timestamp;
				float Remaining = Sel.Lifetime - static_cast<float>(Elapsed);
				ImGui::Text("Lifetime: %.1fs remaining", Remaining);
			}
			else
			{
				ImGui::TextDisabled("Lifetime: Unlimited");
			}

			// Type
			ImGui::Spacing();
			if (Sel.bIsAdvertisement)
			{
				if (Sel.bClaimed)
				{
					ImGui::TextColored(ImAdClaimedColor, "Claimed by E%d", Sel.ClaimedBy.Index);
				}
				else
				{
					ImGui::TextColored(ImAdUnclaimedColor, "Unclaimed Advertisement");
				}
			}

			// Assign entity button for unclaimed ads
			if (Sel.bIsAdvertisement && !Sel.bClaimed)
			{
				ImGui::Spacing();
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
				if (ImGui::Button("Assign Entity"))
				{
					ClaimTargetHandle = Sel.Handle;
					bShowClaimPopup = true;
					ClaimEntityFilterBuf[0] = '\0';
					RefreshClaimableEntities();
				}
				ImGui::PopStyleColor();
			}
		}
		else
		{
			ImGui::TextDisabled("Select an entry to view details");
		}
	}
	ImGui::EndChild();
}

// ====================================================================
// Claim Entity Popup
// ====================================================================

void FArcAreaDebugger::DrawClaimEntityPopup()
{
	if (!bShowClaimPopup)
	{
		return;
	}

	if (!ClaimTargetHandle.IsValid())
	{
		bShowClaimPopup = false;
		return;
	}

	ImGui::OpenPopup("Claim Advertisement");

	if (ImGui::BeginPopupModal("Claim Advertisement", &bShowClaimPopup, ImGuiWindowFlags_AlwaysAutoResize))
	{
		UArcKnowledgeSubsystem* KnowledgeSub = GetKnowledgeSubsystem();

		ImGui::Text("Advertisement: H%u", ClaimTargetHandle.GetValue());
		ImGui::Separator();

		ImGui::InputText("Search Entities", ClaimEntityFilterBuf, IM_ARRAYSIZE(ClaimEntityFilterBuf));

		if (ImGui::SmallButton("Refresh Entities"))
		{
			RefreshClaimableEntities();
		}

		ImGui::Spacing();
		ImGui::Text("Claimable Entities (%d):", CachedClaimableEntities.Num());

		FString EntFilter = FString(ANSI_TO_TCHAR(ClaimEntityFilterBuf)).ToLower();

		if (ImGui::BeginChild("ClaimEntityList", ImVec2(500, 300), ImGuiChildFlags_Borders))
		{
			for (int32 i = 0; i < CachedClaimableEntities.Num(); ++i)
			{
				const FClaimableEntityEntry& EntEntry = CachedClaimableEntities[i];

				if (!EntFilter.IsEmpty() && !EntEntry.Label.ToLower().Contains(EntFilter))
				{
					continue;
				}

				ImGui::PushID(i);

				ImGui::PushStyleColor(ImGuiCol_Text, ImKnowledgeColor);
				ImGui::Text("%s", TCHAR_TO_ANSI(*EntEntry.Label));
				ImGui::PopStyleColor();

				// Tooltip
				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text("Entity: E%d (SN:%d)", EntEntry.Entity.Index, EntEntry.Entity.SerialNumber);
					ImGui::Text("Location: (%.0f, %.0f, %.0f)", EntEntry.Location.X, EntEntry.Location.Y, EntEntry.Location.Z);
					ImGui::Text("Role: %s", EntEntry.Role.IsValid() ? TCHAR_TO_ANSI(*EntEntry.Role.ToString()) : "(none)");
					ImGui::EndTooltip();
				}

				ImGui::SameLine(420);
				if (ImGui::SmallButton("Claim"))
				{
					if (KnowledgeSub)
					{
						bool bSuccess = KnowledgeSub->ClaimAdvertisement(ClaimTargetHandle, EntEntry.Entity);
						if (bSuccess)
						{
							bShowClaimPopup = false;
							RefreshKnowledgeEntries();
						}
					}
				}

				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		ImGui::Spacing();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			bShowClaimPopup = false;
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
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	UArcAreaSubsystem* Sub = GetAreaSubsystem();
	if (!Sub)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	constexpr float ZOffset = 100.f;

	FArcAreaDebugDrawData Data;
	Data.ZOffset = ZOffset;
	Data.bDrawLabels = bDrawLabels;

	// Collect all non-selected area markers
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

			FArcAreaDebugDrawEntry& Marker = Data.AreaMarkers.AddDefaulted_GetRef();
			Marker.Location = Entry.Location;
			Marker.Color = AreaColor;
			Marker.Label = FString::Printf(TEXT("H%u %s"), Entry.Handle.GetValue(), *Entry.DisplayName);
		}
	}

	// Collect selected area with full detail
	if (bDrawSelectedDetail && SelectedAreaIndex != INDEX_NONE && CachedAreas.IsValidIndex(SelectedAreaIndex))
	{
		const FAreaListEntry& SelEntry = CachedAreas[SelectedAreaIndex];
		const FArcAreaData* AreaData = Sub->GetAreaData(SelEntry.Handle);
		if (AreaData)
		{
			Data.bHasSelectedArea = true;
			Data.SelectedLocation = AreaData->Location;
			Data.SelectedColor = SelectedAreaColor;
			Data.SelectedLabel = FString::Printf(TEXT("H%u %s\n%d slots"),
				AreaData->Handle.GetValue(), *AreaData->DisplayName.ToString(), AreaData->Slots.Num());

			USmartObjectSubsystem* SOSub = World->GetSubsystem<USmartObjectSubsystem>();
			UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
			FMassEntityManager* Manager = MassSub ? &MassSub->GetMutableEntityManager() : nullptr;

			// --- SO slots: always drawn at their world-space transforms ---
			if (SOSub && AreaData->SmartObjectHandle.IsValid())
			{
				TArray<FSmartObjectSlotHandle> SOSlots;
				SOSub->GetAllSlots(AreaData->SmartObjectHandle, SOSlots);

				for (int32 i = 0; i < SOSlots.Num(); ++i)
				{
					TOptional<FTransform> SlotTransform = SOSub->GetSlotTransform(SOSlots[i]);
					if (!SlotTransform.IsSet())
					{
						continue;
					}

					const ESmartObjectSlotState SOState = SOSub->GetSlotState(SOSlots[i]);

					FArcAreaDebugSlotMarker& Marker = Data.SlotMarkers.AddDefaulted_GetRef();
					Marker.SlotLocation = SlotTransform.GetValue().GetLocation();
					Marker.SlotColor = SOSlotStateWorldColor(SOState);
					Marker.SlotLabel = FString::Printf(TEXT("SO%d [%s]"), i, *SOSlotStateToString(SOState));
					Marker.bSelected = (SelectedSlotIndex == i);
				}
			}

			// --- Area definition slots: fan-out around area center ---
			{
				constexpr float FanRadius = 80.f;
				const FVector Center = AreaData->Location;
				const int32 NumSlots = AreaData->Slots.Num();

				for (int32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
				{
					const FArcAreaSlotRuntime& SlotRuntime = AreaData->Slots[SlotIdx];

					FVector SlotPos;
					if (NumSlots == 1)
					{
						SlotPos = Center + FVector(FanRadius, 0.f, 0.f);
					}
					else
					{
						const float Angle = (2.f * UE_PI * SlotIdx) / NumSlots;
						SlotPos = Center + FVector(FMath::Cos(Angle) * FanRadius, FMath::Sin(Angle) * FanRadius, 0.f);
					}

					FArcAreaDebugSlotMarker& Marker = Data.SlotMarkers.AddDefaulted_GetRef();
					Marker.SlotLocation = SlotPos;
					Marker.SlotColor = SlotStateWorldColor(SlotRuntime.State);
					Marker.bSelected = (SelectedSlotIndex == (SOSub && AreaData->SmartObjectHandle.IsValid()
						? -1 : SlotIdx));

					if (SlotRuntime.AssignedEntity.IsValid())
					{
						Marker.SlotLabel = FString::Printf(TEXT("S%d E%d [%s]"),
							SlotIdx, SlotRuntime.AssignedEntity.Index, *SlotStateToString(SlotRuntime.State));
					}
					else
					{
						Marker.SlotLabel = FString::Printf(TEXT("S%d"), SlotIdx);
					}

					if (SlotRuntime.AssignedEntity.IsValid() && Manager && Manager->IsEntityValid(SlotRuntime.AssignedEntity))
					{
						const FTransformFragment* Transform = Manager->GetFragmentDataPtr<FTransformFragment>(SlotRuntime.AssignedEntity);
						if (Transform)
						{
							Marker.bHasEntity = true;
							Marker.EntityLocation = Transform->GetTransform().GetLocation();
							Marker.LineColor = SlotStateWorldColor(SlotRuntime.State);
						}
					}
				}
			}
		}
	}

	EnsureDrawActor(World);
	if (DrawComponent.IsValid())
	{
		DrawComponent->UpdateAreaData(Data);
	}
}

// ====================================================================
// AreaDebuggerDraw
// ====================================================================

void FArcAreaDebugger::EnsureDrawActor(UWorld* World)
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

#if WITH_EDITOR
	NewActor->SetActorLabel(TEXT("AreaDebuggerDraw"));
#endif

	UArcAreaDebuggerDrawComponent* NewComponent = NewObject<UArcAreaDebuggerDrawComponent>(NewActor, UArcAreaDebuggerDrawComponent::StaticClass());
	NewComponent->RegisterComponent();
	NewActor->AddInstanceComponent(NewComponent);

	DrawActor = NewActor;
	DrawComponent = NewComponent;
}

void FArcAreaDebugger::DestroyDrawActor()
{
	if (DrawActor.IsValid())
	{
		DrawActor->Destroy();
	}
	DrawActor.Reset();
	DrawComponent.Reset();
}
