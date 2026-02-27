// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisEntityDebugger.h"

#include "imgui.h"
#include "ArcMass/ArcMassEntityVisualization.h"
#include "ArcMass/ArcVisLifecycle.h"
#include "ArcMass/ArcMassLifecycle.h"
#include "DrawDebugHelpers.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

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

	const char* GetVisStateString(bool bIsActor, int32 ISMInstanceId)
	{
		if (bIsActor)
		{
			return "Full Actor";
		}
		if (ISMInstanceId != INDEX_NONE)
		{
			return "ISM Mesh";
		}
		return "No Visualization";
	}

	ImVec4 GetVisStateColor(bool bIsActor, int32 ISMInstanceId)
	{
		if (bIsActor)
		{
			return ImVec4(0.3f, 1.0f, 0.3f, 1.0f);    // Green
		}
		if (ISMInstanceId != INDEX_NONE)
		{
			return ImVec4(0.4f, 0.7f, 1.0f, 1.0f);     // Blue
		}
		return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);         // Gray
	}

	const char* GetLifecyclePhaseString(EArcLifecyclePhase Phase)
	{
		switch (Phase)
		{
		case EArcLifecyclePhase::Start:   return "Start";
		case EArcLifecyclePhase::Growing: return "Growing";
		case EArcLifecyclePhase::Grown:   return "Grown";
		case EArcLifecyclePhase::Dying:   return "Dying";
		case EArcLifecyclePhase::Dead:    return "Dead";
		default: return "Unknown";
		}
	}

	ImVec4 GetLifecyclePhaseColor(EArcLifecyclePhase Phase)
	{
		switch (Phase)
		{
		case EArcLifecyclePhase::Start:   return ImVec4(0.8f, 0.8f, 0.3f, 1.0f);  // Yellow
		case EArcLifecyclePhase::Growing: return ImVec4(0.3f, 1.0f, 0.3f, 1.0f);  // Green
		case EArcLifecyclePhase::Grown:   return ImVec4(0.3f, 0.8f, 1.0f, 1.0f);  // Cyan
		case EArcLifecyclePhase::Dying:   return ImVec4(1.0f, 0.6f, 0.3f, 1.0f);  // Orange
		case EArcLifecyclePhase::Dead:    return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red
		default: return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		}
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

	FMassEntityManager* Manager = GetEntityManager();
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
				const char* State = Rep ? (Rep->bIsActorRepresentation ? "ACT" : "ISM") : "???";
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

	FMassEntityManager* Manager = GetEntityManager();
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

	FMassEntityManager* Manager = GetEntityManager();
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
			const char* StateStr = GetVisStateString(Rep->bIsActorRepresentation, Rep->ISMInstanceId);
			ImVec4 StateColor = GetVisStateColor(Rep->bIsActorRepresentation, Rep->ISMInstanceId);
			ImGui::TextColored(StateColor, "State: %s", StateStr);

			ImGui::Text("Grid Coords: (%d, %d, %d)", Rep->GridCoords.X, Rep->GridCoords.Y, Rep->GridCoords.Z);

			if (!Rep->bIsActorRepresentation)
			{
				ImGui::Text("ISM Instance ID: %d", Rep->ISMInstanceId);
				ImGui::Text("Partition Slot: %d", Rep->PartitionSlotIndex);
				if (Rep->CurrentISMMesh)
				{
					ImGui::Text("ISM Mesh: %s", TCHAR_TO_ANSI(*Rep->CurrentISMMesh->GetName()));
				}
				else
				{
					ImGui::TextDisabled("ISM Mesh: (base config)");
				}
			}
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
			if (Config->StaticMesh)
			{
				ImGui::Text("  Base Mesh: %s", TCHAR_TO_ANSI(*Config->StaticMesh->GetName()));
			}
			if (Config->ActorClass)
			{
				ImGui::Text("  Actor Class: %s", TCHAR_TO_ANSI(*Config->ActorClass->GetName()));
			}
			ImGui::Text("  Cast Shadows: %s", Config->bCastShadows ? "true" : "false");
		}
	}

	// === LIFECYCLE ===
	const FArcVisLifecycleFragment* LCFrag = Manager->GetFragmentDataPtr<FArcVisLifecycleFragment>(Entity);
	if (LCFrag)
	{
		if (ImGui::CollapsingHeader("Lifecycle", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImVec4 PhaseColor = GetLifecyclePhaseColor(LCFrag->CurrentPhase);
			ImGui::TextColored(PhaseColor, "Phase: %s", GetLifecyclePhaseString(LCFrag->CurrentPhase));
			ImGui::Text("Previous Phase: %s", GetLifecyclePhaseString(LCFrag->PreviousPhase));
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

				ImGui::TextDisabled("Auto Tick: %s", LCConfig->bDisableAutoTick ? "disabled" : "enabled");
			}
		}
	}

	// Also check non-vis lifecycle
	const FArcLifecycleFragment* BaseLCFrag = Manager->GetFragmentDataPtr<FArcLifecycleFragment>(Entity);
	if (BaseLCFrag && !LCFrag)
	{
		if (ImGui::CollapsingHeader("Lifecycle (Base)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImVec4 PhaseColor = GetLifecyclePhaseColor(BaseLCFrag->CurrentPhase);
			ImGui::TextColored(PhaseColor, "Phase: %s", GetLifecyclePhaseString(BaseLCFrag->CurrentPhase));
			ImGui::Text("Previous Phase: %s", GetLifecyclePhaseString(BaseLCFrag->PreviousPhase));
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
		Label += Rep->bIsActorRepresentation ? TEXT(" [ACT]") : TEXT(" [ISM]");
	}
	DrawDebugString(World, Location + FVector(0, 0, 320.f), Label, nullptr, FColor::White, -1.f, true, 1.2f);
#endif
}

void FArcVisEntityDebugger::DrawGridVisualization()
{
#if WITH_MASSENTITY_DEBUG
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FMassEntityManager* Manager = GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FArcVisualizationGrid& Grid = Subsystem->GetGrid();
	const float CellSize = Grid.CellSize;
	const float HalfCell = CellSize * 0.5f;
	const FIntVector PlayerCell = Subsystem->GetLastPlayerCell();

	// Draw all cells that contain entities
	for (const auto& CellPair : Grid.CellEntities)
	{
		const FIntVector& CellCoord = CellPair.Key;
		const TArray<FMassEntityHandle>& Entities = CellPair.Value;

		const FVector CellCenter(
			CellCoord.X * CellSize + HalfCell,
			CellCoord.Y * CellSize + HalfCell,
			CellCoord.Z * CellSize + HalfCell
		);

		// Determine cell state from first valid entity
		bool bIsActorCell = false;
		for (const FMassEntityHandle& Entity : Entities)
		{
			if (Manager->IsEntityValid(Entity))
			{
				const FArcVisRepresentationFragment* Rep = Manager->GetFragmentDataPtr<FArcVisRepresentationFragment>(Entity);
				if (Rep)
				{
					bIsActorCell = Rep->bIsActorRepresentation;
					break;
				}
			}
		}

		const FColor DrawColor = bIsActorCell ? FColor::Green : FColor::Blue;
		DrawDebugBox(World, CellCenter, FVector(HalfCell * 0.9f), DrawColor, false, -1.f, 0, 2.f);

		// Entity count label
		const FString CountLabel = FString::Printf(TEXT("%d %s"), Entities.Num(), bIsActorCell ? TEXT("ACT") : TEXT("ISM"));
		DrawDebugString(World, CellCenter + FVector(0, 0, HalfCell * 0.7f), CountLabel, nullptr,
			bIsActorCell ? FColor::Green : FColor::Cyan, -1.f, true, 1.f);
	}

	// Draw player cell highlight
	if (PlayerCell != FIntVector(TNumericLimits<int32>::Max()))
	{
		const FVector PlayerCellCenter(
			PlayerCell.X * CellSize + HalfCell,
			PlayerCell.Y * CellSize + HalfCell,
			PlayerCell.Z * CellSize + HalfCell
		);
		DrawDebugBox(World, PlayerCellCenter, FVector(HalfCell * 0.95f), FColor::White, false, -1.f, 0, 3.f);

		// Draw swap radius boundary
		const int32 RadiusCells = Subsystem->GetSwapRadiusCells();
		TArray<FIntVector> ActiveCells;
		Grid.GetCellsInRadius(PlayerCell, RadiusCells, ActiveCells);

		for (const FIntVector& Cell : ActiveCells)
		{
			const int32 DX = FMath::Abs(Cell.X - PlayerCell.X);
			const int32 DY = FMath::Abs(Cell.Y - PlayerCell.Y);
			const int32 DZ = FMath::Abs(Cell.Z - PlayerCell.Z);
			if (DX == RadiusCells || DY == RadiusCells || DZ == RadiusCells)
			{
				const FVector EdgeCellCenter(
					Cell.X * CellSize + HalfCell,
					Cell.Y * CellSize + HalfCell,
					Cell.Z * CellSize + HalfCell
				);
				DrawDebugBox(World, EdgeCellCenter, FVector(HalfCell), FColor::Orange, false, -1.f, 0, 1.f);
			}
		}
	}
#endif
}
