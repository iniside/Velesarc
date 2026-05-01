// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAIDebugger.h"

#include "imgui.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeFragments.h"
#include "MassStateTreeSubsystem.h"
#include "Fragments/ArcNeedFragment.h"
#include "SmartObjectPlanner/ArcMassGoalPlanInfoSharedFragment.h"
#include "SmartObjectPlanner/ArcSmartObjectPlannerSubsystem.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanDebugData.h"
#include "ArcAIDebuggerDrawComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeInstanceData.h"
#include "StateTreeNodeBase.h"
#include "AsyncGameplayMessageSystem.h"
#include "AsyncMessageWorldSubsystem.h"
#include "ArcMass/Messaging/ArcMassAsyncMessageEndpointFragment.h"
#include "Perception/ArcAISense_GameplayAbility.h"
#include "ArcImGuiReflectionWidget.h"
#include "ArcImGuiStructEditorWidget.h"
#include "ArcGameplayTagTreeWidget.h"
#include "SmartObjectPlanner/Tasks/ArcMassUseSmartObjectTask.h"
#include "SmartObjectPlanner/Tasks/ArcMassExecutePlanStepTask.h"
#include "GameplayBehavior.h"
#include "StateTree/ArcMassExecuteAdvertisementTask.h"

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

void FArcAIDebugger::Initialize()
{
	SelectedEntityIndex = INDEX_NONE;
	EntityFilterBuf[0] = '\0';
	CachedEntities.Reset();
	LastRefreshTime = 0.f;
	RefreshEntityList();

	AsyncMessageTagWidget.Initialize();
	AsyncMessagePayloadEditor.SetBaseStruct(FArcAIBaseEvent::StaticStruct());

	StateTreeEventTagWidget.Initialize();
	StateTreeEventPayloadEditor.SetBaseStruct(FArcAIBaseEvent::StaticStruct());
}

void FArcAIDebugger::Uninitialize()
{
	AsyncMessageTagWidget.Uninitialize();
	AsyncMessagePayloadEditor.Reset();

	StateTreeEventTagWidget.Uninitialize();
	StateTreeEventPayloadEditor.Reset();

	CachedEntities.Reset();
	SelectedEntityIndex = INDEX_NONE;
	DestroyDrawActor();
}

void FArcAIDebugger::RefreshEntityList()
{
	CachedEntities.Reset();

	FMassEntityManager* Manager = GetEntityManager();
	if (!Manager)
	{
		return;
	}

#if WITH_MASSENTITY_DEBUG
	TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);

	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		// Check if this archetype has FMassStateTreeSharedFragment (i.e. it's an AI entity)
		FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);
		if (!Composition.Contains<FMassStateTreeSharedFragment>())
		{
			continue;
		}

		TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle& Entity : Entities)
		{
			FEntityEntry& Entry = CachedEntities.AddDefaulted_GetRef();
			Entry.Entity = Entity;

			// Try to get location
			if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
			{
				Entry.Location = TransformFrag->GetTransform().GetLocation();
			}

			// Try to get state tree name for a better label
			FString StateTreeName = TEXT("Unknown");
			if (const FMassStateTreeSharedFragment* SharedST = Manager->GetConstSharedFragmentDataPtr<FMassStateTreeSharedFragment>(Entity))
			{
				if (SharedST->StateTree)
				{
					StateTreeName = SharedST->StateTree->GetName();
				}
			}

			Entry.Label = FString::Printf(TEXT("E%d [%s]"), Entity.Index, *StateTreeName);
		}
	}
#endif
}

void FArcAIDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(1300, 650), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("AI Debugger", &bShow))
	{
		ImGui::End();
		if (DrawComponent.IsValid()) { DrawComponent->ClearShapes(); }
		return;
	}

	FMassEntityManager* Manager = GetEntityManager();
	if (!Manager)
	{
		ImGui::TextDisabled("No MassEntitySubsystem available");
		ImGui::End();
		return;
	}

	// Refresh button + auto-refresh
	if (ImGui::Button("Refresh"))
	{
		RefreshEntityList();
	}
	ImGui::SameLine();
	ImGui::Text("AI Entities: %d", CachedEntities.Num());

	// Auto-refresh every 2 seconds
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

	// Split into three panes
	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.20f;
	float RightPanelWidth = PanelWidth * 0.30f;

	// Left panel: entity list
	if (ImGui::BeginChild("AIEntityListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawEntityListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Middle panel: entity detail
	float MiddlePanelWidth = ImGui::GetContentRegionAvail().x - RightPanelWidth - ImGui::GetStyle().ItemSpacing.x;
	if (ImGui::BeginChild("AIEntityDetailPanel", ImVec2(MiddlePanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawEntityDetailPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel: selected state details
	if (ImGui::BeginChild("AISelectedStatePanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawSelectedStatePanel();
	}
	ImGui::EndChild();

	ImGui::End();

	// Draw plan in world for selected entity
	DrawPlanInWorld();
#endif
}

void FArcAIDebugger::DrawEntityListPanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("AI Entities");
	ImGui::Separator();

	ImGui::InputText("Filter", EntityFilterBuf, IM_ARRAYSIZE(EntityFilterBuf));

	FString Filter = FString(ANSI_TO_TCHAR(EntityFilterBuf)).ToLower();

	if (ImGui::BeginChild("AIEntityScroll", ImVec2(0, 0)))
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
				SelectedEntityIndex = i;
			}
		}
	}
	ImGui::EndChild();
#endif
}

void FArcAIDebugger::DrawEntityDetailPanel()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		ImGui::TextDisabled("Select an AI entity from the list");
		return;
	}

	FMassEntityManager* Manager = GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FEntityEntry& Entry = CachedEntities[SelectedEntityIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Entity is no longer active");
		return;
	}

	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", TCHAR_TO_ANSI(*Entry.Label));

	// --- Location ---
	if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
	{
		FVector Loc = TransformFrag->GetTransform().GetLocation();
		ImGui::Text("Location: (%.0f, %.0f, %.0f)", Loc.X, Loc.Y, Loc.Z);
	}

	ImGui::Spacing();

	// === STATE TREE INFO ===
	if (ImGui::CollapsingHeader("State Tree", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const FMassStateTreeSharedFragment* SharedST = Manager->GetConstSharedFragmentDataPtr<FMassStateTreeSharedFragment>(Entity);
		const FMassStateTreeInstanceFragment* InstanceST = Manager->GetFragmentDataPtr<FMassStateTreeInstanceFragment>(Entity);

		if (SharedST && SharedST->StateTree)
		{
			ImGui::Text("StateTree: %s", TCHAR_TO_ANSI(*SharedST->StateTree->GetName()));

			if (InstanceST && InstanceST->InstanceHandle.IsValid())
			{
				UWorld* World = GetDebugWorld();
				UMassStateTreeSubsystem* STSubsystem = World ? World->GetSubsystem<UMassStateTreeSubsystem>() : nullptr;

				if (STSubsystem)
				{
					const FStateTreeInstanceData* InstanceData = STSubsystem->GetInstanceData(InstanceST->InstanceHandle);
					if (InstanceData)
					{
						const FStateTreeExecutionState* ExecState = InstanceData->GetExecutionState();
						if (ExecState)
						{
							ImGui::Text("Active Frames: %d", ExecState->ActiveFrames.Num());

							for (int32 FrameIdx = 0; FrameIdx < ExecState->ActiveFrames.Num(); FrameIdx++)
							{
								const FStateTreeExecutionFrame& Frame = ExecState->ActiveFrames[FrameIdx];

								if (!Frame.StateTree)
								{
									continue;
								}

								FString FrameLabel = FString::Printf(TEXT("Frame %d [%s]"), FrameIdx, *Frame.StateTree->GetName());

								// Collect active state handles for this frame
								TSet<uint16> ActiveStateIndices;
								for (int32 StateIdx = 0; StateIdx < Frame.ActiveStates.Num(); StateIdx++)
								{
									ActiveStateIndices.Add(Frame.ActiveStates[StateIdx].Index);
								}

								ImGuiTreeNodeFlags FrameNodeFlags = ImGuiTreeNodeFlags_DefaultOpen;
								if (bSelectedFrameGlobal && SelectedStateFrameIdx == FrameIdx && SelectedNestedType == ENestedTreeType::None)
								{
									FrameNodeFlags |= ImGuiTreeNodeFlags_Selected;
								}

								bool bFrameOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*FrameLabel), FrameNodeFlags);

								if (ImGui::IsItemClicked())
								{
									SelectedStateFrameIdx = FrameIdx;
									SelectedStateIdx = MAX_uint16;
									bSelectedFrameGlobal = true;
									SelectedNestedType = ENestedTreeType::None;
								}

								if (bFrameOpen)
								{
									TConstArrayView<FCompactStateTreeState> AllStates = Frame.StateTree->GetStates();

									// Recursive lambda to draw state hierarchy
									TFunction<void(uint16, uint16)> DrawStateTree = [&, FrameIdx](uint16 Begin, uint16 End)
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

											// Append type suffix for non-standard states
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
												bSelectedFrameGlobal = false;
												SelectedNestedType = ENestedTreeType::None;
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

											// Advance to next sibling
											Idx = State.GetNextSibling();
										}
									};

									// Draw full hierarchy starting from all root-level states
									DrawStateTree(0, static_cast<uint16>(AllStates.Num()));

									ImGui::TreePop();
								}
							}

							// Scan active task instance data for nested Gameplay Interaction state trees
							for (int32 ScanFrameIdx = 0; ScanFrameIdx < ExecState->ActiveFrames.Num(); ScanFrameIdx++)
							{
								const FStateTreeExecutionFrame& ScanFrame = ExecState->ActiveFrames[ScanFrameIdx];
								if (!ScanFrame.StateTree || !ScanFrame.ActiveInstanceIndexBase.IsValid())
								{
									continue;
								}

								TConstArrayView<FCompactStateTreeState> ScanStates = ScanFrame.StateTree->GetStates();
								int32 ScanRunningIndex = ScanFrame.ActiveInstanceIndexBase.Get();

								for (int32 AS = 0; AS < ScanFrame.ActiveStates.Num(); AS++)
								{
									uint16 ActiveIdx = ScanFrame.ActiveStates[AS].Index;
									if (!ScanStates.IsValidIndex(ActiveIdx))
									{
										continue;
									}

									const FCompactStateTreeState& ActiveState = ScanStates[ActiveIdx];

									for (int32 DataIdx = 0; DataIdx < ActiveState.InstanceDataNum; DataIdx++)
									{
										int32 StorageIdx = ScanRunningIndex + DataIdx;
										if (!InstanceData->IsValidIndex(StorageIdx) || InstanceData->IsObject(StorageIdx))
										{
											continue;
										}

										FConstStructView View = InstanceData->GetStruct(StorageIdx);
										if (View.GetScriptStruct() != FArcMassUseSmartObjectTaskInstanceData::StaticStruct())
										{
											continue;
										}

										const FArcMassUseSmartObjectTaskInstanceData& TaskData = View.Get<const FArcMassUseSmartObjectTaskInstanceData>();
										const FArcGameplayInteractionContext& GICtx = TaskData.GameplayInteractionContext;
										const UArcGameplayInteractionSmartObjectBehaviorDefinition* GIDef = GICtx.GetDefinition();

										if (!GIDef)
										{
											continue;
										}

										if (!GIDef->IsA<UArcAIGameplayInteractionSmartObjectBehaviorDefinition>())
										{
											ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Gameplay Interaction: Running");
											continue;
										}

										// Render nested Gameplay Interaction state tree
										const FStateTreeInstanceData& NestedInstanceData = GICtx.GetStateTreeInstanceData();
										const FStateTreeExecutionState* NestedExecState = NestedInstanceData.GetExecutionState();
										if (!NestedExecState)
										{
											continue;
										}

										for (int32 NFrameIdx = 0; NFrameIdx < NestedExecState->ActiveFrames.Num(); NFrameIdx++)
										{
											const FStateTreeExecutionFrame& NFrame = NestedExecState->ActiveFrames[NFrameIdx];
											if (!NFrame.StateTree)
											{
												continue;
											}

											FString NFrameLabel = FString::Printf(TEXT("Gameplay Interaction [%s]"), *NFrame.StateTree->GetName());

											TSet<uint16> NActiveStateIndices;
											for (int32 NAS = 0; NAS < NFrame.ActiveStates.Num(); NAS++)
											{
												NActiveStateIndices.Add(NFrame.ActiveStates[NAS].Index);
											}

											ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
											bool bNFrameOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*NFrameLabel), ImGuiTreeNodeFlags_DefaultOpen);
											ImGui::PopStyleColor();

											if (bNFrameOpen)
											{
												TConstArrayView<FCompactStateTreeState> NAllStates = NFrame.StateTree->GetStates();

												TFunction<void(uint16, uint16)> DrawNestedStates = [&](uint16 Begin, uint16 End)
												{
													for (uint16 Idx = Begin; Idx < End; )
													{
														if (!NAllStates.IsValidIndex(Idx))
														{
															break;
														}

														const FCompactStateTreeState& State = NAllStates[Idx];
														const bool bIsActive = NActiveStateIndices.Contains(Idx);
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
														if (SelectedNestedType == ENestedTreeType::SmartObject && NestedTaskStorageIdx == StorageIdx
															&& SelectedStateFrameIdx == NFrameIdx && SelectedStateIdx == Idx)
														{
															NodeFlags |= ImGuiTreeNodeFlags_Selected;
														}

														if (bIsActive)
														{
															ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
														}

														ImGui::PushID(static_cast<int>(Idx) + 10000 * (NFrameIdx + 1));
														bool bNodeOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*StateLabel), NodeFlags);

														if (ImGui::IsItemClicked())
														{
															SelectedStateFrameIdx = NFrameIdx;
															SelectedStateIdx = Idx;
															SelectedNestedType = ENestedTreeType::SmartObject;
															NestedTaskStorageIdx = StorageIdx;
														}

														ImGui::PopID();

														if (bIsActive)
														{
															ImGui::PopStyleColor();
														}

														if (bHasChildren && bNodeOpen)
														{
															DrawNestedStates(State.ChildrenBegin, State.ChildrenEnd);
															ImGui::TreePop();
														}

														Idx = State.GetNextSibling();
													}
												};

												DrawNestedStates(0, static_cast<uint16>(NAllStates.Num()));
												ImGui::TreePop();
											}
										}
									}

									ScanRunningIndex += ActiveState.InstanceDataNum;
								}
							}

							// Scan active task instance data for nested Advertisement state trees
							for (int32 ScanFrameIdx = 0; ScanFrameIdx < ExecState->ActiveFrames.Num(); ScanFrameIdx++)
							{
								const FStateTreeExecutionFrame& ScanFrame = ExecState->ActiveFrames[ScanFrameIdx];
								if (!ScanFrame.StateTree || !ScanFrame.ActiveInstanceIndexBase.IsValid())
								{
									continue;
								}

								TConstArrayView<FCompactStateTreeState> ScanStates = ScanFrame.StateTree->GetStates();
								int32 ScanRunningIndex = ScanFrame.ActiveInstanceIndexBase.Get();

								for (int32 AS = 0; AS < ScanFrame.ActiveStates.Num(); AS++)
								{
									uint16 ActiveIdx = ScanFrame.ActiveStates[AS].Index;
									if (!ScanStates.IsValidIndex(ActiveIdx))
									{
										continue;
									}

									const FCompactStateTreeState& ActiveState = ScanStates[ActiveIdx];

									for (int32 DataIdx = 0; DataIdx < ActiveState.InstanceDataNum; DataIdx++)
									{
										int32 StorageIdx = ScanRunningIndex + DataIdx;
										if (!InstanceData->IsValidIndex(StorageIdx) || InstanceData->IsObject(StorageIdx))
										{
											continue;
										}

										FConstStructView View = InstanceData->GetStruct(StorageIdx);
										if (View.GetScriptStruct() != FArcMassExecuteAdvertisementTaskInstanceData::StaticStruct())
										{
											continue;
										}

										const FArcMassExecuteAdvertisementTaskInstanceData& TaskData = View.Get<const FArcMassExecuteAdvertisementTaskInstanceData>();
										const FArcAdvertisementExecutionContext& AdvCtx = TaskData.ExecutionContext;

										if (!AdvCtx.IsValid())
										{
											continue;
										}

										const FStateTreeInstanceData& NestedInstanceData = AdvCtx.GetStateTreeInstanceData();
										const FStateTreeExecutionState* NestedExecState = NestedInstanceData.GetExecutionState();
										if (!NestedExecState)
										{
											continue;
										}

										for (int32 NFrameIdx = 0; NFrameIdx < NestedExecState->ActiveFrames.Num(); NFrameIdx++)
										{
											const FStateTreeExecutionFrame& NFrame = NestedExecState->ActiveFrames[NFrameIdx];
											if (!NFrame.StateTree)
											{
												continue;
											}

											FString NFrameLabel = FString::Printf(TEXT("Advertisement [%s]"), *NFrame.StateTree->GetName());

											TSet<uint16> NActiveStateIndices;
											for (int32 NAS = 0; NAS < NFrame.ActiveStates.Num(); NAS++)
											{
												NActiveStateIndices.Add(NFrame.ActiveStates[NAS].Index);
											}

											ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
											bool bNFrameOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*NFrameLabel), ImGuiTreeNodeFlags_DefaultOpen);
											ImGui::PopStyleColor();

											if (bNFrameOpen)
											{
												TConstArrayView<FCompactStateTreeState> NAllStates = NFrame.StateTree->GetStates();

												TFunction<void(uint16, uint16)> DrawNestedStates = [&](uint16 Begin, uint16 End)
												{
													for (uint16 Idx = Begin; Idx < End; )
													{
														if (!NAllStates.IsValidIndex(Idx))
														{
															break;
														}

														const FCompactStateTreeState& State = NAllStates[Idx];
														const bool bIsActive = NActiveStateIndices.Contains(Idx);
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
														if (SelectedNestedType == ENestedTreeType::Advertisement && NestedTaskStorageIdx == StorageIdx
															&& SelectedStateFrameIdx == NFrameIdx && SelectedStateIdx == Idx)
														{
															NodeFlags |= ImGuiTreeNodeFlags_Selected;
														}

														if (bIsActive)
														{
															ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
														}

														ImGui::PushID(static_cast<int>(Idx) + 20000 * (NFrameIdx + 1));
														bool bNodeOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*StateLabel), NodeFlags);

														if (ImGui::IsItemClicked())
														{
															SelectedStateFrameIdx = NFrameIdx;
															SelectedStateIdx = Idx;
															SelectedNestedType = ENestedTreeType::Advertisement;
															NestedTaskStorageIdx = StorageIdx;
														}

														ImGui::PopID();

														if (bIsActive)
														{
															ImGui::PopStyleColor();
														}

														if (bHasChildren && bNodeOpen)
														{
															DrawNestedStates(State.ChildrenBegin, State.ChildrenEnd);
															ImGui::TreePop();
														}

														Idx = State.GetNextSibling();
													}
												};

												DrawNestedStates(0, static_cast<uint16>(NAllStates.Num()));
												ImGui::TreePop();
											}
										}
									}

									ScanRunningIndex += ActiveState.InstanceDataNum;
								}
							}

							// Scan for ExecutePlanStep nested state trees (unified SO + Knowledge path)
							for (int32 ScanFrameIdx = 0; ScanFrameIdx < ExecState->ActiveFrames.Num(); ScanFrameIdx++)
							{
								const FStateTreeExecutionFrame& ScanFrame = ExecState->ActiveFrames[ScanFrameIdx];
								if (!ScanFrame.StateTree || !ScanFrame.ActiveInstanceIndexBase.IsValid())
								{
									continue;
								}

								TConstArrayView<FCompactStateTreeState> ScanStates = ScanFrame.StateTree->GetStates();
								int32 ScanRunningIndex = ScanFrame.ActiveInstanceIndexBase.Get();

								for (int32 AS = 0; AS < ScanFrame.ActiveStates.Num(); AS++)
								{
									uint16 ActiveIdx = ScanFrame.ActiveStates[AS].Index;
									if (!ScanStates.IsValidIndex(ActiveIdx))
									{
										continue;
									}

									const FCompactStateTreeState& ActiveState = ScanStates[ActiveIdx];

									for (int32 DataIdx = 0; DataIdx < ActiveState.InstanceDataNum; DataIdx++)
									{
										int32 StorageIdx = ScanRunningIndex + DataIdx;
										if (!InstanceData->IsValidIndex(StorageIdx) || InstanceData->IsObject(StorageIdx))
										{
											continue;
										}

										FConstStructView View = InstanceData->GetStruct(StorageIdx);
										if (View.GetScriptStruct() != FArcMassExecutePlanStepTaskInstanceData::StaticStruct())
										{
											continue;
										}

										const FArcMassExecutePlanStepTaskInstanceData& TaskData = View.Get<const FArcMassExecutePlanStepTaskInstanceData>();

										// Knowledge path — show advertisement StateTree
										if (TaskData.AdvertisementHandle.IsValid() && TaskData.AdvertisementExecutionContext.IsValid())
										{
											const FStateTreeInstanceData& NestedInstanceData = TaskData.AdvertisementExecutionContext.GetStateTreeInstanceData();
											const FStateTreeExecutionState* NestedExecState = NestedInstanceData.GetExecutionState();
											if (NestedExecState)
											{
												for (int32 NFrameIdx = 0; NFrameIdx < NestedExecState->ActiveFrames.Num(); NFrameIdx++)
												{
													const FStateTreeExecutionFrame& NFrame = NestedExecState->ActiveFrames[NFrameIdx];
													if (!NFrame.StateTree)
													{
														continue;
													}

													FString NFrameLabel = FString::Printf(TEXT("Plan Step: Advertisement [%s]"), *NFrame.StateTree->GetName());

													TSet<uint16> NActiveStateIndices;
													for (int32 NAS = 0; NAS < NFrame.ActiveStates.Num(); NAS++)
													{
														NActiveStateIndices.Add(NFrame.ActiveStates[NAS].Index);
													}

													ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
													bool bNFrameOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*NFrameLabel), ImGuiTreeNodeFlags_DefaultOpen);
													ImGui::PopStyleColor();

													if (bNFrameOpen)
													{
														TConstArrayView<FCompactStateTreeState> NAllStates = NFrame.StateTree->GetStates();

														TFunction<void(uint16, uint16)> DrawNestedStates = [&](uint16 Begin, uint16 End)
														{
															for (uint16 Idx = Begin; Idx < End; )
															{
																if (!NAllStates.IsValidIndex(Idx))
																{
																	break;
																}

																const FCompactStateTreeState& State = NAllStates[Idx];
																const bool bIsActive = NActiveStateIndices.Contains(Idx);
																const bool bHasChildren = State.HasChildren();

																FString StateLabel = State.Name.ToString();

																ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
																if (!bHasChildren)
																{
																	NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
																}
																if (bIsActive)
																{
																	NodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
																}

																if (bIsActive)
																{
																	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
																}

																ImGui::PushID(static_cast<int>(Idx) + 30000 * (NFrameIdx + 1));
																bool bNodeOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*StateLabel), NodeFlags);
																ImGui::PopID();

																if (bIsActive)
																{
																	ImGui::PopStyleColor();
																}

																if (bHasChildren && bNodeOpen)
																{
																	DrawNestedStates(State.ChildrenBegin, State.ChildrenEnd);
																	ImGui::TreePop();
																}

																Idx = State.GetNextSibling();
															}
														};

														DrawNestedStates(0, static_cast<uint16>(NAllStates.Num()));
														ImGui::TreePop();
													}
												}
											}
										}
										// SO path — show GameplayInteraction StateTree or behavior name
										else
										{
											const FArcGameplayInteractionContext& GICtx = TaskData.GameplayInteractionContext;
											const UArcGameplayInteractionSmartObjectBehaviorDefinition* GIDef = GICtx.GetDefinition();

											if (GIDef && GIDef->IsA<UArcAIGameplayInteractionSmartObjectBehaviorDefinition>())
											{
												const FStateTreeInstanceData& NestedInstanceData = GICtx.GetStateTreeInstanceData();
												const FStateTreeExecutionState* NestedExecState = NestedInstanceData.GetExecutionState();
												if (NestedExecState)
												{
													for (int32 NFrameIdx = 0; NFrameIdx < NestedExecState->ActiveFrames.Num(); NFrameIdx++)
													{
														const FStateTreeExecutionFrame& NFrame = NestedExecState->ActiveFrames[NFrameIdx];
														if (!NFrame.StateTree)
														{
															continue;
														}

														FString NFrameLabel = FString::Printf(TEXT("Plan Step: SO Interaction [%s]"), *NFrame.StateTree->GetName());

														TSet<uint16> NActiveStateIndices;
														for (int32 NAS = 0; NAS < NFrame.ActiveStates.Num(); NAS++)
														{
															NActiveStateIndices.Add(NFrame.ActiveStates[NAS].Index);
														}

														ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
														bool bNFrameOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*NFrameLabel), ImGuiTreeNodeFlags_DefaultOpen);
														ImGui::PopStyleColor();

														if (bNFrameOpen)
														{
															TConstArrayView<FCompactStateTreeState> NAllStates = NFrame.StateTree->GetStates();

															TFunction<void(uint16, uint16)> DrawNestedStates = [&](uint16 Begin, uint16 End)
															{
																for (uint16 Idx = Begin; Idx < End; )
																{
																	if (!NAllStates.IsValidIndex(Idx))
																	{
																		break;
																	}

																	const FCompactStateTreeState& State = NAllStates[Idx];
																	const bool bIsActive = NActiveStateIndices.Contains(Idx);
																	const bool bHasChildren = State.HasChildren();

																	FString StateLabel = State.Name.ToString();

																	ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
																	if (!bHasChildren)
																	{
																		NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
																	}
																	if (bIsActive)
																	{
																		NodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
																	}

																	if (bIsActive)
																	{
																		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
																	}

																	ImGui::PushID(static_cast<int>(Idx) + 40000 * (NFrameIdx + 1));
																	bool bNodeOpen = ImGui::TreeNodeEx(TCHAR_TO_ANSI(*StateLabel), NodeFlags);
																	ImGui::PopID();

																	if (bIsActive)
																	{
																		ImGui::PopStyleColor();
																	}

																	if (bHasChildren && bNodeOpen)
																	{
																		DrawNestedStates(State.ChildrenBegin, State.ChildrenEnd);
																		ImGui::TreePop();
																	}

																	Idx = State.GetNextSibling();
																}
															};

															DrawNestedStates(0, static_cast<uint16>(NAllStates.Num()));
															ImGui::TreePop();
														}
													}
												}
											}
											else if (GIDef)
											{
												ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Plan Step: SO Interaction (Running)");
											}
											else if (TaskData.GameplayBehavior)
											{
												ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Plan Step: Behavior [%s]",
													TCHAR_TO_ANSI(*TaskData.GameplayBehavior->GetName()));
											}
											else
											{
												ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Plan Step: Mass Behavior");
											}
										}
									}

									ScanRunningIndex += ActiveState.InstanceDataNum;
								}
							}

						}
					}
				}

				ImGui::TextDisabled("Last Update: %.2fs", InstanceST->LastUpdateTimeInSeconds);
				
				//TODO:: Check if have FArcMassTickStateTreeTag instead.
				//ImGui::TextDisabled("Should Tick: %s", InstanceST->bShouldTick ? "true" : "false");
			}
			else
			{
				ImGui::TextDisabled("No valid instance handle");
			}
		}
		else
		{
			ImGui::TextDisabled("No StateTree assigned");
		}
	}

	// === SMART OBJECT PLAN ===
	if (ImGui::CollapsingHeader("Smart Object Plan", ImGuiTreeNodeFlags_DefaultOpen))
	{
		UWorld* World = GetDebugWorld();
		UArcSmartObjectPlannerSubsystem* PlannerSubsystem = World ? World->GetSubsystem<UArcSmartObjectPlannerSubsystem>() : nullptr;

		if (PlannerSubsystem)
		{
			const FArcSmartObjectPlanContainer* Plan = PlannerSubsystem->GetDebugPlan(Entity);
			if (Plan && Plan->Items.Num() > 0)
			{
				ImGui::Text("Plan Steps: %d", Plan->Items.Num());
				ImGui::Separator();

				for (int32 StepIdx = 0; StepIdx < Plan->Items.Num(); StepIdx++)
				{
					const FArcSmartObjectPlanStep& Step = Plan->Items[StepIdx];

					ImGui::PushID(StepIdx);

					FString StepHeader = FString::Printf(TEXT("Step %d"), StepIdx);
					if (ImGui::TreeNodeEx(TCHAR_TO_ANSI(*StepHeader), ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Text("Location: (%.0f, %.0f, %.0f)", Step.Location.X, Step.Location.Y, Step.Location.Z);
						ImGui::Text("Entity: %s", TCHAR_TO_ANSI(*Step.EntityHandle.DebugGetDescription()));

						if (!Step.Requires.IsEmpty())
						{
							ImGui::Text("Requires: %s", TCHAR_TO_ANSI(*Step.Requires.ToStringSimple()));
						}

						ImGui::Text("Candidate Slots: %d", Step.FoundCandidateSlots.NumSlots);

						// Show GoalPlanInfo for this step's entity (if it's active)
						if (Step.EntityHandle.IsSet() && Manager->IsEntityActive(Step.EntityHandle))
						{
							const FArcMassGoalPlanInfoSharedFragment* GoalInfo =
								Manager->GetConstSharedFragmentDataPtr<FArcMassGoalPlanInfoSharedFragment>(Step.EntityHandle);
							if (GoalInfo)
							{
								if (!GoalInfo->Provides.IsEmpty())
								{
									ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Provides: %s",
										TCHAR_TO_ANSI(*GoalInfo->Provides.ToStringSimple()));
								}
								if (!GoalInfo->Requires.IsEmpty())
								{
									ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Requires: %s",
										TCHAR_TO_ANSI(*GoalInfo->Requires.ToStringSimple()));
								}
							}
						}

						ImGui::TreePop();
					}

					ImGui::PopID();
				}
			}
			else
			{
				ImGui::TextDisabled("No active plan for this entity");
			}

#if !UE_BUILD_SHIPPING
			const FArcSmartObjectPlanDebugData* Diag = PlannerSubsystem->GetDebugDiagnostics(Entity);
			if (Diag)
			{
				ImGui::Separator();

				if (ImGui::TreeNodeEx("Query", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Text("Origin: (%.0f, %.0f, %.0f)", Diag->SearchOrigin.X, Diag->SearchOrigin.Y, Diag->SearchOrigin.Z);
					ImGui::Text("Radius: %.0f", Diag->SearchRadius);
					ImGui::Text("Required: %s", TCHAR_TO_ANSI(*Diag->RequiredTags.ToStringSimple()));
					ImGui::Text("Initial: %s", TCHAR_TO_ANSI(*Diag->InitialTags.ToStringSimple()));
					ImGui::Text("Plans Found: %d", Diag->PlansFound);

					if (Diag->bHitDepthLimit)
					{
						ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "WARNING: Hit 20-step depth limit");
					}

					if (!Diag->UnsatisfiedTags.IsEmpty())
					{
						ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Unsatisfied: %s",
							TCHAR_TO_ANSI(*Diag->UnsatisfiedTags.ToStringSimple()));
					}

					ImGui::TreePop();
				}

				FString CandHeader = FString::Printf(TEXT("Candidates (%d)"), Diag->Candidates.Num());
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*CandHeader)))
				{
					for (int32 i = 0; i < Diag->Candidates.Num(); i++)
					{
						const FArcPlanCandidateDebugEntry& Cand = Diag->Candidates[i];
						ImGui::PushID(i);

						ImVec4 Color;
						const TCHAR* RejectionStr;
						switch (Cand.Rejection)
						{
						case EArcPlanCandidateRejection::None:
							Color = ImVec4(0.5f, 1.f, 0.5f, 1.f);
							RejectionStr = TEXT("OK");
							break;
						case EArcPlanCandidateRejection::NoSlots:
							Color = ImVec4(1.f, 0.4f, 0.4f, 1.f);
							RejectionStr = TEXT("No Slots");
							break;
						case EArcPlanCandidateRejection::CustomConditionFailed:
							Color = ImVec4(1.f, 0.4f, 0.4f, 1.f);
							RejectionStr = TEXT("Condition Failed");
							break;
						case EArcPlanCandidateRejection::NoNewTags:
							Color = ImVec4(0.8f, 0.8f, 0.3f, 1.f);
							RejectionStr = TEXT("No New Tags");
							break;
						case EArcPlanCandidateRejection::RequirementConflict:
							Color = ImVec4(1.f, 0.5f, 0.2f, 1.f);
							RejectionStr = TEXT("Req Conflict");
							break;
						case EArcPlanCandidateRejection::RequirementUnsatisfiable:
							Color = ImVec4(1.f, 0.3f, 0.3f, 1.f);
							RejectionStr = TEXT("Req Unsatisfiable");
							break;
						default:
							Color = ImVec4(1.f, 1.f, 1.f, 1.f);
							RejectionStr = TEXT("Unknown");
							break;
						}

						FString NodeLabel = FString::Printf(TEXT("%s [%s]"),
							*Cand.EntityHandle.DebugGetDescription(), RejectionStr);

						ImGui::PushStyleColor(ImGuiCol_Text, Color);
						bool bOpen = ImGui::TreeNode(TCHAR_TO_ANSI(*NodeLabel));
						ImGui::PopStyleColor();

						if (bOpen)
						{
							ImGui::Text("Location: (%.0f, %.0f, %.0f)", Cand.Location.X, Cand.Location.Y, Cand.Location.Z);
							ImGui::Text("Slots: %d", Cand.SlotCount);

							if (!Cand.Provides.IsEmpty())
							{
								ImGui::TextColored(ImVec4(0.5f, 1.f, 0.5f, 1.f), "Provides: %s",
									TCHAR_TO_ANSI(*Cand.Provides.ToStringSimple()));
							}
							if (!Cand.Requires.IsEmpty())
							{
								ImGui::TextColored(ImVec4(1.f, 0.7f, 0.3f, 1.f), "Requires: %s",
									TCHAR_TO_ANSI(*Cand.Requires.ToStringSimple()));
							}
							if (!Cand.RejectionDetail.IsEmpty())
							{
								ImGui::TextColored(ImVec4(1.f, 0.5f, 0.5f, 1.f), "Detail: %s",
									TCHAR_TO_ANSI(*Cand.RejectionDetail));
							}
							if (!Cand.SensorSource.IsEmpty())
							{
								ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.9f, 1.f), "Sensor: %s",
									TCHAR_TO_ANSI(*Cand.SensorSource));
							}

							ImGui::TreePop();
						}

						ImGui::PopID();
					}

					ImGui::TreePop();
				}
			}
#endif // !UE_BUILD_SHIPPING
		}
		else
		{
			ImGui::TextDisabled("PlannerSubsystem not available");
		}
	}

	// === NEEDS ===
	if (ImGui::CollapsingHeader("Needs"))
	{
		bool bHasAnyNeeds = false;

		if (const FArcHungerNeedFragment* Hunger = Manager->GetFragmentDataPtr<FArcHungerNeedFragment>(Entity))
		{
			bHasAnyNeeds = true;
			float Ratio = Hunger->CurrentValue / 100.f;
			ImGui::Text("Hunger");
			ImGui::SameLine(150);
			ImGui::ProgressBar(Ratio, ImVec2(200, 0));
			ImGui::SameLine();
			ImGui::Text("%.3f (Rate: %.4f)", Hunger->CurrentValue, Hunger->ChangeRate);
		}

		if (const FArcThirstNeedFragment* Thirst = Manager->GetFragmentDataPtr<FArcThirstNeedFragment>(Entity))
		{
			bHasAnyNeeds = true;
			float Ratio = Thirst->CurrentValue / 100.f;
			ImGui::Text("Thirst");
			ImGui::SameLine(150);
			ImGui::ProgressBar(Ratio, ImVec2(200, 0));
			ImGui::SameLine();
			ImGui::Text("%.3f (Rate: %.4f)", Thirst->CurrentValue, Thirst->ChangeRate);
		}

		if (const FArcFatigueNeedFragment* Fatigue = Manager->GetFragmentDataPtr<FArcFatigueNeedFragment>(Entity))
		{
			bHasAnyNeeds = true;
			float Ratio = Fatigue->CurrentValue / 100.f;
			ImGui::Text("Fatigue");
			ImGui::SameLine(150);
			ImGui::ProgressBar(Ratio, ImVec2(200, 0));
			ImGui::SameLine();
			ImGui::Text("%.3f (Rate: %.4f)", Fatigue->CurrentValue, Fatigue->ChangeRate);
		}

		if (!bHasAnyNeeds)
		{
			ImGui::TextDisabled("No needs fragments on this entity");
		}
	}

	// === GOAL PLAN INFO (ConstShared) ===
	if (ImGui::CollapsingHeader("Goal Plan Info"))
	{
		const FArcMassGoalPlanInfoSharedFragment* GoalInfo =
			Manager->GetConstSharedFragmentDataPtr<FArcMassGoalPlanInfoSharedFragment>(Entity);
		if (GoalInfo)
		{
			if (!GoalInfo->Provides.IsEmpty())
			{
				ImGui::Text("Provides: %s", TCHAR_TO_ANSI(*GoalInfo->Provides.ToStringSimple()));
			}
			else
			{
				ImGui::TextDisabled("Provides: (none)");
			}

			if (!GoalInfo->Requires.IsEmpty())
			{
				ImGui::Text("Requires: %s", TCHAR_TO_ANSI(*GoalInfo->Requires.ToStringSimple()));
			}
			else
			{
				ImGui::TextDisabled("Requires: (none)");
			}

			ImGui::Text("Custom Conditions: %d", GoalInfo->CustomConditions.Num());
		}
		else
		{
			ImGui::TextDisabled("No GoalPlanInfo on this entity");
		}
	}

	// === SEND ACTIONS ===
	if (ImGui::CollapsingHeader("Actions"))
	{
		DrawSendAsyncMessage();
		ImGui::Spacing();
		DrawSendStateTreeEvent();
	}
#endif
}

void FArcAIDebugger::DrawSelectedStatePanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("Selected State Details");
	ImGui::Separator();

	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		ImGui::TextDisabled("Select an AI entity from the list");
		return;
	}

	if (SelectedStateFrameIdx == INDEX_NONE || (!bSelectedFrameGlobal && SelectedStateIdx == MAX_uint16))
	{
		ImGui::TextDisabled("Select a state from the tree");
		return;
	}

	FMassEntityManager* Manager = GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FEntityEntry& Entry = CachedEntities[SelectedEntityIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		ImGui::TextDisabled("Entity is no longer active");
		return;
	}

	const FMassStateTreeInstanceFragment* InstanceST = Manager->GetFragmentDataPtr<FMassStateTreeInstanceFragment>(Entity);
	if (!InstanceST || !InstanceST->InstanceHandle.IsValid())
	{
		ImGui::TextDisabled("No valid instance handle");
		return;
	}

	UWorld* World = GetDebugWorld();
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

	// If selection is in a nested Gameplay Interaction tree, resolve to nested instance data
	if (SelectedNestedType == ENestedTreeType::SmartObject)
	{
		if (!InstanceData->IsValidIndex(NestedTaskStorageIdx) || InstanceData->IsObject(NestedTaskStorageIdx))
		{
			ImGui::TextDisabled("Nested task data no longer valid");
			return;
		}
		FConstStructView TaskView = InstanceData->GetStruct(NestedTaskStorageIdx);
		if (TaskView.GetScriptStruct() != FArcMassUseSmartObjectTaskInstanceData::StaticStruct())
		{
			ImGui::TextDisabled("Nested task data type mismatch");
			return;
		}
		const FArcMassUseSmartObjectTaskInstanceData& TaskData = TaskView.Get<const FArcMassUseSmartObjectTaskInstanceData>();
		InstanceData = &TaskData.GameplayInteractionContext.GetStateTreeInstanceData();
	}
	else if (SelectedNestedType == ENestedTreeType::Advertisement)
	{
		if (!InstanceData->IsValidIndex(NestedTaskStorageIdx) || InstanceData->IsObject(NestedTaskStorageIdx))
		{
			ImGui::TextDisabled("Nested advertisement data no longer valid");
			return;
		}
		FConstStructView TaskView = InstanceData->GetStruct(NestedTaskStorageIdx);
		if (TaskView.GetScriptStruct() != FArcMassExecuteAdvertisementTaskInstanceData::StaticStruct())
		{
			ImGui::TextDisabled("Nested advertisement data type mismatch");
			return;
		}
		const FArcMassExecuteAdvertisementTaskInstanceData& TaskData = TaskView.Get<const FArcMassExecuteAdvertisementTaskInstanceData>();
		const FArcAdvertisementExecutionContext& AdvCtx = TaskData.ExecutionContext;

		// Advertisement metadata
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Advertisement Context");
		ImGui::Separator();
		ImGui::Text("Handle: %u", TaskData.AdvertisementHandle.GetValue());
		ImGui::Text("Source Entity: %s", TCHAR_TO_ANSI(*AdvCtx.GetSourceEntity().DebugGetDescription()));
		ImGui::Text("Executing Entity: %s", TCHAR_TO_ANSI(*AdvCtx.GetExecutingEntity().DebugGetDescription()));

		const FInstancedStruct& Payload = AdvCtx.GetAdvertisementPayload();
		if (Payload.IsValid())
		{
			FString PayloadTypeName = Payload.GetScriptStruct()->GetName();
			ImGui::Text("Payload Type: %s", TCHAR_TO_ANSI(*PayloadTypeName));
			ArcImGuiReflection::DrawStructView(TEXT("Payload"), FConstStructView(Payload));
		}
		else
		{
			ImGui::TextDisabled("Payload: (none)");
		}

		ImGui::Spacing();
		ImGui::Separator();

		InstanceData = &AdvCtx.GetStateTreeInstanceData();
	}

	const FStateTreeExecutionState* ExecState = InstanceData->GetExecutionState();
	if (!ExecState || !ExecState->ActiveFrames.IsValidIndex(SelectedStateFrameIdx))
	{
		ImGui::TextDisabled("Selected frame is no longer valid");
		return;
	}

	const FStateTreeExecutionFrame& SelFrame = ExecState->ActiveFrames[SelectedStateFrameIdx];
	if (!SelFrame.StateTree)
	{
		ImGui::TextDisabled("Frame has no state tree");
		return;
	}

	// --- Global frame view (parameters, evaluators, tasks) ---
	if (bSelectedFrameGlobal)
	{
		const UStateTree* StateTree = SelFrame.StateTree;
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Frame: %s", TCHAR_TO_ANSI(*StateTree->GetName()));
		ImGui::Separator();

		// Global Parameters
		if (ImGui::CollapsingHeader("Global Parameters", ImGuiTreeNodeFlags_DefaultOpen))
		{
			FConstStructView GlobalParams = InstanceData->GetStorage().GetGlobalParameters();
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

		// Global Evaluators
		uint16 EvalsBegin = StateTree->GetGlobalEvaluatorsBegin();
		uint16 EvalsNum = StateTree->GetGlobalEvaluatorsNum();
		if (EvalsNum > 0 && ImGui::CollapsingHeader("Global Evaluators", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (uint16 i = 0; i < EvalsNum; i++)
			{
				int32 NodeIdx = EvalsBegin + i;
				FConstStructView NodeView = StateTree->GetNode(NodeIdx);
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
							if (InstanceData->IsValidIndex(StorageIdx) && !InstanceData->IsObject(StorageIdx))
							{
								FConstStructView View = InstanceData->GetStruct(StorageIdx);
								ArcImGuiReflection::DrawStructView(TEXT("Instance"), View);
							}
							else if (InstanceData->IsValidIndex(StorageIdx))
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

		// Global Tasks
		uint16 TasksBegin = StateTree->GetGlobalTasksBegin();
		uint16 TasksNum = StateTree->GetGlobalTasksNum();
		if (TasksNum > 0 && ImGui::CollapsingHeader("Global Tasks", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (uint16 i = 0; i < TasksNum; i++)
			{
				int32 NodeIdx = TasksBegin + i;
				FConstStructView NodeView = StateTree->GetNode(NodeIdx);
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
							if (InstanceData->IsValidIndex(StorageIdx) && !InstanceData->IsObject(StorageIdx))
							{
								FConstStructView View = InstanceData->GetStruct(StorageIdx);
								ArcImGuiReflection::DrawStructView(TEXT("Instance"), View);
							}
							else if (InstanceData->IsValidIndex(StorageIdx))
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

	// Check if the state is currently active
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
		// Walk active states in order to compute the storage offset
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
				if (InstanceData->IsValidIndex(StorageIdx) && !InstanceData->IsObject(StorageIdx))
				{
					FConstStructView View = InstanceData->GetStruct(StorageIdx);
					FString ItemLabel = FString::Printf(TEXT("[%d]"), i);
					ArcImGuiReflection::DrawStructView(*ItemLabel, View);
				}
				else if (InstanceData->IsValidIndex(StorageIdx))
				{
					ImGui::TextDisabled("[%d]: (UObject instance)", i);
				}
			}
		}
	}
#endif
}

void FArcAIDebugger::DrawSendAsyncMessage()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		return;
	}

	ImGui::Text("Send Async Message");
	ImGui::Separator();

	// Tag picker
	if (ImGui::TreeNodeEx("Message Tag##AsyncTagTree", ImGuiTreeNodeFlags_DefaultOpen))
	{
		AsyncMessageTagWidget.DrawInline();
		ImGui::TreePop();
	}

	ImGui::Separator();

	// Payload editor
	if (ImGui::TreeNodeEx("Payload##AsyncPayload", ImGuiTreeNodeFlags_DefaultOpen))
	{
		AsyncMessagePayloadEditor.DrawInline();
		ImGui::TreePop();
	}

	ImGui::Separator();

	const FGameplayTagContainer& SelectedTags = AsyncMessageTagWidget.GetSelectedTags();
	bool bHasTag = SelectedTags.Num() > 0;

	if (!bHasTag)
	{
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Send Async Message"))
	{
		// Use the first selected tag
		FGameplayTag Tag;
		for (const FGameplayTag& T : SelectedTags)
		{
			Tag = T;
			break;
		}

		UWorld* World = GetDebugWorld();
		if (!World)
		{
			return;
		}

		FMassEntityManager* Manager = GetEntityManager();
		if (!Manager)
		{
			return;
		}

		const FMassEntityHandle Entity = CachedEntities[SelectedEntityIndex].Entity;
		if (!Manager->IsEntityActive(Entity))
		{
			return;
		}

		TSharedPtr<FAsyncGameplayMessageSystem> MessageSystem = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(World);
		if (!MessageSystem.IsValid())
		{
			return;
		}

		const FArcMassAsyncMessageEndpointFragment* EndpointFragment = Manager->GetFragmentDataPtr<FArcMassAsyncMessageEndpointFragment>(Entity);
		if (!EndpointFragment || !EndpointFragment->Endpoint.IsValid())
		{
			return;
		}

		FAsyncMessageId MessageId(Tag);
		if (AsyncMessagePayloadEditor.HasValidInstance())
		{
			MessageSystem->QueueMessageForBroadcast(MessageId, AsyncMessagePayloadEditor.GetStructView(), EndpointFragment->Endpoint);
		}
		else
		{
			FArcAIBaseEvent Payload;
			MessageSystem->QueueMessageForBroadcast(MessageId, FConstStructView::Make(Payload), EndpointFragment->Endpoint);
		}
	}

	if (!bHasTag)
	{
		ImGui::EndDisabled();
	}
#endif
}

void FArcAIDebugger::DrawSendStateTreeEvent()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		return;
	}

	ImGui::Text("Send StateTree Event");
	ImGui::Separator();

	// Tag picker
	if (ImGui::TreeNodeEx("Event Tag##STTagTree", ImGuiTreeNodeFlags_DefaultOpen))
	{
		StateTreeEventTagWidget.DrawInline();
		ImGui::TreePop();
	}

	ImGui::Separator();

	// Payload editor
	if (ImGui::TreeNodeEx("Payload##STPayload", ImGuiTreeNodeFlags_DefaultOpen))
	{
		StateTreeEventPayloadEditor.DrawInline();
		ImGui::TreePop();
	}

	ImGui::Separator();

	const FGameplayTagContainer& SelectedTags = StateTreeEventTagWidget.GetSelectedTags();
	bool bHasTag = SelectedTags.Num() > 0;

	if (!bHasTag)
	{
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Send StateTree Event"))
	{
		FGameplayTag Tag;
		for (const FGameplayTag& T : SelectedTags)
		{
			Tag = T;
			break;
		}

		UWorld* World = GetDebugWorld();
		if (!World)
		{
			return;
		}

		FMassEntityManager* Manager = GetEntityManager();
		if (!Manager)
		{
			return;
		}

		const FMassEntityHandle Entity = CachedEntities[SelectedEntityIndex].Entity;
		if (!Manager->IsEntityActive(Entity))
		{
			return;
		}

		const FMassStateTreeSharedFragment* StateTreeShared = Manager->GetConstSharedFragmentDataPtr<FMassStateTreeSharedFragment>(Entity);
		FMassStateTreeInstanceFragment* InstanceST = Manager->GetFragmentDataPtr<FMassStateTreeInstanceFragment>(Entity);
		if (!StateTreeShared || !InstanceST || !InstanceST->InstanceHandle.IsValid())
		{
			return;
		}

		UMassStateTreeSubsystem* STSubsystem = World->GetSubsystem<UMassStateTreeSubsystem>();
		if (!STSubsystem)
		{
			return;
		}

		FStateTreeInstanceData* STData = STSubsystem->GetInstanceData(InstanceST->InstanceHandle);
		if (!STData)
		{
			return;
		}

		FStateTreeMinimalExecutionContext Context(STSubsystem, StateTreeShared->StateTree, *STData);
		if (StateTreeEventPayloadEditor.HasValidInstance())
		{
			Context.SendEvent(Tag, StateTreeEventPayloadEditor.GetStructView());
		}
		else
		{
			FArcAIBaseEvent Payload;
			Context.SendEvent(Tag, FConstStructView::Make(Payload));
		}

		UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
		}
	}

	if (!bHasTag)
	{
		ImGui::EndDisabled();
	}
#endif
}

void FArcAIDebugger::DrawPlanInWorld()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	UWorld* World = GetDebugWorld();
	if (!World)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	FMassEntityManager* Manager = GetEntityManager();
	if (!Manager)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	const FEntityEntry& Entry = CachedEntities[SelectedEntityIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	FVector EntityLoc = Entry.Location;
	if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
	{
		EntityLoc = TransformFrag->GetTransform().GetLocation();
	}

	FVector PlayerLoc = FVector::ZeroVector;
	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		PlayerLoc = PlayerPawn->GetActorLocation();
	}

	static const FColor EntityColor = FColor::Cyan;
	static const FColor StepColor = FColor::Yellow;
	constexpr float StepSphereRadius = 30.f;
	constexpr float ZOffset = 100.f;

	FArcAIDebugPlanDrawData DrawData;
	DrawData.EntityLocation = EntityLoc;
	DrawData.PlayerLocation = PlayerLoc;
	DrawData.EntityColor = EntityColor;
	DrawData.ZOffset = ZOffset;
	DrawData.StepSphereRadius = StepSphereRadius;
	DrawData.bHasEntity = true;

	UArcSmartObjectPlannerSubsystem* PlannerSubsystem = World->GetSubsystem<UArcSmartObjectPlannerSubsystem>();
	const FArcSmartObjectPlanContainer* Plan = PlannerSubsystem ? PlannerSubsystem->GetDebugPlan(Entity) : nullptr;

	if (Plan)
	{
		for (int32 StepIdx = 0; StepIdx < Plan->Items.Num(); ++StepIdx)
		{
			const FArcSmartObjectPlanStep& Step = Plan->Items[StepIdx];
			FArcAIDebugPlanStep& OutStep = DrawData.Steps.AddDefaulted_GetRef();
			OutStep.Location = Step.Location + FVector(0, 0, ZOffset);
			OutStep.Label = FString::Printf(TEXT("Step %d"), StepIdx);
			OutStep.Color = StepColor;
		}
	}

	EnsureDrawActor(World);
	if (DrawComponent.IsValid())
	{
		DrawComponent->UpdatePlanData(DrawData);
	}
#endif
}

void FArcAIDebugger::EnsureDrawActor(UWorld* World)
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
	NewActor->SetActorLabel(TEXT("AIDebuggerDraw"));
#endif

	UArcAIDebuggerDrawComponent* NewComponent = NewObject<UArcAIDebuggerDrawComponent>(NewActor, UArcAIDebuggerDrawComponent::StaticClass());
	NewComponent->RegisterComponent();
	NewActor->AddInstanceComponent(NewComponent);

	DrawActor = NewActor;
	DrawComponent = NewComponent;
}

void FArcAIDebugger::DestroyDrawActor()
{
	if (DrawActor.IsValid())
	{
		DrawActor->Destroy();
	}
	DrawActor.Reset();
	DrawComponent.Reset();
}
