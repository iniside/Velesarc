// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMobileVisEntityDebugger.h"

#include "imgui.h"
#include "ArcMass/MobileVisualization/ArcMobileVisualization.h"
#include "MassActorSubsystem.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

namespace Arcx::GameplayDebugger::MobileVisEntity
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

	const char* GetLODString(EArcMobileVisLOD LOD)
	{
		switch (LOD)
		{
			case EArcMobileVisLOD::Actor:      return "ACTOR";
			case EArcMobileVisLOD::InstancedMesh: return "ISM";
			default:                           return "NONE";
		}
	}

	ImVec4 GetLODColor(EArcMobileVisLOD LOD)
	{
		switch (LOD)
		{
			case EArcMobileVisLOD::Actor:      return ImVec4(0.31f, 0.78f, 0.39f, 1.0f);
			case EArcMobileVisLOD::InstancedMesh: return ImVec4(0.27f, 0.51f, 0.75f, 1.0f);
			default:                           return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
		}
	}
}

void FArcMobileVisEntityDebugger::Initialize()
{
	SelectedEntityIndex = INDEX_NONE;
	EntityFilterBuf[0] = '\0';
	CachedEntities.Reset();
	LastRefreshTime = 0.f;
	RefreshEntityList();
}

void FArcMobileVisEntityDebugger::Uninitialize()
{
	CachedEntities.Reset();
	SelectedEntityIndex = INDEX_NONE;
}

void FArcMobileVisEntityDebugger::RefreshEntityList()
{
	CachedEntities.Reset();

#if WITH_MASSENTITY_DEBUG
	FMassEntityManager* Manager = Arcx::GameplayDebugger::MobileVisEntity::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);

	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);
		if (!Composition.Contains<FArcMobileVisEntityTag>())
		{
			continue;
		}

		TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle& Entity : Entities)
		{
			FMobileVisEntityEntry& Entry = CachedEntities.AddDefaulted_GetRef();
			Entry.Entity = Entity;

			const FArcMobileVisLODFragment* LODFrag = Manager->GetFragmentDataPtr<FArcMobileVisLODFragment>(Entity);
			if (LODFrag)
			{
				Entry.LOD = LODFrag->CurrentLOD;
			}

			if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
			{
				Entry.Location = TransformFrag->GetTransform().GetLocation();
			}

			const char* LODStr = Arcx::GameplayDebugger::MobileVisEntity::GetLODString(Entry.LOD);
			Entry.Label = FString::Printf(TEXT("E%d [%hs]"), Entity.Index, LODStr);
		}
	}
#endif
}

void FArcMobileVisEntityDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Arc Mobile Visualization", &bShow))
	{
		ImGui::End();
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::MobileVisEntity::GetEntityManager();
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
	ImGui::Text("Entities: %d", CachedEntities.Num());

	// Auto-refresh every 2 seconds
	UWorld* World = Arcx::GameplayDebugger::MobileVisEntity::GetDebugWorld();
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
	float LeftPanelWidth = PanelWidth * 0.30f;

	if (ImGui::BeginChild("MobileVisEntityListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawEntityListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("MobileVisEntityDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawEntityDetailPanel();
	}
	ImGui::EndChild();

	ImGui::End();
#endif
}

void FArcMobileVisEntityDebugger::DrawEntityListPanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("Mobile Vis Entities");
	ImGui::Separator();

	ImGui::InputText("Filter", EntityFilterBuf, IM_ARRAYSIZE(EntityFilterBuf));

	// LOD summary line
	int32 ActorCount = 0;
	int32 ISMCount = 0;
	int32 NoneCount = 0;
	for (const FMobileVisEntityEntry& Entry : CachedEntities)
	{
		switch (Entry.LOD)
		{
			case EArcMobileVisLOD::Actor:      ++ActorCount; break;
			case EArcMobileVisLOD::InstancedMesh: ++ISMCount;   break;
			default:                           ++NoneCount;  break;
		}
	}

	ImGui::TextColored(Arcx::GameplayDebugger::MobileVisEntity::GetLODColor(EArcMobileVisLOD::Actor), "A:%d", ActorCount);
	ImGui::SameLine();
	ImGui::TextColored(Arcx::GameplayDebugger::MobileVisEntity::GetLODColor(EArcMobileVisLOD::InstancedMesh), "I:%d", ISMCount);
	ImGui::SameLine();
	ImGui::TextColored(Arcx::GameplayDebugger::MobileVisEntity::GetLODColor(EArcMobileVisLOD::None), "N:%d", NoneCount);

	ImGui::Separator();

	FString Filter = FString(ANSI_TO_TCHAR(EntityFilterBuf)).ToLower();

	if (ImGui::BeginChild("MobileVisEntityScroll", ImVec2(0, 0)))
	{
		for (int32 i = 0; i < CachedEntities.Num(); ++i)
		{
			const FMobileVisEntityEntry& Entry = CachedEntities[i];

			if (!Filter.IsEmpty() && !Entry.Label.ToLower().Contains(Filter))
			{
				continue;
			}

			ImGui::PushID(i);

			ImVec4 Color = Arcx::GameplayDebugger::MobileVisEntity::GetLODColor(Entry.LOD);
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

void FArcMobileVisEntityDebugger::DrawEntityDetailPanel()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		ImGui::TextDisabled("Select a mobile vis entity from the list");
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::MobileVisEntity::GetEntityManager();
	if (!Manager)
	{
		return;
	}

	const FMobileVisEntityEntry& Entry = CachedEntities[SelectedEntityIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Entity is no longer active");
		return;
	}

	// --- Header ---
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Entity %d (SN:%d)", Entity.Index, Entity.SerialNumber);

	if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
	{
		FVector Loc = TransformFrag->GetTransform().GetLocation();
		ImGui::Text("Location: (%.0f, %.0f, %.0f)", Loc.X, Loc.Y, Loc.Z);
	}

	ImGui::Spacing();

	// === LOD STATE ===
	if (ImGui::CollapsingHeader("LOD State", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const FArcMobileVisLODFragment* LODFrag = Manager->GetFragmentDataPtr<FArcMobileVisLODFragment>(Entity);
		if (LODFrag)
		{
			ImVec4 CurColor = Arcx::GameplayDebugger::MobileVisEntity::GetLODColor(LODFrag->CurrentLOD);
			ImGui::TextColored(CurColor, "Current LOD: %s", Arcx::GameplayDebugger::MobileVisEntity::GetLODString(LODFrag->CurrentLOD));

			ImVec4 PrevColor = Arcx::GameplayDebugger::MobileVisEntity::GetLODColor(LODFrag->PrevLOD);
			ImGui::TextColored(PrevColor, "Previous LOD: %s", Arcx::GameplayDebugger::MobileVisEntity::GetLODString(LODFrag->PrevLOD));
		}
		else
		{
			ImGui::TextDisabled("No FArcMobileVisLODFragment");
		}
	}

	// === REPRESENTATION ===
	if (ImGui::CollapsingHeader("Representation", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const FArcMobileVisRepFragment* RepFrag = Manager->GetFragmentDataPtr<FArcMobileVisRepFragment>(Entity);
		if (RepFrag)
		{
			ImGui::Text("Grid Coords: (%d, %d, %d)", RepFrag->GridCoords.X, RepFrag->GridCoords.Y, RepFrag->GridCoords.Z);
			ImGui::Text("Region Coord: (%d, %d, %d)", RepFrag->RegionCoord.X, RepFrag->RegionCoord.Y, RepFrag->RegionCoord.Z);
			ImGui::Text("ISM Instance ID: %d", RepFrag->ISMInstanceId);
			ImGui::Text("Is Actor: %s", RepFrag->bIsActorRepresentation ? "true" : "false");

			if (RepFrag->CurrentISMMesh)
			{
				ImGui::Text("Current ISM Mesh: %s", TCHAR_TO_ANSI(*RepFrag->CurrentISMMesh->GetName()));
			}
			else
			{
				ImGui::TextDisabled("Current ISM Mesh: (none)");
			}

			ImGui::Text("Last Position: (%.0f, %.0f, %.0f)",
				RepFrag->LastPosition.X, RepFrag->LastPosition.Y, RepFrag->LastPosition.Z);
		}
		else
		{
			ImGui::TextDisabled("No FArcMobileVisRepFragment");
		}
	}

	// === HYDRATED ACTOR ===
	const FMassActorFragment* ActorFrag = Manager->GetFragmentDataPtr<FMassActorFragment>(Entity);
	if (ActorFrag && ActorFrag->Get())
	{
		if (ImGui::CollapsingHeader("Hydrated Actor", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const AActor* Actor = ActorFrag->Get();
			ImGui::Text("Name: %s", TCHAR_TO_ANSI(*Actor->GetName()));
			ImGui::Text("Class: %s", TCHAR_TO_ANSI(*Actor->GetClass()->GetName()));
			FVector ActorLoc = Actor->GetActorLocation();
			ImGui::Text("Location: (%.0f, %.0f, %.0f)", ActorLoc.X, ActorLoc.Y, ActorLoc.Z);
		}
	}

	// === CONFIG ===
	if (ImGui::CollapsingHeader("Config", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const FArcMobileVisConfigFragment* ConfigFrag = Manager->GetConstSharedFragmentDataPtr<FArcMobileVisConfigFragment>(Entity);
		if (ConfigFrag)
		{
			ImGui::TextDisabled("FArcMobileVisConfigFragment:");
			if (ConfigFrag->StaticMesh)
			{
				ImGui::Text("  StaticMesh: %s", TCHAR_TO_ANSI(*ConfigFrag->StaticMesh->GetName()));
			}
			else
			{
				ImGui::TextDisabled("  StaticMesh: (none)");
			}

			if (ConfigFrag->ActorClass)
			{
				ImGui::Text("  ActorClass: %s", TCHAR_TO_ANSI(*ConfigFrag->ActorClass->GetName()));
			}
			else
			{
				ImGui::TextDisabled("  ActorClass: (none)");
			}

			if (ConfigFrag->ISMManagerClass)
			{
				ImGui::Text("  ISMManagerClass: %s", TCHAR_TO_ANSI(*ConfigFrag->ISMManagerClass->GetName()));
			}
			else
			{
				ImGui::TextDisabled("  ISMManagerClass: (none)");
			}

			ImGui::Text("  bCastShadows: %s", ConfigFrag->bCastShadows ? "true" : "false");
			ImGui::Text("  MaterialOverrides: %d", ConfigFrag->MaterialOverrides.Num());
		}
		else
		{
			ImGui::TextDisabled("No FArcMobileVisConfigFragment");
		}

		const FArcMobileVisDistanceConfigFragment* DistConfigFrag = Manager->GetConstSharedFragmentDataPtr<FArcMobileVisDistanceConfigFragment>(Entity);
		if (DistConfigFrag)
		{
			ImGui::Spacing();
			ImGui::TextDisabled("FArcMobileVisDistanceConfigFragment:");
			ImGui::Text("  ActorRadius: %.0f", DistConfigFrag->ActorRadius);
			ImGui::Text("  ISMRadius: %.0f", DistConfigFrag->ISMRadius);
			ImGui::Text("  HysteresisPercent: %.1f%%", DistConfigFrag->HysteresisPercent);

			const char* PolicyStr = (DistConfigFrag->CellUpdatePolicy == EArcMobileVisCellUpdatePolicy::EveryTick)
				? "EveryTick"
				: "DistanceThreshold";
			ImGui::Text("  CellUpdatePolicy: %s", PolicyStr);
		}
		else
		{
			ImGui::TextDisabled("No FArcMobileVisDistanceConfigFragment");
		}
	}

	// === SOURCE DISTANCE ===
	if (ImGui::CollapsingHeader("Source Distance"))
	{
		UWorld* World = Arcx::GameplayDebugger::MobileVisEntity::GetDebugWorld();
		UArcMobileVisSubsystem* MobileVisSub = World ? World->GetSubsystem<UArcMobileVisSubsystem>() : nullptr;
		if (!MobileVisSub)
		{
			ImGui::TextDisabled("UArcMobileVisSubsystem not available");
		}
		else
		{
			const FArcMobileVisRepFragment* RepFrag = Manager->GetFragmentDataPtr<FArcMobileVisRepFragment>(Entity);
			FVector EntityWorldPos = Entry.Location;
			if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
			{
				EntityWorldPos = TransformFrag->GetTransform().GetLocation();
			}

			const TMap<FMassEntityHandle, FIntVector>& SourcePositions = MobileVisSub->GetSourcePositions();

			float NearestDist = TNumericLimits<float>::Max();
			FIntVector NearestCell = FIntVector::ZeroValue;
			bool bFoundSource = false;

			const float CellSize = MobileVisSub->GetCellSize();

			for (const TPair<FMassEntityHandle, FIntVector>& SourcePair : SourcePositions)
			{
				FVector SourceWorldPos = FVector(
					SourcePair.Value.X * CellSize + CellSize * 0.5f,
					SourcePair.Value.Y * CellSize + CellSize * 0.5f,
					SourcePair.Value.Z * CellSize + CellSize * 0.5f
				);

				float Dist = FVector::Dist(EntityWorldPos, SourceWorldPos);
				if (Dist < NearestDist)
				{
					NearestDist = Dist;
					NearestCell = SourcePair.Value;
					bFoundSource = true;
				}
			}

			if (!bFoundSource)
			{
				ImGui::TextDisabled("No source entities found");
			}
			else
			{
				ImGui::Text("Distance: %.0f units", NearestDist);
				ImGui::Text("Source Cell: (%d, %d, %d)", NearestCell.X, NearestCell.Y, NearestCell.Z);

				const FArcMobileVisDistanceConfigFragment* DistConfigFrag = Manager->GetConstSharedFragmentDataPtr<FArcMobileVisDistanceConfigFragment>(Entity);
				if (DistConfigFrag)
				{
					if (NearestDist <= DistConfigFrag->ActorRadius)
					{
						ImGui::TextColored(Arcx::GameplayDebugger::MobileVisEntity::GetLODColor(EArcMobileVisLOD::Actor), "Within Actor radius");
					}
					else if (NearestDist <= DistConfigFrag->ISMRadius)
					{
						ImGui::TextColored(Arcx::GameplayDebugger::MobileVisEntity::GetLODColor(EArcMobileVisLOD::InstancedMesh), "Within ISM radius");
					}
					else
					{
						ImGui::TextColored(Arcx::GameplayDebugger::MobileVisEntity::GetLODColor(EArcMobileVisLOD::None), "Outside all radii");
					}
				}
			}
		}
	}
#endif
}
