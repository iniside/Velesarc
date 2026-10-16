// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassProcessorDebugger.h"

#include "imgui.h"

#if WITH_MASSENTITY_DEBUG
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Mass/Debugger.h"
#include "Mass/Element.h"
#include "Mass/ElementAccessTypes.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityQuery.h"
#include "Mass/Processor.h"
#include "Mass/TypeManager.h"
#include "MassEntitySubsystem.h"
#include "MassSystem.h"
#include "Misc/ScopeLock.h"
#include "UObject/Class.h"

namespace Arcx::GameplayDebugger::MassProcessor
{
	const TCHAR* PhaseLabel(const int32 PhaseIndex);
	UWorld* GetDebugWorld();
}

namespace Arcx::GameplayDebugger::MassProcessorDetail
{
	constexpr float EntityListHeight = 220.f;

	const char* AccessString(const EMassFragmentAccess Access)
	{
		switch (Access)
		{
		case EMassFragmentAccess::None: return "None";
		case EMassFragmentAccess::ReadOnly: return "ReadOnly";
		case EMassFragmentAccess::ReadWrite: return "ReadWrite";
		default: return "<unknown>";
		}
	}

	const char* PresenceString(const EMassFragmentPresence Presence)
	{
		switch (Presence)
		{
		case EMassFragmentPresence::All: return "All (required)";
		case EMassFragmentPresence::Any: return "Any";
		case EMassFragmentPresence::None: return "None (forbidden)";
		case EMassFragmentPresence::Optional: return "Optional";
		default: return "<unknown>";
		}
	}

	FMassEntityManager* GetEntityManager()
	{
		UWorld* World = Arcx::GameplayDebugger::MassProcessor::GetDebugWorld();
		if (!World)
		{
			return nullptr;
		}

		UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		return EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;
	}

	FString JoinNames(TConstArrayView<FName> Names)
	{
		if (Names.IsEmpty())
		{
			return FString(TEXT("-"));
		}

		FString Result;
		for (const FName Name : Names)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}
			Result += Name.ToString();
		}
		return Result;
	}

	void DrawRequirementTable(const char* TableId, const char* Label, TConstArrayView<FMassFragmentRequirementDescription> Requirements)
	{
		if (Requirements.IsEmpty())
		{
			ImGui::TextDisabled("%s: none", Label);
			return;
		}

		ImGui::Text("%s (%d)", Label, Requirements.Num());
		if (!ImGui::BeginTable(TableId, 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			return;
		}

		ImGui::TableSetupColumn("Type");
		ImGui::TableSetupColumn("Access");
		ImGui::TableSetupColumn("Presence");
		ImGui::TableHeadersRow();

		for (const FMassFragmentRequirementDescription& Requirement : Requirements)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(Requirement.StructType ? TCHAR_TO_UTF8(*Requirement.StructType->GetName()) : "<null>");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(AccessString(Requirement.AccessMode));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(PresenceString(Requirement.Presence));
		}

		ImGui::EndTable();
	}

	void DrawTagList(const char* Label, const FMassTagBitSet& Tags)
	{
		TArray<const UScriptStruct*> TagTypes;
		Tags.ExportTypes(TagTypes);

		if (TagTypes.IsEmpty())
		{
			ImGui::TextDisabled("%s: none", Label);
			return;
		}

		ImGui::Text("%s (%d)", Label, TagTypes.Num());
		for (const UScriptStruct* TagType : TagTypes)
		{
			ImGui::BulletText("%s", TagType ? TCHAR_TO_UTF8(*TagType->GetName()) : "<null>");
		}
	}

	void DrawSystemRows(const FMassSystemBitSet& Systems, const char* AccessLabel, const FMassEntityManager& EntityManager)
	{
		TArray<const UScriptStruct*> SystemStructs;
		Systems.ExportTypes(SystemStructs);

		for (const UScriptStruct* SystemStruct : SystemStructs)
		{
			if (!SystemStruct)
			{
				continue;
			}

			const UClass* SubsystemClass = EntityManager.GetTypeManager().FindSubsystemForSystemStruct(SystemStruct);
			const FString TypeName = SubsystemClass ? SubsystemClass->GetName() : SystemStruct->GetName();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(TCHAR_TO_UTF8(*TypeName));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(AccessLabel);
		}
	}
} // namespace Arcx::GameplayDebugger::MassProcessorDetail

void FArcMassProcessorDebugger::DrawQueryRequirements(const UE::Mass::Debug::FQueryRequirementsView& Requirements)
{
	using namespace Arcx::GameplayDebugger::MassProcessorDetail;

	DrawRequirementTable("##fragments", "Fragments", Requirements.FragmentRequirements);
	DrawRequirementTable("##chunkfragments", "Chunk fragments", Requirements.ChunkRequirements);
	DrawRequirementTable("##constshared", "Const shared fragments", Requirements.ConstSharedRequirements);
	DrawRequirementTable("##shared", "Shared fragments", Requirements.SharedRequirements);

	ImGui::Spacing();
	DrawTagList("Required tags (All)", Requirements.RequiredAllTags);
	DrawTagList("Required tags (Any)", Requirements.RequiredAnyTags);
	DrawTagList("Forbidden tags (None)", Requirements.RequiredNoneTags);
	DrawTagList("Optional tags", Requirements.RequiredOptionalTags);
}

void FArcMassProcessorDebugger::DrawSubsystemRequirements(const UE::Mass::Debug::FQueryRequirementsView& Requirements, const FMassEntityManager& EntityManager)
{
	using namespace Arcx::GameplayDebugger::MassProcessorDetail;

	const int32 ConstCount = Requirements.RequiredConstSystems.CountStoredTypes();
	const int32 MutableCount = Requirements.RequiredMutableSystems.CountStoredTypes();
	if (ConstCount + MutableCount == 0)
	{
		ImGui::TextDisabled("Subsystems: none");
		return;
	}

	ImGui::Text("Subsystems (%d)", ConstCount + MutableCount);
	if (!ImGui::BeginTable("##subsystems", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		return;
	}

	ImGui::TableSetupColumn("Type");
	ImGui::TableSetupColumn("Access");
	ImGui::TableHeadersRow();

	DrawSystemRows(Requirements.RequiredConstSystems, "Const", EntityManager);
	DrawSystemRows(Requirements.RequiredMutableSystems, "Mutable", EntityManager);

	ImGui::EndTable();
}

void FArcMassProcessorDebugger::RefreshMatchedEntities(TConstArrayView<FMassEntityQuery*> Queries, const FMassEntityManager& EntityManager)
{
	CachedMatchedEntities.Reset();

	TSet<FMassEntityHandle> UniqueEntities;
	for (const FMassEntityQuery* Query : Queries)
	{
		if (!Query)
		{
			continue;
		}

		const TArray<FMassEntityHandle> Matched = FMassDebugger::GetEntitiesMatchingQuery(EntityManager, *Query);
		for (const FMassEntityHandle& Entity : Matched)
		{
			bool bAlreadyPresent = false;
			UniqueEntities.Add(Entity, &bAlreadyPresent);
			if (bAlreadyPresent)
			{
				continue;
			}

			CachedMatchedEntities.Add(Entity);
		}
	}

	CachedEntitiesProcessorKey = SelectedProcessorKey;
	bMatchedEntitiesValid = true;
}

void FArcMassProcessorDebugger::DrawMatchedEntities(TConstArrayView<FMassEntityQuery*> Queries, const FMassEntityManager& EntityManager)
{
	using namespace Arcx::GameplayDebugger::MassProcessorDetail;

	const bool bSelectionChanged = !bMatchedEntitiesValid || CachedEntitiesProcessorKey != SelectedProcessorKey;
	const bool bRefreshRequested = ImGui::Button("Refresh entities");
	if (bSelectionChanged || bRefreshRequested)
	{
		RefreshMatchedEntities(Queries, EntityManager);
	}

	ImGui::SameLine();
	ImGui::Text("Matched entities: %d", CachedMatchedEntities.Num());

	if (ImGui::BeginChild("MassProcMatchedEntities", ImVec2(0.f, EntityListHeight), ImGuiChildFlags_Borders))
	{
		ImGuiListClipper Clipper;
		Clipper.Begin(CachedMatchedEntities.Num());
		while (Clipper.Step())
		{
			for (int32 RowIndex = Clipper.DisplayStart; RowIndex < Clipper.DisplayEnd; ++RowIndex)
			{
				const FMassEntityHandle& Entity = CachedMatchedEntities[RowIndex];
				const FString Label = FString::Printf(TEXT("Entity %d (SN:%d)"), Entity.Index, Entity.SerialNumber);
				ImGui::TextUnformatted(TCHAR_TO_UTF8(*Label));
			}
		}
	}
	ImGui::EndChild();
}

void FArcMassProcessorDebugger::DrawSignalSection(const FArcMassProcessorNode& Node, const UMassProcessor& Processor)
{
	ImGui::Text("Registered signals (%d)", Node.RegisteredSignals.Num());
	for (const FName SignalName : Node.RegisteredSignals)
	{
		ImGui::BulletText("%s", TCHAR_TO_UTF8(*SignalName.ToString()));
	}

	double LastSignalTime = 0.0;
	bool bHasSignalTime = false;
	{
		FScopeLock Lock(&SignalTimesCS);
		if (const double* Found = LastSignalTimes.Find(TObjectKey<UMassProcessor>(&Processor)))
		{
			LastSignalTime = *Found;
			bHasSignalTime = true;
		}
	}

	if (bHasSignalTime)
	{
		ImGui::Text("last raised: %.1fs ago", FPlatformTime::Seconds() - LastSignalTime);
	}
	else
	{
		ImGui::TextDisabled("last raised: never");
	}
}

void FArcMassProcessorDebugger::DrawDetailPanel()
{
	using namespace Arcx::GameplayDebugger::MassProcessorDetail;

	if (!Nodes.IsValidIndex(SelectedNodeIndex))
	{
		ImGui::TextDisabled("Select a processor");
		return;
	}

	const FArcMassProcessorNode& Node = Nodes[SelectedNodeIndex];
	if (!Node.Processor.IsValid())
	{
		ImGui::TextDisabled("Selected processor is no longer valid");
		return;
	}

	UMassProcessor& Processor = *Node.Processor.Get();

	ImGui::TextUnformatted(TCHAR_TO_UTF8(*Node.DisplayName));
	ImGui::Separator();

	ImGui::Text("Class: %s", TCHAR_TO_UTF8(*Processor.GetClass()->GetName()));

	const TCHAR* PhaseName = Arcx::GameplayDebugger::MassProcessor::PhaseLabel(Node.PhaseIndex);
	ImGui::Text("Phase: %d %s", Node.PhaseIndex, PhaseName ? TCHAR_TO_UTF8(PhaseName) : "");
	ImGui::Text("Group: %s", TCHAR_TO_UTF8(*Node.GroupName.ToString()));
	ImGui::Text("Active: %s", Processor.IsActive() ? "yes" : "no");
	ImGui::Text("Owned queries: %d", Processor.GetOwnedQueriesNum());

	const FMassProcessorExecutionOrder& ExecutionOrder = Processor.GetExecutionOrder();
	ImGui::Text("ExecuteInGroup: %s", TCHAR_TO_UTF8(*ExecutionOrder.ExecuteInGroup.ToString()));
	ImGui::Text("ExecuteBefore: %s", TCHAR_TO_UTF8(*JoinNames(ExecutionOrder.ExecuteBefore)));
	ImGui::Text("ExecuteAfter: %s", TCHAR_TO_UTF8(*JoinNames(ExecutionOrder.ExecuteAfter)));

	if (!Node.bScheduled)
	{
		ImGui::TextColored(ImVec4(1.f, 0.65f, 0.25f, 1.f), "not scheduled - pruned by solver (class defaults shown)");
		ImGui::Separator();
		ImGui::TextWrapped(
			"Requirements and matched entities are unavailable for a pruned processor: this node is a class "
			"default object, and a CDO's queries are never configured (ConfigureQueries runs from "
			"UMassProcessor::CallInitialize, which skips class default objects).");

		if (Node.bSignalProcessor)
		{
			ImGui::Separator();
			DrawSignalSection(Node, Processor);
		}
		return;
	}

	FMassEntityManager* EntityManager = GetEntityManager();
	if (!EntityManager)
	{
		ImGui::Separator();
		ImGui::TextDisabled("No entity manager available");
		return;
	}

	TConstArrayView<FMassEntityQuery*> Queries = FMassDebugger::GetUpToDateProcessorQueries(*EntityManager, Processor);

	ImGui::Separator();
	if (Queries.IsEmpty())
	{
		ImGui::TextDisabled("No queries");
	}

	for (int32 QueryIndex = 0; QueryIndex < Queries.Num(); ++QueryIndex)
	{
		const FMassEntityQuery* Query = Queries[QueryIndex];
		if (!Query)
		{
			continue;
		}

		ImGui::PushID(QueryIndex);

		const FString QueryLabel = FString::Printf(TEXT("Query %d"), QueryIndex);
		if (ImGui::CollapsingHeader(TCHAR_TO_UTF8(*QueryLabel)))
		{
			const UE::Mass::Debug::FQueryRequirementsView Requirements = FMassDebugger::GetQueryRequirements(*Query);
			DrawQueryRequirements(Requirements);
			ImGui::Spacing();
			DrawSubsystemRequirements(Requirements, *EntityManager);
		}

		ImGui::PopID();
	}

	ImGui::Separator();
	DrawMatchedEntities(Queries, *EntityManager);

	if (Node.bSignalProcessor)
	{
		ImGui::Separator();
		DrawSignalSection(Node, Processor);
	}
}

#endif // WITH_MASSENTITY_DEBUG
