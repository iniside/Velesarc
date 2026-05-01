// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAbilitiesDebugger.h"

#include "imgui.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeInstanceData.h"
#include "StateTreeNodeBase.h"
#include "ArcImGuiReflectionWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "MassStateTreeSubsystem.h"

namespace
{
	UWorld* GetDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	FMassEntityManager* GetEntityManager()
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
}

void FArcMassAbilitiesDebugger::Initialize()
{
	SelectedEntityIndex = INDEX_NONE;
	AbilitySelectedIndex = INDEX_NONE;
	EffectSelectedIndex = INDEX_NONE;
	AbilitySTSelectedFrameIdx = INDEX_NONE;
	AbilitySTSelectedStateIdx = MAX_uint16;
	bAbilitySTSelectedFrameGlobal = false;
	EffectSTSelectedFrameIdx = INDEX_NONE;
	EffectSTSelectedStateIdx = MAX_uint16;
	bEffectSTSelectedFrameGlobal = false;
	EntityFilterBuf[0] = '\0';
	CachedEntities.Reset();
	LastRefreshTime = 0.f;
	RefreshEntityList();
}

void FArcMassAbilitiesDebugger::Uninitialize()
{
	CachedEntities.Reset();
	SelectedEntityIndex = INDEX_NONE;
	AbilitySelectedIndex = INDEX_NONE;
	EffectSelectedIndex = INDEX_NONE;

#if ARC_MASSABILITIES_DEBUG_EVENTLOG
	UnsubscribeEvents();
#endif
}

void FArcMassAbilitiesDebugger::RefreshEntityList()
{
#if WITH_MASSENTITY_DEBUG
	CachedEntities.Reset();

	FMassEntityManager* Manager = GetEntityManager();
	if (!Manager)
	{
		return;
	}

	TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);

	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);

		const bool bHasAbilities = Composition.Contains<FArcAbilityCollectionFragment>();
		const bool bHasEffects = Composition.Contains<FArcEffectStackFragment>();
		const bool bHasAggregator = Composition.Contains<FArcAggregatorFragment>();

		if (!bHasAbilities && !bHasEffects && !bHasAggregator)
		{
			continue;
		}

		TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle& Entity : Entities)
		{
			FEntityEntry& Entry = CachedEntities.AddDefaulted_GetRef();
			Entry.Entity = Entity;

			if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
			{
				Entry.Location = TransformFrag->GetTransform().GetLocation();
			}

			FString Flags;
			if (bHasAbilities)
			{
				Flags += TEXT("A ");
			}
			if (bHasEffects)
			{
				Flags += TEXT("E ");
			}
			if (bHasAggregator)
			{
				Flags += TEXT("Attr");
			}
			Flags = Flags.TrimEnd();

			Entry.Label = FString::Printf(TEXT("E%d [%s]"), Entity.Index, *Flags);
		}
	}
#endif
}

void FArcMassAbilitiesDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(1400, 700), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Mass Abilities Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	FMassEntityManager* Manager = GetEntityManager();
	if (!Manager)
	{
		ImGui::TextDisabled("No MassEntitySubsystem available");
		ImGui::End();
		return;
	}

	if (ImGui::Button("Refresh"))
	{
		RefreshEntityList();
	}
	ImGui::SameLine();
	ImGui::Text("Entities: %d", CachedEntities.Num());

	UWorld* World = GetDebugWorld();
	if (World)
	{
		float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastRefreshTime > 2.0f)
		{
			LastRefreshTime = CurrentTime;
			RefreshEntityList();
		}
	}

	ImGui::Separator();

	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.18f;

	if (ImGui::BeginChild("EntityListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawEntityListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("DetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
		{
			ImGui::TextDisabled("Select an entity from the list");
		}
		else
		{
			FMassEntityHandle Entity = CachedEntities[SelectedEntityIndex].Entity;
			if (!Manager->IsEntityActive(Entity))
			{
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Entity is no longer active");
			}
			else
			{
				ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", TCHAR_TO_ANSI(*CachedEntities[SelectedEntityIndex].Label));
				ImGui::Spacing();

				if (ImGui::BeginTabBar("MassAbilitiesTabs"))
				{
					if (ImGui::BeginTabItem("Abilities"))
					{
						DrawAbilitiesTab(*Manager, Entity);
						ImGui::EndTabItem();
					}
					if (ImGui::BeginTabItem("Effects"))
					{
						DrawEffectsTab(*Manager, Entity);
						ImGui::EndTabItem();
					}
					if (ImGui::BeginTabItem("Attributes"))
					{
						DrawAttributesTab(*Manager, Entity);
						ImGui::EndTabItem();
					}
#if ARC_MASSABILITIES_DEBUG_EVENTLOG
					if (ImGui::BeginTabItem("Event Log"))
					{
						DrawEventLogTab();
						ImGui::EndTabItem();
					}
#endif
					ImGui::EndTabBar();
				}
			}
		}
	}
	ImGui::EndChild();

	ImGui::End();
#endif
}

void FArcMassAbilitiesDebugger::DrawEntityListPanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("Ability Entities");
	ImGui::Separator();

	ImGui::InputText("Filter", EntityFilterBuf, IM_ARRAYSIZE(EntityFilterBuf));

	FString Filter = FString(ANSI_TO_TCHAR(EntityFilterBuf)).ToLower();

	if (ImGui::BeginChild("AbilityEntityScroll", ImVec2(0, 0)))
	{
		for (int32 i = 0; i < CachedEntities.Num(); ++i)
		{
			const FEntityEntry& Entry = CachedEntities[i];

			if (!Filter.IsEmpty() && !Entry.Label.ToLower().Contains(Filter))
			{
				continue;
			}

			const bool bSelected = (SelectedEntityIndex == i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Entry.Label), bSelected))
			{
				if (SelectedEntityIndex != i)
				{
					SelectedEntityIndex = i;
					AbilitySelectedIndex = INDEX_NONE;
					EffectSelectedIndex = INDEX_NONE;
					AbilitySTSelectedFrameIdx = INDEX_NONE;
					AbilitySTSelectedStateIdx = MAX_uint16;
					bAbilitySTSelectedFrameGlobal = false;
					EffectSTSelectedFrameIdx = INDEX_NONE;
					EffectSTSelectedStateIdx = MAX_uint16;
					bEffectSTSelectedFrameGlobal = false;

#if ARC_MASSABILITIES_DEBUG_EVENTLOG
					UnsubscribeEvents();
					if (CachedEntities.IsValidIndex(i))
					{
						SubscribeEvents(CachedEntities[i].Entity);
					}
#endif
				}
			}
		}
	}
	ImGui::EndChild();
#endif
}

void FArcMassAbilitiesDebugger::DrawStateTreeIntrospection(
	const FStateTreeInstanceData& InstanceData,
	const UStateTree* StateTree,
	int32& SelectedFrameIdx,
	uint16& SelectedStateIdx,
	bool& bSelectedFrameGlobal,
	const char* IdPrefix)
{
#if WITH_MASSENTITY_DEBUG
	const FStateTreeExecutionState* ExecState = InstanceData.GetExecutionState();
	if (!ExecState)
	{
		ImGui::TextDisabled("No execution state");
		return;
	}

	ImGui::Text("Active Frames: %d", ExecState->ActiveFrames.Num());

	int32 IdPrefixHash = static_cast<int32>(GetTypeHash(FString(ANSI_TO_TCHAR(IdPrefix))));

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
		if (bSelectedFrameGlobal && SelectedFrameIdx == FrameIdx)
		{
			FrameNodeFlags |= ImGuiTreeNodeFlags_Selected;
		}

		ImGui::PushID(IdPrefixHash + FrameIdx * 100000);
		bool bFrameOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*FrameLabel), FrameNodeFlags);

		if (ImGui::IsItemClicked())
		{
			SelectedFrameIdx = FrameIdx;
			SelectedStateIdx = MAX_uint16;
			bSelectedFrameGlobal = true;
		}

		if (bFrameOpen)
		{
			TConstArrayView<FCompactStateTreeState> AllStates = Frame.StateTree->GetStates();

			TFunction<void(uint16, uint16)> DrawStateTree = [&](uint16 Begin, uint16 End)
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
					if (SelectedFrameIdx == FrameIdx && SelectedStateIdx == Idx && !bSelectedFrameGlobal)
					{
						NodeFlags |= ImGuiTreeNodeFlags_Selected;
					}

					if (bIsActive)
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
					}

					ImGui::PushID(IdPrefixHash + static_cast<int32>(Idx) + FrameIdx * 10000);
					bool bNodeOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*StateLabel), NodeFlags);

					if (ImGui::IsItemClicked())
					{
						SelectedFrameIdx = FrameIdx;
						SelectedStateIdx = Idx;
						bSelectedFrameGlobal = false;
					}

					ImGui::PopID();

					if (bIsActive)
					{
						ImGui::PopStyleColor();
					}

					if (bHasChildren && bNodeOpen)
					{
						DrawStateTree(State.ChildrenBegin, State.ChildrenEnd);
						ImGui::TreePop();
					}

					Idx = State.GetNextSibling();
				}
			};

			DrawStateTree(0, static_cast<uint16>(AllStates.Num()));
			ImGui::TreePop();
		}

		ImGui::PopID();
	}
#endif
}

void FArcMassAbilitiesDebugger::DrawStateTreeSelectedStateDetails(
	const FStateTreeInstanceData& InstanceData,
	const UStateTree* StateTree,
	int32 SelectedFrameIdx,
	uint16 SelectedStateIdx,
	bool bSelectedFrameGlobal)
{
#if WITH_MASSENTITY_DEBUG
	const FStateTreeExecutionState* ExecState = InstanceData.GetExecutionState();
	if (!ExecState || !ExecState->ActiveFrames.IsValidIndex(SelectedFrameIdx))
	{
		ImGui::TextDisabled("Selected frame is no longer valid");
		return;
	}

	const FStateTreeExecutionFrame& SelFrame = ExecState->ActiveFrames[SelectedFrameIdx];
	if (!SelFrame.StateTree)
	{
		ImGui::TextDisabled("Frame has no state tree");
		return;
	}

	if (bSelectedFrameGlobal)
	{
		const UStateTree* FrameStateTree = SelFrame.StateTree.Get();
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Frame: %s", TCHAR_TO_ANSI(*GetNameSafe(FrameStateTree)));
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Global Parameters", ImGuiTreeNodeFlags_DefaultOpen))
		{
			FConstStructView GlobalParams = InstanceData.GetStorage().GetGlobalParameters();
			if (GlobalParams.IsValid())
			{
				ArcImGuiReflection::DrawStructView(TEXT("Parameters"), GlobalParams);
			}
			else
			{
				ImGui::TextDisabled("(none)");
			}
		}

		int32 GlobalDataBase = SelFrame.GlobalInstanceIndexBase.IsValid() ? SelFrame.GlobalInstanceIndexBase.Get() : -1;

		uint16 EvalsBegin = FrameStateTree->GetGlobalEvaluatorsBegin();
		uint16 EvalsNum = FrameStateTree->GetGlobalEvaluatorsNum();
		if (EvalsNum > 0 && ImGui::CollapsingHeader("Global Evaluators", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (uint16 i = 0; i < EvalsNum; i++)
			{
				int32 NodeIdx = EvalsBegin + i;
				FConstStructView NodeView = FrameStateTree->GetNode(NodeIdx);
				FString NodeLabel = NodeView.IsValid()
					? NodeView.GetScriptStruct()->GetName()
					: FString::Printf(TEXT("Evaluator %d"), i);

				ImGui::PushID(NodeIdx);

				if (ImGui::TreeNodeEx(TCHAR_TO_ANSI(*NodeLabel), ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (GlobalDataBase >= 0 && NodeView.IsValid())
					{
						const FStateTreeNodeBase& NodeBase = NodeView.Get<const FStateTreeNodeBase>();
						if (NodeBase.InstanceDataHandle.IsValid())
						{
							int32 StorageIdx = GlobalDataBase + NodeBase.InstanceDataHandle.GetIndex();
							if (InstanceData.IsValidIndex(StorageIdx) && !InstanceData.IsObject(StorageIdx))
							{
								FConstStructView View = InstanceData.GetStruct(StorageIdx);
								ArcImGuiReflection::DrawStructView(TEXT("Instance"), View);
							}
							else if (InstanceData.IsValidIndex(StorageIdx))
							{
								ImGui::TextDisabled("(UObject instance)");
							}
						}
					}
					ImGui::TreePop();
				}

				ImGui::PopID();
			}
		}

		uint16 TasksBegin = FrameStateTree->GetGlobalTasksBegin();
		uint16 TasksNum = FrameStateTree->GetGlobalTasksNum();
		if (TasksNum > 0 && ImGui::CollapsingHeader("Global Tasks", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (uint16 i = 0; i < TasksNum; i++)
			{
				int32 NodeIdx = TasksBegin + i;
				FConstStructView NodeView = FrameStateTree->GetNode(NodeIdx);
				FString NodeLabel = NodeView.IsValid()
					? NodeView.GetScriptStruct()->GetName()
					: FString::Printf(TEXT("Task %d"), i);

				ImGui::PushID(NodeIdx);

				if (ImGui::TreeNodeEx(TCHAR_TO_ANSI(*NodeLabel), ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (GlobalDataBase >= 0 && NodeView.IsValid())
					{
						const FStateTreeNodeBase& NodeBase = NodeView.Get<const FStateTreeNodeBase>();
						if (NodeBase.InstanceDataHandle.IsValid())
						{
							int32 StorageIdx = GlobalDataBase + NodeBase.InstanceDataHandle.GetIndex();
							if (InstanceData.IsValidIndex(StorageIdx) && !InstanceData.IsObject(StorageIdx))
							{
								FConstStructView View = InstanceData.GetStruct(StorageIdx);
								ArcImGuiReflection::DrawStructView(TEXT("Instance"), View);
							}
							else if (InstanceData.IsValidIndex(StorageIdx))
							{
								ImGui::TextDisabled("(UObject instance)");
							}
						}
					}
					ImGui::TreePop();
				}

				ImGui::PopID();
			}
		}

		return;
	}

	if (!SelFrame.ActiveInstanceIndexBase.IsValid())
	{
		ImGui::TextDisabled("Frame has no active instance data");
		return;
	}

	TConstArrayView<FCompactStateTreeState> SelStates = SelFrame.StateTree->GetStates();
	if (!SelStates.IsValidIndex(SelectedStateIdx))
	{
		ImGui::TextDisabled("Selected state index is invalid");
		return;
	}

	const FCompactStateTreeState& SelState = SelStates[SelectedStateIdx];

	bool bSelStateActive = false;
	for (int32 AS = 0; AS < SelFrame.ActiveStates.Num(); AS++)
	{
		if (SelFrame.ActiveStates[AS].Index == SelectedStateIdx)
		{
			bSelStateActive = true;
			break;
		}
	}

	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Instance Data: %s",
		TCHAR_TO_ANSI(*SelState.Name.ToString()));

	if (!bSelStateActive)
	{
		ImGui::TextDisabled("State is not currently active");
	}
	else if (SelState.InstanceDataNum == 0)
	{
		ImGui::TextDisabled("No instance data for this state");
	}
	else
	{
		int32 RunningIndex = SelFrame.ActiveInstanceIndexBase.Get();
		bool bFound = false;

		for (int32 AS = 0; AS < SelFrame.ActiveStates.Num(); AS++)
		{
			uint16 ActiveIdx = SelFrame.ActiveStates[AS].Index;
			if (ActiveIdx == SelectedStateIdx)
			{
				bFound = true;
				break;
			}
			if (SelStates.IsValidIndex(ActiveIdx))
			{
				RunningIndex += SelStates[ActiveIdx].InstanceDataNum;
			}
		}

		if (bFound)
		{
			for (int32 i = 0; i < SelState.InstanceDataNum; i++)
			{
				int32 StorageIdx = RunningIndex + i;
				if (InstanceData.IsValidIndex(StorageIdx) && !InstanceData.IsObject(StorageIdx))
				{
					FConstStructView View = InstanceData.GetStruct(StorageIdx);
					FString ItemLabel = FString::Printf(TEXT("[%d]"), i);
					ArcImGuiReflection::DrawStructView(*ItemLabel, View);
				}
				else if (InstanceData.IsValidIndex(StorageIdx))
				{
					ImGui::TextDisabled("[%d]: (UObject instance)", i);
				}
			}
		}
	}
#endif
}

