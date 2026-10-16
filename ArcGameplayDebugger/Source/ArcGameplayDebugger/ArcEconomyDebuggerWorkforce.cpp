// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebugger.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Mass/Debugger.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassActorSubsystem.h"
#include "Operations/ArcMassItemOperations.h"
#include "Sim/ArcBodyUtilityFragments.h"
#include "Sim/ArcEconomyNames.h"
#include "imgui.h"

namespace ArcEconomyDebuggerWorkforcePrivate
{
	const char* BodyKindName(EArcNPCEmploymentKind Kind)
	{
		switch (Kind)
		{
		case EArcNPCEmploymentKind::Work:    return "Worker";
		case EArcNPCEmploymentKind::Gather:  return "Gatherer";
		case EArcNPCEmploymentKind::Carrier: return "Carrier";
		case EArcNPCEmploymentKind::None:    return "None";
		default:                            return "?";
		}
	}

	ImVec4 BodyKindColor(EArcNPCEmploymentKind Kind)
	{
		switch (Kind)
		{
		case EArcNPCEmploymentKind::Work:   return ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
		case EArcNPCEmploymentKind::Gather: return ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
		case EArcNPCEmploymentKind::None:   return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
		default:                     return ImVec4(1.0f, 0.82f, 0.24f, 1.0f);
		}
	}
}

void FArcEconomyDebugger::RefreshNPCList()
{
#if WITH_MASSENTITY_DEBUG
	FMassEntityHandle PreviouslySelected;
	if (CachedNPCs.IsValidIndex(SelectedNPCIdx))
	{
		PreviouslySelected = CachedNPCs[SelectedNPCIdx].Entity;
	}

	CachedNPCs.Reset();
	SelectedNPCIdx = INDEX_NONE;

	FMassEntityManager* Manager = ArcEconomyDebuggerShared::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const UScriptStruct* BodyType = FArcBodyFragment::StaticStruct();

	const TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);
	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		const FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);
		if (!Composition.Contains(BodyType))
		{
			continue;
		}

		const TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle& Entity : Entities)
		{
			if (!Manager->IsEntityActive(Entity))
			{
				continue;
			}

			const FMassEntityView EntityView(*Manager, Entity);

			FNPCEntry& Entry = CachedNPCs.AddDefaulted_GetRef();
			Entry.Entity = Entity;
			Entry.Label = Entity.DebugGetDescription();

			const FMassActorFragment* ActorFragment = EntityView.GetFragmentDataPtr<FMassActorFragment>();
			if (ActorFragment)
			{
				const AActor* Actor = ActorFragment->Get();
				if (Actor)
				{
					Entry.Label = Actor->GetActorNameOrLabel();
				}
			}

			const FTransformFragment* Transform = EntityView.GetFragmentDataPtr<FTransformFragment>();
			if (Transform)
			{
				Entry.Location = Transform->GetTransform().GetLocation();
			}

			const FArcBodyFragment* Body = EntityView.GetFragmentDataPtr<FArcBodyFragment>();
			const FArcWorkAssignmentFragment* Work = EntityView.GetFragmentDataPtr<FArcWorkAssignmentFragment>();
			const FArcGatherAssignmentFragment* Gather = EntityView.GetFragmentDataPtr<FArcGatherAssignmentFragment>();
			const FArcCarrierAssignmentFragment* Carrier = EntityView.GetFragmentDataPtr<FArcCarrierAssignmentFragment>();
			const FArcTransportJobFragment* TransportJob =
				EntityView.GetSparseFragmentDataPtr<FArcTransportJobFragment>();
			const FArcBodyUtilityIntentFragment* UtilityIntent =
				EntityView.GetSparseFragmentDataPtr<FArcBodyUtilityIntentFragment>();
			if (Body)
			{
				Entry.StateText = ArcEconomyDebuggerShared::BodyStateName(Body->State);
			}
			if (TransportJob)
			{
				Entry.Source = TransportJob->Source;
				Entry.Sink = TransportJob->Sink;
				Entry.Item = TransportJob->Item;
				Entry.Quantity = TransportJob->Quantity;
				Entry.bHasTransportJob = true;
				Entry.TransportGeneration = TransportJob->Generation;
				Entry.TransportCommandSerial = TransportJob->ActiveCommandSerial;
				Entry.TransportPhase = TransportJob->Phase;
			}
			if (UtilityIntent && (UtilityIntent->SelectedGoal.IsValid() || UtilityIntent->ActiveCommandSerial != 0))
			{
				Entry.bHasUtilityIntent = true;
				Entry.UtilityGoal = UtilityIntent->SelectedGoal;
				Entry.UtilityCommandSerial = UtilityIntent->ActiveCommandSerial;
			}
			if (Work && Work->Workplace.IsValid())
			{
				Entry.Kind = EArcNPCEmploymentKind::Work;
				Entry.Workplace = Work->Workplace;
			}
			else if (Gather && Gather->Workplace.IsValid())
			{
				Entry.Kind = EArcNPCEmploymentKind::Gather;
				Entry.Workplace = Gather->Workplace;
				Entry.GatherArea = Gather->GatherArea;
				Entry.GatherItem = Gather->Item;
				Entry.GatherQuantity = Gather->Quantity;
				Entry.GatherCommandSerial = Gather->ActiveCommandSerial;
				Entry.GatherPhase = Gather->Phase;
			}
			else if (Carrier && Carrier->bAssigned)
			{
				Entry.Kind = EArcNPCEmploymentKind::Carrier;
			}

			const FArcMassItemStoreFragment* BodyStore = EntityView.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
			if (BodyStore)
			{
				TArray<FPrimaryAssetId> CargoIds;
				for (const FArcMassReplicatedItem& Item : BodyStore->ReplicatedItems.Items)
				{
					const FArcItemData* Data = Item.ToItem();
					if (!Data)
					{
						continue;
					}
					const FPrimaryAssetId DefinitionId = Data->GetItemDefinitionId();
					if (DefinitionId.IsValid())
					{
						CargoIds.AddUnique(DefinitionId);
					}
				}

				for (const FPrimaryAssetId& DefinitionId : CargoIds)
				{
					FStoreLine& Line = Entry.Cargo.AddDefaulted_GetRef();
					Line.Item = DefinitionId;
					Line.Count = ArcMassItems::CountItemsByDefinition(*BodyStore, DefinitionId);
				}
			}
		}
	}

	if (PreviouslySelected.IsValid())
	{
		for (int32 Index = 0; Index < CachedNPCs.Num(); ++Index)
		{
			if (CachedNPCs[Index].Entity == PreviouslySelected)
			{
				SelectedNPCIdx = Index;
				break;
			}
		}
	}
#endif
}

void FArcEconomyDebugger::DrawWorkforceTab()
{
#if WITH_MASSENTITY_DEBUG
	FMassEntityManager* Manager = ArcEconomyDebuggerShared::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	if (CachedNPCs.IsEmpty())
	{
		ImGui::TextDisabled("No settlement bodies in this world");
		return;
	}

	const float AvailableWidth = ImGui::GetContentRegionAvail().x;
	const float AvailableHeight = ImGui::GetContentRegionAvail().y;
	const float SplitterWidth = 6.0f;
	const float ListWidth = FMath::Max(1.0f, AvailableWidth * WorkforceListWidthFraction);

	if (ImGui::BeginChild("BodyTableChild", ImVec2(ListWidth, AvailableHeight), ImGuiChildFlags_None))
	{
		constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_Resizable;

		if (ImGui::BeginTable("BodyTable", 6, TableFlags))
		{
			ImGui::TableSetupColumn("NPC", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Employment", ImGuiTableColumnFlags_WidthFixed, 75.0f);
			ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 130.0f);
			ImGui::TableSetupColumn("Assignment", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Temporary task", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Cargo", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			for (int32 Index = 0; Index < CachedNPCs.Num(); ++Index)
			{
				const FNPCEntry& NPC = CachedNPCs[Index];

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				const FString SelectLabel = FString::Printf(TEXT("%s##body%d"), *NPC.Label, Index);
				if (ImGui::Selectable(TCHAR_TO_ANSI(*SelectLabel), SelectedNPCIdx == Index, ImGuiSelectableFlags_SpanAllColumns))
				{
					SelectedNPCIdx = Index;
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::TextColored(ArcEconomyDebuggerWorkforcePrivate::BodyKindColor(NPC.Kind),
					"%s", ArcEconomyDebuggerWorkforcePrivate::BodyKindName(NPC.Kind));

				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%s", TCHAR_TO_ANSI(*NPC.StateText));

				ImGui::TableSetColumnIndex(3);
				if (NPC.Kind == EArcNPCEmploymentKind::Carrier)
				{
					ImGui::TextUnformatted("Carrier pool");
				}
				else if (NPC.Workplace.IsValid())
				{
					const FString WorkplaceLabel = ArcEconomyNames::BuildingName(*Manager, NPC.Workplace);
					ImGui::Text("%s", TCHAR_TO_ANSI(*WorkplaceLabel));
				}
				else
				{
					ImGui::TextDisabled("---");
				}

				ImGui::TableSetColumnIndex(4);
				if (NPC.bHasUtilityIntent)
				{
					const FString Goal = NPC.UtilityGoal.ToString();
					ImGui::Text("%s (command %u)", TCHAR_TO_ANSI(*Goal), NPC.UtilityCommandSerial);
				}
				else if (NPC.bHasTransportJob)
				{
					ImGui::Text("Transport %s",
						NPC.TransportPhase == EArcTransportJobPhase::Pickup ? "Pickup" : "Drop");
				}
				else if (NPC.Kind == EArcNPCEmploymentKind::Gather)
				{
					ImGui::Text("Gather %s",
						NPC.GatherPhase == EArcGatherAssignmentPhase::Gather ? "Gather" : "Drop");
				}
				else
				{
					ImGui::TextDisabled("---");
				}

				ImGui::TableSetColumnIndex(5);
				if (NPC.Cargo.IsEmpty())
				{
					ImGui::TextDisabled("---");
				}
				else
				{
					for (const FStoreLine& Line : NPC.Cargo)
					{
						ImGui::Text("%s x%d", TCHAR_TO_ANSI(*ArcEconomyNames::ItemName(Line.Item)), Line.Count);
					}
				}
			}

			ImGui::EndTable();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();
	const ImVec2 SplitterMin = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("WorkforceSplitter", ImVec2(SplitterWidth, AvailableHeight));
	const ImVec2 SplitterMax(SplitterMin.x + SplitterWidth, SplitterMin.y + AvailableHeight);
	const ImU32 SplitterColor = ImGui::IsItemActive()
		? IM_COL32(180, 180, 180, 255)
		: (ImGui::IsItemHovered() ? IM_COL32(140, 140, 140, 255) : IM_COL32(90, 90, 90, 255));
	ImGui::GetWindowDrawList()->AddRectFilled(SplitterMin, SplitterMax, SplitterColor);
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	if (ImGui::IsItemActive() && AvailableWidth > 0.0f)
	{
		WorkforceListWidthFraction += ImGui::GetIO().MouseDelta.x / AvailableWidth;
		WorkforceListWidthFraction = FMath::Clamp(WorkforceListWidthFraction, 0.22f, 0.75f);
	}

	ImGui::SameLine();
	if (ImGui::BeginChild("BodyDetailChild", ImVec2(0, AvailableHeight), ImGuiChildFlags_None))
	{
		DrawNPCDetailPanel();
	}
	ImGui::EndChild();
#endif
}
