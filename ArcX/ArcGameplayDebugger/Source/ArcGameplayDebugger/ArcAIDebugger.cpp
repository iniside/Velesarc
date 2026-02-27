// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAIDebugger.h"

#include "imgui.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassStateTreeFragments.h"
#include "MassStateTreeSubsystem.h"
#include "NeedsSystem/ArcNeedsFragment.h"
#include "SmartObjectPlanner/ArcMassGoalPlanInfoSharedFragment.h"
#include "SmartObjectPlanner/ArcSmartObjectPlannerSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "StateTree.h"

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
}

void FArcAIDebugger::Uninitialize()
{
	CachedEntities.Reset();
	SelectedEntityIndex = INDEX_NONE;
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
	ImGui::SetNextWindowSize(ImVec2(950, 650), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("AI Debugger", &bShow))
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

	// Split into two panes
	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.30f;

	// Left panel: entity list
	if (ImGui::BeginChild("AIEntityListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawEntityListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel: entity detail
	if (ImGui::BeginChild("AIEntityDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawEntityDetailPanel();
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

								FString FrameLabel = FString::Printf(TEXT("Frame %d"), FrameIdx);
								if (Frame.StateTree)
								{
									FrameLabel = FString::Printf(TEXT("Frame %d [%s]"), FrameIdx, *Frame.StateTree->GetName());
								}

								if (ImGui::TreeNodeEx(TCHAR_TO_ANSI(*FrameLabel), ImGuiTreeNodeFlags_DefaultOpen))
								{
									// Show active states in this frame
									for (int32 StateIdx = 0; StateIdx < Frame.ActiveStates.Num(); StateIdx++)
									{
										FStateTreeStateHandle StateHandle = Frame.ActiveStates[StateIdx];

										FString StateName = TEXT("?");
										if (Frame.StateTree)
										{
											const FCompactStateTreeState* CompactState = Frame.StateTree->GetStateFromHandle(StateHandle);
											if (CompactState)
											{
												StateName = CompactState->Name.ToString();
											}
										}

										ImGui::BulletText("%s", TCHAR_TO_ANSI(*StateName));
									}

									ImGui::TreePop();
								}
							}
						}
					}
				}

				ImGui::TextDisabled("Last Update: %.2fs", InstanceST->LastUpdateTimeInSeconds);
				ImGui::TextDisabled("Should Tick: %s", InstanceST->bShouldTick ? "true" : "false");
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

		if (const FArcNeedsFragment* NeedsFragment = Manager->GetFragmentDataPtr<FArcNeedsFragment>(Entity))
		{
			bHasAnyNeeds = true;
			for (const FArcNeedItem& Need : NeedsFragment->Needs)
			{
				float Ratio = Need.CurrentValue / 100.f;
				ImGui::Text("%s", TCHAR_TO_ANSI(*Need.NeedName.ToString()));
				ImGui::SameLine(150);
				ImGui::ProgressBar(Ratio, ImVec2(200, 0));
				ImGui::SameLine();
				ImGui::Text("%.1f (Rate: %.2f)", Need.CurrentValue, Need.ChangeRate);
			}
		}

		if (const FArcHungerNeedFragment* Hunger = Manager->GetFragmentDataPtr<FArcHungerNeedFragment>(Entity))
		{
			bHasAnyNeeds = true;
			float Ratio = Hunger->CurrentValue / 100.f;
			ImGui::Text("Hunger");
			ImGui::SameLine(150);
			ImGui::ProgressBar(Ratio, ImVec2(200, 0));
			ImGui::SameLine();
			ImGui::Text("%.1f (Rate: %.2f)", Hunger->CurrentValue, Hunger->ChangeRate);
		}

		if (const FArcThirstNeedFragment* Thirst = Manager->GetFragmentDataPtr<FArcThirstNeedFragment>(Entity))
		{
			bHasAnyNeeds = true;
			float Ratio = Thirst->CurrentValue / 100.f;
			ImGui::Text("Thirst");
			ImGui::SameLine(150);
			ImGui::ProgressBar(Ratio, ImVec2(200, 0));
			ImGui::SameLine();
			ImGui::Text("%.1f (Rate: %.2f)", Thirst->CurrentValue, Thirst->ChangeRate);
		}

		if (const FArcFatigueNeedFragment* Fatigue = Manager->GetFragmentDataPtr<FArcFatigueNeedFragment>(Entity))
		{
			bHasAnyNeeds = true;
			float Ratio = Fatigue->CurrentValue / 100.f;
			ImGui::Text("Fatigue");
			ImGui::SameLine(150);
			ImGui::ProgressBar(Ratio, ImVec2(200, 0));
			ImGui::SameLine();
			ImGui::Text("%.1f (Rate: %.2f)", Fatigue->CurrentValue, Fatigue->ChangeRate);
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
#endif
}

void FArcAIDebugger::DrawPlanInWorld()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		return;
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

	const FEntityEntry& Entry = CachedEntities[SelectedEntityIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		return;
	}

	UArcSmartObjectPlannerSubsystem* PlannerSubsystem = World->GetSubsystem<UArcSmartObjectPlannerSubsystem>();
	if (!PlannerSubsystem)
	{
		return;
	}

	const FArcSmartObjectPlanContainer* Plan = PlannerSubsystem->GetDebugPlan(Entity);
	if (!Plan || Plan->Items.Num() == 0)
	{
		return;
	}

	// Draw entity location marker
	FVector EntityLoc = Entry.Location;
	if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
	{
		EntityLoc = TransformFrag->GetTransform().GetLocation();
	}

	static const FColor EntityColor = FColor::Cyan;
	static const FColor StepColor = FColor::Yellow;
	static const FColor LineColor = FColor(255, 140, 0); // Orange
	static const FColor ArrowColor = FColor::Red;
	constexpr float StepSphereRadius = 30.f;
	constexpr float ZOffset = 100.f;

	// Draw sphere at entity location
	DrawDebugSphere(World, EntityLoc + FVector(0, 0, ZOffset), StepSphereRadius * 1.5f, 12, EntityColor, false, -1.f, 0, 2.f);

	// Draw line from entity to first step
	if (Plan->Items.Num() > 0)
	{
		FVector FirstStepLoc = Plan->Items[0].Location + FVector(0, 0, ZOffset);
		DrawDebugLine(World, EntityLoc + FVector(0, 0, ZOffset), FirstStepLoc, LineColor, false, -1.f, 0, 3.f);
	}

	// Draw each step
	for (int32 StepIdx = 0; StepIdx < Plan->Items.Num(); StepIdx++)
	{
		const FArcSmartObjectPlanStep& Step = Plan->Items[StepIdx];
		FVector StepLoc = Step.Location + FVector(0, 0, ZOffset);

		// Step sphere
		DrawDebugSphere(World, StepLoc, StepSphereRadius, 12, StepColor, false, -1.f, 0, 2.f);

		// Step number text
		FString StepText = FString::Printf(TEXT("Step %d"), StepIdx);
		DrawDebugString(World, StepLoc + FVector(0, 0, 40.f), StepText, nullptr, FColor::White, -1.f, true, 1.2f);

		// Arrow to next step
		int32 NextIdx = StepIdx + 1;
		if (Plan->Items.IsValidIndex(NextIdx))
		{
			FVector NextStepLoc = Plan->Items[NextIdx].Location + FVector(0, 0, ZOffset);
			DrawDebugDirectionalArrow(World, StepLoc, NextStepLoc, 40.f, ArrowColor, false, -1.f, 0, 3.f);
		}
	}
#endif
}
