// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebugger.h"

#include "imgui.h"
#include "MassEntitySubsystem.h"
#include "Mass/ArcEconomyFragments.h"
#include "Mass/EntityFragments.h"
#include "MassStateTreeFragments.h"
#include "MassStateTreeSubsystem.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeInstanceData.h"
#include "StateTreeNodeBase.h"
#include "Mass/ArcMassItemFragments.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

namespace Arcx::GameplayDebugger::Economy
{
	extern UWorld* GetDebugWorld();
	extern FMassEntityManager* GetEntityManager();
	extern const ImVec4 DimColor;
	extern const ImVec4 HeaderColor;
} // namespace Arcx::GameplayDebugger::Economy

void FArcEconomyDebugger::DrawNPCDetailPanel()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedNPCIdx == INDEX_NONE || !CachedNPCs.IsValidIndex(SelectedNPCIdx))
	{
		ImGui::TextDisabled("Click an NPC name to inspect");
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FNPCEntry& NPC = CachedNPCs[SelectedNPCIdx];
	if (!Manager->IsEntityActive(NPC.Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "NPC entity is no longer active");
		return;
	}

	ImGui::TextColored(Arcx::GameplayDebugger::Economy::HeaderColor, "NPC: %s", TCHAR_TO_ANSI(*NPC.Label));

	// Location
	FStructView TransformView = Manager->GetFragmentDataStruct(NPC.Entity, FTransformFragment::StaticStruct());
	if (TransformView.IsValid())
	{
		FVector Loc = TransformView.Get<FTransformFragment>().GetTransform().GetLocation();
		ImGui::Text("Location: (%.0f, %.0f, %.0f)", Loc.X, Loc.Y, Loc.Z);
	}

	DrawNPCInventory();
	DrawNPCStateTree();
#endif
}

void FArcEconomyDebugger::DrawNPCInventory()
{
#if WITH_MASSENTITY_DEBUG
	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FNPCEntry& NPC = CachedNPCs[SelectedNPCIdx];

	if (ImGui::CollapsingHeader("Inventory", ImGuiTreeNodeFlags_DefaultOpen))
	{
		FStructView ItemsView = Manager->GetFragmentDataStruct(NPC.Entity, FArcMassItemSpecArrayFragment::StaticStruct());
		if (!ItemsView.IsValid())
		{
			ImGui::TextDisabled("No item fragment");
			return;
		}

		const FArcMassItemSpecArrayFragment& ItemsFrag = ItemsView.Get<FArcMassItemSpecArrayFragment>();

		if (ItemsFrag.Items.IsEmpty())
		{
			ImGui::TextDisabled("Empty");
			return;
		}

		constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("NPCInventory", 2, TableFlags))
		{
			ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 50.f);
			ImGui::TableHeadersRow();

			for (const FArcItemSpec& Spec : ItemsFrag.Items)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				const UArcItemDefinition* ItemDef = Spec.GetItemDefinition();
				FString ItemName = ItemDef ? ItemDef->GetName() : TEXT("(null)");
				ImGui::Text("%s", TCHAR_TO_ANSI(*ItemName));
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%d", Spec.Amount);
			}

			ImGui::EndTable();
		}
	}
#endif
}

void FArcEconomyDebugger::DrawNPCStateTree()
{
#if WITH_MASSENTITY_DEBUG
	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FNPCEntry& NPC = CachedNPCs[SelectedNPCIdx];

	if (!ImGui::CollapsingHeader("State Tree", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	const FMassStateTreeSharedFragment* SharedST = Manager->GetConstSharedFragmentDataPtr<FMassStateTreeSharedFragment>(NPC.Entity);
	const FMassStateTreeInstanceFragment* InstanceST = Manager->GetFragmentDataPtr<FMassStateTreeInstanceFragment>(NPC.Entity);

	if (!SharedST || !SharedST->StateTree)
	{
		ImGui::TextDisabled("No state tree");
		return;
	}

	ImGui::Text("StateTree: %s", TCHAR_TO_ANSI(*SharedST->StateTree->GetName()));

	if (!InstanceST || !InstanceST->InstanceHandle.IsValid())
	{
		ImGui::TextDisabled("No instance handle");
		return;
	}

	UWorld* World = Arcx::GameplayDebugger::Economy::GetDebugWorld();
	UMassStateTreeSubsystem* STSubsystem = World ? World->GetSubsystem<UMassStateTreeSubsystem>() : nullptr;
	if (!STSubsystem)
	{
		return;
	}

	const FStateTreeInstanceData* InstanceData = STSubsystem->GetInstanceData(InstanceST->InstanceHandle);
	if (!InstanceData)
	{
		return;
	}

	const FStateTreeExecutionState* ExecState = InstanceData->GetExecutionState();
	if (!ExecState)
	{
		return;
	}

	ImGui::Text("Active Frames: %d", ExecState->ActiveFrames.Num());

	for (int32 FrameIdx = 0; FrameIdx < ExecState->ActiveFrames.Num(); FrameIdx++)
	{
		const FStateTreeExecutionFrame& Frame = ExecState->ActiveFrames[FrameIdx];
		if (!Frame.StateTree)
		{
			continue;
		}

		FString FrameLabel = FString::Printf(TEXT("Frame %d [%s]"), FrameIdx, *Frame.StateTree->GetName());

		TSet<uint16> ActiveStateIndices;
		for (int32 StateIdx = 0; StateIdx < Frame.ActiveStates.Num(); StateIdx++)
		{
			ActiveStateIndices.Add(Frame.ActiveStates[StateIdx].Index);
		}

		ImGuiTreeNodeFlags FrameNodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
		bool bFrameOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*FrameLabel), FrameNodeFlags);

		if (bFrameOpen)
		{
			TConstArrayView<FCompactStateTreeState> AllStates = Frame.StateTree->GetStates();

			TFunction<void(uint16, uint16)> DrawStateHierarchy = [&, FrameIdx](uint16 Begin, uint16 End)
			{
				for (uint16 Idx = Begin; Idx < End; )
				{
					if (!AllStates.IsValidIndex(Idx))
					{
						break;
					}

					const FCompactStateTreeState& State = AllStates[Idx];
					const bool bIsActive = ActiveStateIndices.Contains(Idx);
					const bool bHasChildren = State.HasChildren();

					FString StateLabel = State.Name.ToString();
					if (State.Type == EStateTreeStateType::Linked || State.Type == EStateTreeStateType::LinkedAsset)
					{
						StateLabel += TEXT(" (Linked)");
					}
					else if (State.Type == EStateTreeStateType::Subtree)
					{
						StateLabel += TEXT(" (Subtree)");
					}

					ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
					if (!bHasChildren)
					{
						NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
					}
					if (bIsActive)
					{
						NodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
					}
					if (SelectedStateFrameIdx == FrameIdx && SelectedStateIdx == Idx)
					{
						NodeFlags |= ImGuiTreeNodeFlags_Selected;
					}

					if (bIsActive)
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
					}

					ImGui::PushID(Idx);
					bool bNodeOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*StateLabel), NodeFlags);

					if (ImGui::IsItemClicked())
					{
						SelectedStateFrameIdx = FrameIdx;
						SelectedStateIdx = Idx;
					}

					ImGui::PopID();

					if (bIsActive)
					{
						ImGui::PopStyleColor();
					}

					if (bHasChildren && bNodeOpen)
					{
						DrawStateHierarchy(State.ChildrenBegin, State.ChildrenEnd);
						ImGui::TreePop();
					}

					Idx = State.GetNextSibling();
				}
			};

			DrawStateHierarchy(0, static_cast<uint16>(AllStates.Num()));
			ImGui::TreePop();
		}
	}
#endif
}
