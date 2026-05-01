// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisEntityDebugger.h"

#include "imgui.h"
#include "ArcMass/Visualization/ArcMassEntityVisualization.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"
#include "ArcMass/Visualization/ArcVisLifecycle.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "ArcVisEntityDebuggerDrawComponent.h"
#include "GameFramework/Actor.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "MassSubsystemBase.h"

namespace Arcx::GameplayDebugger::VisEntity
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

	const char* GetVisStateString(bool bHasMeshRendering, bool bHasPhysicsBody)
	{
		if (bHasMeshRendering && bHasPhysicsBody)
		{
			return "Mesh + Physics";
		}
		if (bHasMeshRendering)
		{
			return "Mesh Only";
		}
		return "No Visualization";
	}

	ImVec4 GetVisStateColor(bool bHasMeshRendering)
	{
		if (bHasMeshRendering)
		{
			return ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
		}
		return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
	}

	const char* GetLifecyclePhaseString(uint8 Phase)
	{
		static char Buf[16];
		FCStringAnsi::Snprintf(Buf, sizeof(Buf), "Phase %d", Phase);
		return Buf;
	}

	ImVec4 GetLifecyclePhaseColor(uint8 Phase)
	{
		static const ImVec4 Palette[] = {
			ImVec4(0.71f, 0.71f, 0.71f, 1.0f), // grey
			ImVec4(0.39f, 0.78f, 0.39f, 1.0f), // green
			ImVec4(0.39f, 0.78f, 0.39f, 1.0f), // green
			ImVec4(0.86f, 0.78f, 0.20f, 1.0f), // yellow
			ImVec4(0.86f, 0.31f, 0.31f, 1.0f), // red
			ImVec4(0.59f, 0.59f, 1.00f, 1.0f), // blue
			ImVec4(0.78f, 0.59f, 1.00f, 1.0f), // purple
		};
		static const int32 PaletteSize = UE_ARRAY_COUNT(Palette);
		return Palette[Phase % PaletteSize];
	}
}

void FArcVisEntityDebugger::Initialize()
{
	SelectedEntityIndex = INDEX_NONE;
	EntityFilterBuf[0] = '\0';
	CachedEntities.Reset();
	LastRefreshTime = 0.f;
	RefreshEntityList();
	RefreshHolderList();
}

void FArcVisEntityDebugger::Uninitialize()
{
	DestroyDrawActor();
	CachedEntities.Reset();
	SelectedEntityIndex = INDEX_NONE;
	CachedHolders.Reset();
	SelectedHolderIndex = INDEX_NONE;
}

void FArcVisEntityDebugger::RefreshEntityList()
{
	CachedEntities.Reset();

	FMassEntityManager* Manager = Arcx::GameplayDebugger::VisEntity::GetEntityManager();
	if (!Manager)
	{
		return;
	}

#if WITH_MASSENTITY_DEBUG
	TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);

	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);
		if (!Composition.Contains<FArcVisEntityTag>())
		{
			continue;
		}

		TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle& Entity : Entities)
		{
			FVisEntityEntry& Entry = CachedEntities.AddDefaulted_GetRef();
			Entry.Entity = Entity;

			const FArcVisRepresentationFragment* Rep = Manager->GetFragmentDataPtr<FArcVisRepresentationFragment>(Entity);

			if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
			{
				Entry.Location = TransformFrag->GetTransform().GetLocation();
			}

			const char* State = Rep ? (Rep->bHasMeshRendering ? "MESH" : "---") : "???";
			Entry.Label = FString::Printf(TEXT("E%d (%hs)"), Entity.Index, State);
		}
	}
#endif
}

void FArcVisEntityDebugger::RefreshHolderList()
{
	CachedHolders.Reset();

	UWorld* World = Arcx::GameplayDebugger::VisEntity::GetDebugWorld();
	if (!World)
	{
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::VisEntity::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* VisSub = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!VisSub)
	{
		return;
	}

	const TMap<FIntVector, TMap<FArcVisISMHolderKey, FMassEntityHandle>>& CellHolders = VisSub->GetCellISMHolders();

	for (const TPair<FIntVector, TMap<FArcVisISMHolderKey, FMassEntityHandle>>& CellPair : CellHolders)
	{
		for (const TPair<FArcVisISMHolderKey, FMassEntityHandle>& HolderPair : CellPair.Value)
		{
			if (!Manager->IsEntityValid(HolderPair.Value))
			{
				continue;
			}

			FHolderEntityEntry& Entry = CachedHolders.AddDefaulted_GetRef();
			Entry.Entity = HolderPair.Value;
			Entry.Cell = CellPair.Key;
			Entry.bCastShadow = HolderPair.Key.bCastShadow;

			if (HolderPair.Key.Mesh.IsValid())
			{
				Entry.MeshName = HolderPair.Key.Mesh->GetName();
			}
			else
			{
				Entry.MeshName = TEXT("(null)");
			}

			const FMassRenderISMFragment* ISMFrag = Manager->GetFragmentDataPtr<FMassRenderISMFragment>(HolderPair.Value);
			if (ISMFrag)
			{
				Entry.InstanceCount = ISMFrag->PerInstanceSMData.Num();
			}
		}
	}

	// Build reverse map: scan vis entities for FArcVisISMInstanceFragment referencing holders
	for (int32 VisIdx = 0; VisIdx < CachedEntities.Num(); ++VisIdx)
	{
		const FVisEntityEntry& VisEntry = CachedEntities[VisIdx];
		if (!Manager->IsEntityValid(VisEntry.Entity))
		{
			continue;
		}

		const FArcVisISMInstanceFragment* ISMInstFrag = Manager->GetFragmentDataPtr<FArcVisISMInstanceFragment>(VisEntry.Entity);
		if (!ISMInstFrag || !ISMInstFrag->HolderEntity.IsValid())
		{
			continue;
		}

		for (int32 HolderIdx = 0; HolderIdx < CachedHolders.Num(); ++HolderIdx)
		{
			if (CachedHolders[HolderIdx].Entity == ISMInstFrag->HolderEntity)
			{
				CachedHolders[HolderIdx].ReferencingVisEntityIndices.Add(VisIdx);
				break;
			}
		}
	}
}

void FArcVisEntityDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(950, 650), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Arc Entity Visualization", &bShow))
	{
		ImGui::End();
		if (DrawComponent.IsValid()) { DrawComponent->ClearShapes(); }
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::VisEntity::GetEntityManager();
	if (!Manager)
	{
		ImGui::TextDisabled("No MassEntitySubsystem available");
		ImGui::End();
		return;
	}

	// Toolbar
	if (ImGui::Button("Refresh"))
	{
		RefreshEntityList();
	}
	ImGui::SameLine();
	ImGui::Text("Vis Entities: %d", CachedEntities.Num());
	ImGui::SameLine();
	ImGui::Checkbox("Show Grid", &bShowGrid);

	// Auto-refresh every 2 seconds
	UWorld* World = Arcx::GameplayDebugger::VisEntity::GetDebugWorld();
	if (World)
	{
		float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastRefreshTime > 2.0f)
		{
			LastRefreshTime = CurrentTime;
			RefreshEntityList();
			RefreshHolderList();
		}
	}

	ImGui::Separator();

	// Handle pending navigation requests
	bool bForceVisTab = false;
	bool bForceHolderTab = false;
	if (PendingVisEntityNavIndex != INDEX_NONE)
	{
		bForceVisTab = true;
	}
	if (PendingHolderNavIndex != INDEX_NONE)
	{
		bForceHolderTab = true;
	}

	if (ImGui::BeginTabBar("DebuggerTabs"))
	{
		ImGuiTabItemFlags VisFlags = bForceVisTab ? ImGuiTabItemFlags_SetSelected : 0;
		ImGuiTabItemFlags HolderFlags = bForceHolderTab ? ImGuiTabItemFlags_SetSelected : 0;

		if (ImGui::BeginTabItem("Vis Entities", nullptr, VisFlags))
		{
			if (PendingVisEntityNavIndex != INDEX_NONE)
			{
				SelectedEntityIndex = PendingVisEntityNavIndex;
				PendingVisEntityNavIndex = INDEX_NONE;
			}

			float PanelWidth = ImGui::GetContentRegionAvail().x;
			float LeftPanelWidth = PanelWidth * 0.35f;

			if (ImGui::BeginChild("VisEntityListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
			{
				DrawEntityListPanel();
			}
			ImGui::EndChild();

			ImGui::SameLine();

			if (ImGui::BeginChild("VisEntityDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
			{
				DrawEntityDetailPanel();
			}
			ImGui::EndChild();

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Holder Entities", nullptr, HolderFlags))
		{
			if (PendingHolderNavIndex != INDEX_NONE)
			{
				SelectedHolderIndex = PendingHolderNavIndex;
				PendingHolderNavIndex = INDEX_NONE;
			}

			float PanelWidth = ImGui::GetContentRegionAvail().x;
			float LeftPanelWidth = PanelWidth * 0.35f;

			if (ImGui::BeginChild("HolderListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
			{
				DrawHolderListPanel();
			}
			ImGui::EndChild();

			ImGui::SameLine();

			if (ImGui::BeginChild("HolderDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
			{
				DrawHolderDetailPanel();
			}
			ImGui::EndChild();

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();

	// Build unified draw data from all active visualizations
	{
		UWorld* DrawWorld = Arcx::GameplayDebugger::VisEntity::GetDebugWorld();
		FArcVisEntityDebugDrawData DrawData;

		if (DrawWorld)
		{
			FMassEntityManager* DrawManager = Arcx::GameplayDebugger::VisEntity::GetEntityManager();
			if (DrawManager)
			{
				DrawSelectedEntityInWorld(*DrawManager, DrawData);
				DrawSelectedHolderInWorld(*DrawManager, *DrawWorld, DrawData);
				if (bShowGrid)
				{
					DrawGridVisualization(*DrawManager, *DrawWorld, DrawData);
				}
			}

			EnsureDrawActor(DrawWorld);
		}

		if (DrawComponent.IsValid())
		{
			DrawComponent->UpdateVisData(DrawData);
		}
	}
#endif
}

void FArcVisEntityDebugger::DrawEntityListPanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("Visualization Entities");
	ImGui::Separator();

	ImGui::InputText("Filter", EntityFilterBuf, IM_ARRAYSIZE(EntityFilterBuf));

	FString Filter = FString(ANSI_TO_TCHAR(EntityFilterBuf)).ToLower();

	if (ImGui::BeginChild("VisEntityScroll", ImVec2(0, 0)))
	{
		for (int32 i = 0; i < CachedEntities.Num(); ++i)
		{
			const FVisEntityEntry& Entry = CachedEntities[i];

			if (!Filter.IsEmpty() && !Entry.Label.ToLower().Contains(Filter))
			{
				continue;
			}

			ImGui::PushID(i);

			// Color-code by visualization state
			ImVec4 Color = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, Color);

			const bool bSelected = (SelectedEntityIndex == i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Entry.Label), bSelected))
			{
				SelectedEntityIndex = i;
			}

			ImGui::PopStyleColor();
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
#endif
}

void FArcVisEntityDebugger::DrawHolderListPanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("ISM Holder Entities");
	ImGui::Separator();

	ImGui::InputText("Filter##Holder", HolderFilterBuf, IM_ARRAYSIZE(HolderFilterBuf));

	FString Filter = FString(ANSI_TO_TCHAR(HolderFilterBuf)).ToLower();

	int32 TotalInstances = 0;
	for (const FHolderEntityEntry& Entry : CachedHolders)
	{
		TotalInstances += Entry.InstanceCount;
	}
	ImGui::Text("Holders: %d  |  Total Instances: %d", CachedHolders.Num(), TotalInstances);
	ImGui::Separator();

	if (ImGui::BeginChild("HolderScroll", ImVec2(0, 0)))
	{
		for (int32 i = 0; i < CachedHolders.Num(); ++i)
		{
			const FHolderEntityEntry& Entry = CachedHolders[i];

			if (!Filter.IsEmpty() && !Entry.MeshName.ToLower().Contains(Filter))
			{
				continue;
			}

			ImGui::PushID(i);

			ImVec4 Color = Entry.InstanceCount > 0
				? ImVec4(1.0f, 0.8f, 0.2f, 1.0f)
				: ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, Color);

			FString Label = FString::Printf(TEXT("%s [%d] (%d,%d,%d)"),
				*Entry.MeshName, Entry.InstanceCount,
				Entry.Cell.X, Entry.Cell.Y, Entry.Cell.Z);

			const bool bSelected = (SelectedHolderIndex == i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Label), bSelected))
			{
				SelectedHolderIndex = i;
			}

			ImGui::PopStyleColor();
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
#endif
}

void FArcVisEntityDebugger::DrawHolderDetailPanel()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedHolderIndex == INDEX_NONE || !CachedHolders.IsValidIndex(SelectedHolderIndex))
	{
		ImGui::TextDisabled("Select a holder entity from the list");
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::VisEntity::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FHolderEntityEntry& Entry = CachedHolders[SelectedHolderIndex];

	if (!Manager->IsEntityActive(Entry.Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Holder entity is no longer active");
		return;
	}

	ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Holder Entity %d (SN:%d)",
		Entry.Entity.Index, Entry.Entity.SerialNumber);

	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Holder Info", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Mesh: %s", TCHAR_TO_ANSI(*Entry.MeshName));
		ImGui::Text("Instance Count: %d", Entry.InstanceCount);
		ImGui::Text("Cell: (%d, %d, %d)", Entry.Cell.X, Entry.Cell.Y, Entry.Cell.Z);
		ImGui::Text("Cast Shadow: %s", Entry.bCastShadow ? "true" : "false");
	}

	if (ImGui::CollapsingHeader("Referencing Vis Entities", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (Entry.ReferencingVisEntityIndices.IsEmpty())
		{
			ImGui::TextDisabled("No vis entities reference this holder");
		}
		else
		{
			ImGui::Text("%d entities:", Entry.ReferencingVisEntityIndices.Num());
			for (int32 VisIdx : Entry.ReferencingVisEntityIndices)
			{
				if (!CachedEntities.IsValidIndex(VisIdx))
				{
					continue;
				}
				const FVisEntityEntry& VisEntry = CachedEntities[VisIdx];
				ImGui::PushID(VisIdx);
				if (ImGui::SmallButton(TCHAR_TO_ANSI(*VisEntry.Label)))
				{
					PendingVisEntityNavIndex = VisIdx;
				}
				ImGui::PopID();
			}
		}
	}
#endif
}

void FArcVisEntityDebugger::DrawEntityDetailPanel()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		ImGui::TextDisabled("Select a visualization entity from the list");
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::VisEntity::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FVisEntityEntry& Entry = CachedEntities[SelectedEntityIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Entity is no longer active");
		return;
	}

	// --- Entity Header ---
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Entity %d (SN:%d)", Entity.Index, Entity.SerialNumber);

	// Actor name
	const FMassActorFragment* ActorFrag = Manager->GetFragmentDataPtr<FMassActorFragment>(Entity);
	if (ActorFrag)
	{
		if (const AActor* Actor = ActorFrag->Get())
		{
			ImGui::Text("Actor: %s", TCHAR_TO_ANSI(*Actor->GetName()));
			ImGui::Text("Class: %s", TCHAR_TO_ANSI(*Actor->GetClass()->GetName()));
		}
		else
		{
			ImGui::TextDisabled("Actor: None");
		}
	}

	// Location
	if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
	{
		FVector Loc = TransformFrag->GetTransform().GetLocation();
		ImGui::Text("Location: (%.0f, %.0f, %.0f)", Loc.X, Loc.Y, Loc.Z);
	}

	ImGui::Spacing();

	// === VISUALIZATION STATE ===
	if (ImGui::CollapsingHeader("Visualization State", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const FArcVisRepresentationFragment* Rep = Manager->GetFragmentDataPtr<FArcVisRepresentationFragment>(Entity);
		if (Rep)
		{
			const FArcMassPhysicsBodyFragment* PhysBodyFrag = Manager->GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(Entity);
			const bool bHasPhysicsBody = PhysBodyFrag && PhysBodyFrag->Body != nullptr;
			const char* StateStr = Arcx::GameplayDebugger::VisEntity::GetVisStateString(Rep->bHasMeshRendering, bHasPhysicsBody);
			ImVec4 StateColor = Arcx::GameplayDebugger::VisEntity::GetVisStateColor(Rep->bHasMeshRendering);
			ImGui::TextColored(StateColor, "State: %s", StateStr);

			ImGui::Text("Mesh Grid: (%d, %d, %d)", Rep->MeshGridCoords.X, Rep->MeshGridCoords.Y, Rep->MeshGridCoords.Z);
			ImGui::Text("Physics Grid: (%d, %d, %d)", Rep->PhysicsGridCoords.X, Rep->PhysicsGridCoords.Y, Rep->PhysicsGridCoords.Z);
			ImGui::Text("Mesh Rendering: %s", Rep->bHasMeshRendering ? "Active" : "Inactive");
			ImGui::Text("Physics Body: %s", bHasPhysicsBody ? "Attached" : "None");
		}
		else
		{
			ImGui::TextDisabled("No FArcVisRepresentationFragment");
		}

		// Config info
		const FArcVisConfigFragment* Config = Manager->GetConstSharedFragmentDataPtr<FArcVisConfigFragment>(Entity);
		if (Config)
		{
			ImGui::Spacing();
			ImGui::TextDisabled("Config:");
			const FArcMassStaticMeshConfigFragment* MeshFrag = Manager->GetConstSharedFragmentDataPtr<FArcMassStaticMeshConfigFragment>(Entity);
			if (MeshFrag && MeshFrag->Mesh.IsValid())
			{
				ImGui::Text("  Base Mesh: %s", TCHAR_TO_ANSI(*MeshFrag->Mesh->GetName()));
			}
			ImGui::Text("  Cast Shadows: %s", Config->bCastShadows ? "true" : "false");
			const FArcMassPhysicsBodyConfigFragment* PhysConfig = Manager->GetConstSharedFragmentDataPtr<FArcMassPhysicsBodyConfigFragment>(Entity);
			ImGui::Text("  Physics Config: %s", PhysConfig ? "true" : "false");
			if (PhysConfig)
			{
				ImGui::Text("  Body Type: %s", PhysConfig->BodyType == EArcMassPhysicsBodyType::Dynamic ? "Dynamic" : "Static");
			}
		}
	}

	// === LIFECYCLE ===
	const FArcVisLifecycleFragment* LCFrag = Manager->GetFragmentDataPtr<FArcVisLifecycleFragment>(Entity);
	if (LCFrag)
	{
		if (ImGui::CollapsingHeader("Lifecycle", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImVec4 PhaseColor = Arcx::GameplayDebugger::VisEntity::GetLifecyclePhaseColor(LCFrag->CurrentPhase);
			ImGui::TextColored(PhaseColor, "Phase: %s", Arcx::GameplayDebugger::VisEntity::GetLifecyclePhaseString(LCFrag->CurrentPhase));
			ImGui::Text("Previous Phase: %s", Arcx::GameplayDebugger::VisEntity::GetLifecyclePhaseString(LCFrag->PreviousPhase));
			ImGui::Text("Time in Phase: %.1fs", LCFrag->PhaseTimeElapsed);

			const FArcVisLifecycleConfigFragment* LCConfig = Manager->GetConstSharedFragmentDataPtr<FArcVisLifecycleConfigFragment>(Entity);
			if (LCConfig)
			{
				float PhaseDuration = LCConfig->GetPhaseDuration(LCFrag->CurrentPhase);
				if (PhaseDuration > 0.f)
				{
					float Progress = FMath::Clamp(LCFrag->PhaseTimeElapsed / PhaseDuration, 0.f, 1.f);
					ImGui::ProgressBar(Progress, ImVec2(-1, 0),
						TCHAR_TO_ANSI(*FString::Printf(TEXT("%.1f / %.1f"), LCFrag->PhaseTimeElapsed, PhaseDuration)));
				}
				else
				{
					ImGui::TextDisabled("  Duration: indefinite");
				}

			}
		}
	}

	// Also check non-vis lifecycle
	const FArcLifecycleFragment* BaseLCFrag = Manager->GetFragmentDataPtr<FArcLifecycleFragment>(Entity);
	if (BaseLCFrag && !LCFrag)
	{
		if (ImGui::CollapsingHeader("Lifecycle (Base)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImVec4 PhaseColor = Arcx::GameplayDebugger::VisEntity::GetLifecyclePhaseColor(BaseLCFrag->CurrentPhase);
			ImGui::TextColored(PhaseColor, "Phase: %s", Arcx::GameplayDebugger::VisEntity::GetLifecyclePhaseString(BaseLCFrag->CurrentPhase));
			ImGui::Text("Previous Phase: %s", Arcx::GameplayDebugger::VisEntity::GetLifecyclePhaseString(BaseLCFrag->PreviousPhase));
			ImGui::Text("Time in Phase: %.1fs", BaseLCFrag->PhaseTimeElapsed);
		}
	}

	// === ISM HOLDER LINK ===
	const FArcVisISMInstanceFragment* ISMInstFrag = Manager->GetFragmentDataPtr<FArcVisISMInstanceFragment>(Entity);
	if (ISMInstFrag && ISMInstFrag->HolderEntity.IsValid())
	{
		if (ImGui::CollapsingHeader("ISM Holder", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Holder Entity: E%d", ISMInstFrag->HolderEntity.Index);
			ImGui::Text("Instance Index: %d", ISMInstFrag->InstanceIndex);

			for (int32 HolderIdx = 0; HolderIdx < CachedHolders.Num(); ++HolderIdx)
			{
				if (CachedHolders[HolderIdx].Entity == ISMInstFrag->HolderEntity)
				{
					const FHolderEntityEntry& HolderEntry = CachedHolders[HolderIdx];
					ImGui::Text("Mesh: %s", TCHAR_TO_ANSI(*HolderEntry.MeshName));
					ImGui::Text("Total Instances on Holder: %d", HolderEntry.InstanceCount);
					ImGui::Text("Siblings: %d", FMath::Max(0, HolderEntry.ReferencingVisEntityIndices.Num() - 1));

					if (ImGui::SmallButton("Go to Holder"))
					{
						PendingHolderNavIndex = HolderIdx;
					}
					break;
				}
			}
		}
	}
#endif
}

void FArcVisEntityDebugger::DrawSelectedEntityInWorld(FMassEntityManager& Manager, FArcVisEntityDebugDrawData& OutDrawData)
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		return;
	}

	const FVisEntityEntry& Entry = CachedEntities[SelectedEntityIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager.IsEntityActive(Entity))
	{
		return;
	}

	FVector Location = Entry.Location;
	if (const FTransformFragment* TransformFrag = Manager.GetFragmentDataPtr<FTransformFragment>(Entity))
	{
		Location = TransformFrag->GetTransform().GetLocation();
	}

	const FArcVisRepresentationFragment* Rep = Manager.GetFragmentDataPtr<FArcVisRepresentationFragment>(Entity);
	FColor ShapeColor = FColor::Yellow;
	if (Rep)
	{
		ShapeColor = FColor::Cyan;
	}

	FString Label = FString::Printf(TEXT("E%d"), Entity.Index);
	if (Rep)
	{
		Label += Rep->bHasMeshRendering ? TEXT(" [MESH]") : TEXT(" [---]");
	}

	OutDrawData.bHasSelectedEntity = true;
	OutDrawData.SelectedEntity.Location = Location;
	OutDrawData.SelectedEntity.Color = ShapeColor;
	OutDrawData.SelectedEntity.Label = Label;
#endif
}

void FArcVisEntityDebugger::DrawSelectedHolderInWorld(FMassEntityManager& Manager, UWorld& World, FArcVisEntityDebugDrawData& OutDrawData)
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedHolderIndex == INDEX_NONE || !CachedHolders.IsValidIndex(SelectedHolderIndex))
	{
		return;
	}

	const FHolderEntityEntry& Entry = CachedHolders[SelectedHolderIndex];
	if (!Manager.IsEntityActive(Entry.Entity))
	{
		return;
	}

	UArcEntityVisualizationSubsystem* VisSub = World.GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!VisSub)
	{
		return;
	}

	const float CellSize = VisSub->GetMeshGrid().CellSize;
	const float HalfCell = CellSize * 0.5f;

	OutDrawData.bHasSelectedHolder = true;

	OutDrawData.SelectedHolder.CellCenter = FVector(
		Entry.Cell.X * CellSize + HalfCell,
		Entry.Cell.Y * CellSize + HalfCell,
		Entry.Cell.Z * CellSize + HalfCell);
	OutDrawData.SelectedHolder.CellHalfExtent = FVector(HalfCell * 0.85f);

	const FMassRenderISMFragment* ISMFrag = Manager.GetFragmentDataPtr<FMassRenderISMFragment>(Entry.Entity);
	if (ISMFrag)
	{
		for (TSparseArray<FInstancedStaticMeshInstanceData>::TConstIterator It(ISMFrag->PerInstanceSMData); It; ++It)
		{
			FArcVisEntityDebugHolderInstance& Inst = OutDrawData.SelectedHolder.Instances.AddDefaulted_GetRef();
			Inst.Location = FVector(It->Transform.GetOrigin());
		}
	}

	for (int32 VisIdx : Entry.ReferencingVisEntityIndices)
	{
		if (CachedEntities.IsValidIndex(VisIdx))
		{
			const FVisEntityEntry& VisEntry = CachedEntities[VisIdx];
			if (Manager.IsEntityValid(VisEntry.Entity))
			{
				const FTransformFragment* TransformFrag = Manager.GetFragmentDataPtr<FTransformFragment>(VisEntry.Entity);
				FVector Loc = TransformFrag ? TransformFrag->GetTransform().GetLocation() : VisEntry.Location;
				OutDrawData.SelectedHolder.ReferencingEntityLocations.Add(Loc);
			}
		}
	}
#endif
}

void FArcVisEntityDebugger::DrawGridVisualization(FMassEntityManager& Manager, UWorld& World, FArcVisEntityDebugDrawData& OutDrawData)
{
#if WITH_MASSENTITY_DEBUG
	UArcEntityVisualizationSubsystem* Subsystem = World.GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	OutDrawData.bShowGrid = true;

	DrawSingleGrid(&World, Subsystem->GetMeshGrid(), Subsystem->GetLastMeshPlayerCell(),
		Subsystem->GetMeshActivationRadiusCells(), Subsystem->GetMeshDeactivationRadiusCells(),
		FColor::Green, FColor::Orange, &Manager, true, OutDrawData);

	DrawSingleGrid(&World, Subsystem->GetPhysicsGrid(), Subsystem->GetLastPhysicsPlayerCell(),
		Subsystem->GetPhysicsActivationRadiusCells(), Subsystem->GetPhysicsDeactivationRadiusCells(),
		FColor::Yellow, FColor::Red, &Manager, false, OutDrawData);
#endif
}

void FArcVisEntityDebugger::DrawSingleGrid(UWorld* World, const FArcVisualizationGrid& Grid, const FIntVector& PlayerCell,
	int32 ActivationCells, int32 DeactivationCells, FColor InActiveColor, FColor BoundaryColor,
	FMassEntityManager* Manager, bool bIsMeshGrid, FArcVisEntityDebugDrawData& OutDrawData)
{
#if WITH_MASSENTITY_DEBUG
	const float CellSize = Grid.CellSize;
	const float HalfCell = CellSize * 0.5f;
	// Offset physics grid drawing slightly to avoid overlap with mesh grid
	const float ZOffset = bIsMeshGrid ? 0.f : 50.f;

	// Build occupied cell data
	for (const TPair<FIntVector, TArray<FMassEntityHandle>>& CellPair : Grid.CellEntities)
	{
		const FIntVector& CellCoord = CellPair.Key;
		const TArray<FMassEntityHandle>& Entities = CellPair.Value;

		const FVector CellCenter(
			CellCoord.X * CellSize + HalfCell,
			CellCoord.Y * CellSize + HalfCell,
			CellCoord.Z * CellSize + HalfCell + ZOffset
		);

		// Determine cell state from first valid entity
		bool bHasMesh = false;
		for (const FMassEntityHandle& Entity : Entities)
		{
			if (Manager->IsEntityValid(Entity))
			{
				const FArcVisRepresentationFragment* Rep = Manager->GetFragmentDataPtr<FArcVisRepresentationFragment>(Entity);
				if (Rep)
				{
					bHasMesh = Rep->bHasMeshRendering;
					break;
				}
			}
		}

		// Determine if cell is in activation or hysteresis zone
		const int32 DXPlayer = FMath::Abs(CellCoord.X - PlayerCell.X);
		const int32 DYPlayer = FMath::Abs(CellCoord.Y - PlayerCell.Y);
		const int32 DZPlayer = FMath::Abs(CellCoord.Z - PlayerCell.Z);
		const int32 MaxDist = FMath::Max3(DXPlayer, DYPlayer, DZPlayer);
		const bool bIsActive = MaxDist <= ActivationCells;
		const bool bIsInHysteresis = !bIsActive && MaxDist <= DeactivationCells;
		const FColor DrawColor = bIsActive ? FColor::Green : (bIsInHysteresis ? FColor::Cyan : FColor::Blue);

		const TCHAR* GridPrefix = bIsMeshGrid ? TEXT("M") : TEXT("P");
		const TCHAR* StateStr = bHasMesh ? TEXT("MESH") : TEXT("---");
		const FString CountLabel = FString::Printf(TEXT("[%s] %d %s"), GridPrefix, Entities.Num(), StateStr);

		FArcVisEntityDebugGridCell& OccupiedCell = OutDrawData.OccupiedCells.AddDefaulted_GetRef();
		OccupiedCell.Center = CellCenter;
		OccupiedCell.HalfExtent = FVector(HalfCell * 0.9f);
		OccupiedCell.Color = DrawColor;
		OccupiedCell.Thickness = 2.f;
		OccupiedCell.CountLabel = CountLabel;
	}

	// Player cell highlight
	if (PlayerCell != FIntVector(TNumericLimits<int32>::Max()))
	{
		const FVector PlayerCellCenter(
			PlayerCell.X * CellSize + HalfCell,
			PlayerCell.Y * CellSize + HalfCell,
			PlayerCell.Z * CellSize + HalfCell + ZOffset
		);

		OutDrawData.bHasPlayerCell = true;
		OutDrawData.PlayerCell.Center = PlayerCellCenter;
		OutDrawData.PlayerCell.HalfExtent = FVector(HalfCell * 0.95f);
		OutDrawData.PlayerCell.Color = FColor::White;
		OutDrawData.PlayerCell.Thickness = 3.f;

		// Activation radius boundary
		TArray<FIntVector> ActiveCells;
		Grid.GetCellsInRadius(PlayerCell, ActivationCells, ActiveCells);

		for (const FIntVector& Cell : ActiveCells)
		{
			const int32 DX = FMath::Abs(Cell.X - PlayerCell.X);
			const int32 DY = FMath::Abs(Cell.Y - PlayerCell.Y);
			const int32 DZ = FMath::Abs(Cell.Z - PlayerCell.Z);
			if (DX == ActivationCells || DY == ActivationCells || DZ == ActivationCells)
			{
				const FVector EdgeCellCenter(
					Cell.X * CellSize + HalfCell,
					Cell.Y * CellSize + HalfCell,
					Cell.Z * CellSize + HalfCell + ZOffset
				);
				FArcVisEntityDebugGridCell& BoundaryCell = OutDrawData.BoundaryCells.AddDefaulted_GetRef();
				BoundaryCell.Center = EdgeCellCenter;
				BoundaryCell.HalfExtent = FVector(HalfCell);
				BoundaryCell.Color = InActiveColor;
				BoundaryCell.Thickness = 1.f;
			}
		}

		// Deactivation radius boundary (hysteresis edge)
		TArray<FIntVector> DeactivationBoundaryCells;
		Grid.GetCellsInRadius(PlayerCell, DeactivationCells, DeactivationBoundaryCells);

		for (const FIntVector& Cell : DeactivationBoundaryCells)
		{
			const int32 DX = FMath::Abs(Cell.X - PlayerCell.X);
			const int32 DY = FMath::Abs(Cell.Y - PlayerCell.Y);
			const int32 DZ = FMath::Abs(Cell.Z - PlayerCell.Z);
			if (DX == DeactivationCells || DY == DeactivationCells || DZ == DeactivationCells)
			{
				const FVector EdgeCellCenter(
					Cell.X * CellSize + HalfCell,
					Cell.Y * CellSize + HalfCell,
					Cell.Z * CellSize + HalfCell + ZOffset
				);
				FArcVisEntityDebugGridCell& BoundaryCell = OutDrawData.BoundaryCells.AddDefaulted_GetRef();
				BoundaryCell.Center = EdgeCellCenter;
				BoundaryCell.HalfExtent = FVector(HalfCell);
				BoundaryCell.Color = BoundaryColor;
				BoundaryCell.Thickness = 1.f;
			}
		}
	}
#endif
}

void FArcVisEntityDebugger::EnsureDrawActor(UWorld* World)
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
	NewActor->SetActorLabel(TEXT("VisEntityDebuggerDraw"));
#endif

	UArcVisEntityDebuggerDrawComponent* NewComponent = NewObject<UArcVisEntityDebuggerDrawComponent>(NewActor, UArcVisEntityDebuggerDrawComponent::StaticClass());
	NewComponent->RegisterComponent();
	NewActor->AddInstanceComponent(NewComponent);

	DrawActor = NewActor;
	DrawComponent = NewComponent;
}

void FArcVisEntityDebugger::DestroyDrawActor()
{
	if (DrawActor.IsValid())
	{
		DrawActor->Destroy();
	}
	DrawActor.Reset();
	DrawComponent.Reset();
}
