// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisEntityDebugger.h"

#include "imgui.h"
#include "ArcMass/Visualization/ArcMassEntityVisualization.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/Visualization/ArcVisLifecycle.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "DrawDebugHelpers.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"

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

	const char* GetVisStateString(bool bIsActor, bool bHasMeshRendering, bool bHasPhysicsBody)
	{
		if (bIsActor)
		{
			return "Full Actor";
		}
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

	ImVec4 GetVisStateColor(bool bIsActor, bool bHasMeshRendering)
	{
		if (bIsActor)
		{
			return ImVec4(0.3f, 1.0f, 0.3f, 1.0f);    // Green
		}
		if (bHasMeshRendering)
		{
			return ImVec4(0.4f, 0.7f, 1.0f, 1.0f);     // Blue
		}
		return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);         // Gray
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
}

void FArcVisEntityDebugger::Uninitialize()
{
	CachedEntities.Reset();
	SelectedEntityIndex = INDEX_NONE;
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
			if (Rep)
			{
				Entry.bIsActorRepresentation = Rep->bIsActorRepresentation;
			}

			if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
			{
				Entry.Location = TransformFrag->GetTransform().GetLocation();
			}

			// Build label: show actor name if available, otherwise entity handle
			FString ActorName;
			if (Rep && Rep->bIsActorRepresentation)
			{
				if (const FMassActorFragment* ActorFrag = Manager->GetFragmentDataPtr<FMassActorFragment>(Entity))
				{
					if (const AActor* Actor = ActorFrag->Get())
					{
						ActorName = Actor->GetName();
					}
				}
			}

			if (!ActorName.IsEmpty())
			{
				Entry.Label = FString::Printf(TEXT("%s [E%d]"), *ActorName, Entity.Index);
			}
			else
			{
				const char* State = Rep ? (Rep->bIsActorRepresentation ? "ACT" : (Rep->bHasMeshRendering ? "MESH" : "---")) : "???";
				Entry.Label = FString::Printf(TEXT("E%d (%hs)"), Entity.Index, State);
			}
		}
	}
#endif
}

void FArcVisEntityDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(950, 650), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Arc Entity Visualization", &bShow))
	{
		ImGui::End();
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
		}
	}

	ImGui::Separator();

	// Split into two panes
	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.35f;

	// Left panel: entity list
	if (ImGui::BeginChild("VisEntityListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawEntityListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel: entity detail
	if (ImGui::BeginChild("VisEntityDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawEntityDetailPanel();
	}
	ImGui::EndChild();

	ImGui::End();

	// World drawing
	DrawSelectedEntityInWorld();
	if (bShowGrid)
	{
		DrawGridVisualization();
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
			ImVec4 Color = Entry.bIsActorRepresentation
				? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
				: ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
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
			const char* StateStr = Arcx::GameplayDebugger::VisEntity::GetVisStateString(Rep->bIsActorRepresentation, Rep->bHasMeshRendering, Rep->bHasPhysicsBody);
			ImVec4 StateColor = Arcx::GameplayDebugger::VisEntity::GetVisStateColor(Rep->bIsActorRepresentation, Rep->bHasMeshRendering);
			ImGui::TextColored(StateColor, "State: %s", StateStr);

			ImGui::Text("Mesh Grid: (%d, %d, %d)", Rep->MeshGridCoords.X, Rep->MeshGridCoords.Y, Rep->MeshGridCoords.Z);
			ImGui::Text("Physics Grid: (%d, %d, %d)", Rep->PhysicsGridCoords.X, Rep->PhysicsGridCoords.Y, Rep->PhysicsGridCoords.Z);
			ImGui::Text("Mesh Rendering: %s", Rep->bHasMeshRendering ? "Active" : "Inactive");
			ImGui::Text("Physics Body: %s", Rep->bHasPhysicsBody ? "Attached" : "None");
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
			if (Config->ActorClass)
			{
				ImGui::Text("  Actor Class: %s", TCHAR_TO_ANSI(*Config->ActorClass->GetName()));
			}
			ImGui::Text("  Cast Shadows: %s", Config->bCastShadows ? "true" : "false");
			ImGui::Text("  Physics Body: %s", Config->bEnablePhysicsBody ? "true" : "false");
			if (Config->bEnablePhysicsBody)
			{
				ImGui::Text("  Body Type: %s", Config->PhysicsBodyType == EArcMassPhysicsBodyType::Dynamic ? "Dynamic" : "Static");
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
#endif
}

void FArcVisEntityDebugger::DrawSelectedEntityInWorld()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		return;
	}

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

	const FVisEntityEntry& Entry = CachedEntities[SelectedEntityIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		return;
	}

	FVector Location = Entry.Location;
	if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
	{
		Location = TransformFrag->GetTransform().GetLocation();
	}

	const FArcVisRepresentationFragment* Rep = Manager->GetFragmentDataPtr<FArcVisRepresentationFragment>(Entity);
	FColor ShapeColor = FColor::Yellow;
	if (Rep)
	{
		ShapeColor = Rep->bIsActorRepresentation ? FColor::Green : FColor::Cyan;
	}

	// Draw a diamond/sphere at entity location
	DrawDebugSphere(World, Location, 60.f, 16, ShapeColor, false, -1.f, 0, 3.f);

	// Draw vertical line for visibility
	DrawDebugLine(World, Location, Location + FVector(0, 0, 300.f), ShapeColor, false, -1.f, 0, 2.f);

	// Draw label
	FString Label = FString::Printf(TEXT("E%d"), Entity.Index);
	if (Rep)
	{
		Label += Rep->bIsActorRepresentation ? TEXT(" [ACT]") : (Rep->bHasMeshRendering ? TEXT(" [MESH]") : TEXT(" [---]"));
	}
	DrawDebugString(World, Location + FVector(0, 0, 320.f), Label, nullptr, FColor::White, -1.f, true, 1.2f);
#endif
}

void FArcVisEntityDebugger::DrawGridVisualization()
{
#if WITH_MASSENTITY_DEBUG
	UWorld* World = Arcx::GameplayDebugger::VisEntity::GetDebugWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::VisEntity::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	// Draw mesh grid (green/cyan boundaries)
	DrawSingleGrid(World, Subsystem->GetMeshGrid(), Subsystem->GetLastMeshPlayerCell(),
		Subsystem->GetMeshActivationRadiusCells(), Subsystem->GetMeshDeactivationRadiusCells(),
		FColor::Green, FColor::Orange, Manager, true);

	// Draw physics grid (yellow/red boundaries, offset slightly to avoid z-fighting)
	DrawSingleGrid(World, Subsystem->GetPhysicsGrid(), Subsystem->GetLastPhysicsPlayerCell(),
		Subsystem->GetPhysicsActivationRadiusCells(), Subsystem->GetPhysicsDeactivationRadiusCells(),
		FColor::Yellow, FColor::Red, Manager, false);
#endif
}

void FArcVisEntityDebugger::DrawSingleGrid(UWorld* World, const FArcVisualizationGrid& Grid, const FIntVector& PlayerCell,
	int32 ActivationCells, int32 DeactivationCells, FColor InActiveColor, FColor BoundaryColor,
	FMassEntityManager* Manager, bool bIsMeshGrid)
{
#if WITH_MASSENTITY_DEBUG
	const float CellSize = Grid.CellSize;
	const float HalfCell = CellSize * 0.5f;
	// Offset physics grid drawing slightly to avoid overlap with mesh grid
	const float ZOffset = bIsMeshGrid ? 0.f : 50.f;

	// Draw all cells that contain entities
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
		bool bHasActor = false;
		for (const FMassEntityHandle& Entity : Entities)
		{
			if (Manager->IsEntityValid(Entity))
			{
				const FArcVisRepresentationFragment* Rep = Manager->GetFragmentDataPtr<FArcVisRepresentationFragment>(Entity);
				if (Rep)
				{
					bHasActor = Rep->bIsActorRepresentation;
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
		FColor DrawColor = bIsActive ? FColor::Green : (bIsInHysteresis ? FColor::Cyan : FColor::Blue);
		DrawDebugBox(World, CellCenter, FVector(HalfCell * 0.9f), DrawColor, false, -1.f, 0, 2.f);

		// Entity count label
		const TCHAR* GridPrefix = bIsMeshGrid ? TEXT("M") : TEXT("P");
		const TCHAR* StateStr = bHasActor ? TEXT("ACT") : (bHasMesh ? TEXT("MESH") : TEXT("---"));
		const FString CountLabel = FString::Printf(TEXT("[%s] %d %s"), GridPrefix, Entities.Num(), StateStr);
		DrawDebugString(World, CellCenter + FVector(0, 0, HalfCell * 0.7f), CountLabel, nullptr,
			DrawColor, -1.f, true, 1.f);
	}

	// Draw player cell highlight
	if (PlayerCell != FIntVector(TNumericLimits<int32>::Max()))
	{
		const FVector PlayerCellCenter(
			PlayerCell.X * CellSize + HalfCell,
			PlayerCell.Y * CellSize + HalfCell,
			PlayerCell.Z * CellSize + HalfCell + ZOffset
		);
		DrawDebugBox(World, PlayerCellCenter, FVector(HalfCell * 0.95f), FColor::White, false, -1.f, 0, 3.f);

		// Draw activation radius boundary
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
				DrawDebugBox(World, EdgeCellCenter, FVector(HalfCell), InActiveColor, false, -1.f, 0, 1.f);
			}
		}

		// Draw deactivation radius boundary (hysteresis edge)
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
				DrawDebugBox(World, EdgeCellCenter, FVector(HalfCell), BoundaryColor, false, -1.f, 0, 1.f);
			}
		}
	}
#endif
}
