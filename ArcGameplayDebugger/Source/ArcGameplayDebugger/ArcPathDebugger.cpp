// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPathDebugger.h"

#include "imgui.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassNavigationFragments.h"
#include "MassMovementFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "ArcPathDebuggerDrawComponent.h"
#include "NavigationPath.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

namespace ArcPathDebuggerLocal
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

	const FString GetMovementActionName(EMassMovementAction Action)
	{
		switch (Action)
		{
		case EMassMovementAction::Stand: return "Stand";
		case EMassMovementAction::Move: return "Move";
		case EMassMovementAction::Animate: return "Animate";
		default: return "Unknown";
		}
	}
}

void FArcPathDebugger::Initialize()
{
	SelectedEntityIndex = INDEX_NONE;
	EntityFilterBuf[0] = '\0';
	CachedEntities.Reset();
	LastRefreshTime = 0.f;
	RefreshEntityList();
}

void FArcPathDebugger::Uninitialize()
{
	DestroyDrawActor();
	CachedEntities.Reset();
	SelectedEntityIndex = INDEX_NONE;
}

void FArcPathDebugger::RefreshEntityList()
{
	CachedEntities.Reset();

	FMassEntityManager* Manager = ArcPathDebuggerLocal::GetEntityManager();
	if (!Manager)
	{
		return;
	}

#if WITH_MASSENTITY_DEBUG
	TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);

	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);

		// Only include entities that have move target fragment (i.e. navigating entities)
		if (!Composition.Contains<FMassMoveTargetFragment>())
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

			// Build a label with action state
			FString ActionStr = TEXT("?");
			if (const FMassMoveTargetFragment* MoveTarget = Manager->GetFragmentDataPtr<FMassMoveTargetFragment>(Entity))
			{
				ActionStr = ArcPathDebuggerLocal::GetMovementActionName(MoveTarget->GetCurrentAction());
			}

			Entry.Label = FString::Printf(TEXT("E%d [%s]"), Entity.Index, *ActionStr);
		}
	}
#endif
}

void FArcPathDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Path Debugger", &bShow))
	{
		ImGui::End();
		if (DrawComponent.IsValid()) { DrawComponent->ClearShapes(); }
		return;
	}

	FMassEntityManager* Manager = ArcPathDebuggerLocal::GetEntityManager();
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
	ImGui::Text("Path Entities: %d", CachedEntities.Num());

	// Auto-refresh every 2 seconds
	UWorld* World = ArcPathDebuggerLocal::GetDebugWorld();
	if (World)
	{
		float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastRefreshTime > 2.0f)
		{
			LastRefreshTime = CurrentTime;
			RefreshEntityList();
		}
	}

	// World draw toggles
	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();
	ImGui::Text("Draw:");
	ImGui::SameLine();
	ImGui::Checkbox("MoveTarget", &bDrawMoveTarget);
	ImGui::SameLine();
	ImGui::Checkbox("Velocity", &bDrawVelocity);
	ImGui::SameLine();
	ImGui::Checkbox("Steering", &bDrawSteering);
	ImGui::SameLine();
	ImGui::Checkbox("Forces", &bDrawForces);
	ImGui::SameLine();
	ImGui::Checkbox("ShortPath", &bDrawShortPath);
	ImGui::SameLine();
	ImGui::Checkbox("Avoidance", &bDrawAvoidance);
	ImGui::SameLine();
	ImGui::Checkbox("NavMesh", &bDrawNavMesh);
	if (bDrawNavMesh)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.f);
		ImGui::DragFloat("Z##NavMeshZ", &NavMeshZOffset, 1.f, 0.f, 200.f, "%.0f");
	}

	ImGui::Separator();

	// Split into two panes
	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.25f;

	// Left panel: entity list
	if (ImGui::BeginChild("PathEntityListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawEntityListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel: entity detail
	if (ImGui::BeginChild("PathEntityDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawEntityDetailPanel();
	}
	ImGui::EndChild();

	ImGui::End();

	// Draw path visualization in world for selected entity
	DrawPathInWorld();
#endif
}

void FArcPathDebugger::DrawEntityListPanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("Navigating Entities");
	ImGui::Separator();

	ImGui::InputText("Filter", EntityFilterBuf, IM_ARRAYSIZE(EntityFilterBuf));

	FString Filter = FString(ANSI_TO_TCHAR(EntityFilterBuf)).ToLower();

	if (ImGui::BeginChild("PathEntityScroll", ImVec2(0, 0)))
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

void FArcPathDebugger::DrawEntityDetailPanel()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		ImGui::TextDisabled("Select a navigating entity from the list");
		return;
	}

	FMassEntityManager* Manager = ArcPathDebuggerLocal::GetEntityManager();
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

	// --- Transform ---
	if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
	{
		FVector Loc = TransformFrag->GetTransform().GetLocation();
		FRotator Rot = TransformFrag->GetTransform().Rotator();
		ImGui::Text("Location: (%.0f, %.0f, %.0f)", Loc.X, Loc.Y, Loc.Z);
		ImGui::Text("Rotation: (P=%.1f, Y=%.1f, R=%.1f)", Rot.Pitch, Rot.Yaw, Rot.Roll);
	}

	ImGui::Spacing();

	// === MOVE TARGET ===
	if (ImGui::CollapsingHeader("Move Target", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const FMassMoveTargetFragment* MoveTarget = Manager->GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
		if (MoveTarget)
		{
			ImGui::Text("Center: (%.0f, %.0f, %.0f)", MoveTarget->Center.X, MoveTarget->Center.Y, MoveTarget->Center.Z);
			ImGui::Text("Forward: (%.2f, %.2f, %.2f)", MoveTarget->Forward.X, MoveTarget->Forward.Y, MoveTarget->Forward.Z);
			ImGui::Text("Distance To Goal: %.1f", MoveTarget->DistanceToGoal);
			ImGui::Text("Entity Distance To Goal: %.1f", MoveTarget->EntityDistanceToGoal);
			ImGui::Text("Slack Radius: %.1f", MoveTarget->SlackRadius);
			ImGui::Text("Desired Speed: %.1f", (float)MoveTarget->DesiredSpeed.Get());

			ImGui::Spacing();
			ImGui::Text("Current Action: %s (ID: %d)", TCHAR_TO_ANSI(*ArcPathDebuggerLocal::GetMovementActionName(MoveTarget->GetCurrentAction())), MoveTarget->GetCurrentActionID());
			ImGui::Text("Previous Action: %s", TCHAR_TO_ANSI(*ArcPathDebuggerLocal::GetMovementActionName(MoveTarget->GetPreviousAction())));
			ImGui::Text("Intent At Goal: %s", TCHAR_TO_ANSI(*ArcPathDebuggerLocal::GetMovementActionName(MoveTarget->IntentAtGoal)));

			ImGui::Spacing();
			ImGui::Text("Off Boundaries: %s", MoveTarget->bOffBoundaries ? "true" : "false");
			ImGui::Text("Steering Falling Behind: %s", MoveTarget->bSteeringFallingBehind ? "true" : "false");
		}
		else
		{
			ImGui::TextDisabled("No FMassMoveTargetFragment");
		}
	}

	// === VELOCITY ===
	if (ImGui::CollapsingHeader("Velocity", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const FMassVelocityFragment* Velocity = Manager->GetFragmentDataPtr<FMassVelocityFragment>(Entity);
		if (Velocity)
		{
			float Speed = Velocity->Value.Size();
			ImGui::Text("Velocity: (%.1f, %.1f, %.1f)", Velocity->Value.X, Velocity->Value.Y, Velocity->Value.Z);
			ImGui::Text("Speed: %.1f cm/s", Speed);

			// Speed bar
			float MaxSpeed = 500.f; // reasonable default for display
			if (const FMassMovementParameters* MovementParams = Manager->GetConstSharedFragmentDataPtr<FMassMovementParameters>(Entity))
			{
				MaxSpeed = MovementParams->MaxSpeed;
			}
			float SpeedRatio = FMath::Clamp(Speed / MaxSpeed, 0.f, 1.f);
			ImGui::ProgressBar(SpeedRatio, ImVec2(200, 0));
			ImGui::SameLine();
			ImGui::Text("%.0f / %.0f", Speed, MaxSpeed);
		}
		else
		{
			ImGui::TextDisabled("No FMassVelocityFragment");
		}
	}

	// === DESIRED MOVEMENT ===
	if (ImGui::CollapsingHeader("Desired Movement"))
	{
		const FMassDesiredMovementFragment* DesiredMovement = Manager->GetFragmentDataPtr<FMassDesiredMovementFragment>(Entity);
		if (DesiredMovement)
		{
			float DesiredSpeed = DesiredMovement->DesiredVelocity.Size();
			ImGui::Text("Desired Velocity: (%.1f, %.1f, %.1f)", DesiredMovement->DesiredVelocity.X, DesiredMovement->DesiredVelocity.Y, DesiredMovement->DesiredVelocity.Z);
			ImGui::Text("Desired Speed: %.1f cm/s", DesiredSpeed);
			FRotator DesiredFacingRot = DesiredMovement->DesiredFacing.Rotator();
			ImGui::Text("Desired Facing: (P=%.1f, Y=%.1f, R=%.1f)", DesiredFacingRot.Pitch, DesiredFacingRot.Yaw, DesiredFacingRot.Roll);
			if (DesiredMovement->DesiredMaxSpeedOverride < FLT_MAX)
			{
				ImGui::Text("Max Speed Override: %.1f", DesiredMovement->DesiredMaxSpeedOverride);
			}
			else
			{
				ImGui::TextDisabled("Max Speed Override: None");
			}
		}
		else
		{
			ImGui::TextDisabled("No FMassDesiredMovementFragment");
		}
	}

	// === STEERING ===
	if (ImGui::CollapsingHeader("Steering"))
	{
		const FMassSteeringFragment* Steering = Manager->GetFragmentDataPtr<FMassSteeringFragment>(Entity);
		if (Steering)
		{
			ImGui::Text("Desired Velocity: (%.1f, %.1f, %.1f)", Steering->DesiredVelocity.X, Steering->DesiredVelocity.Y, Steering->DesiredVelocity.Z);
			ImGui::Text("Desired Speed: %.1f cm/s", Steering->DesiredVelocity.Size());
		}
		else
		{
			ImGui::TextDisabled("No FMassSteeringFragment");
		}

		const FMassStandingSteeringFragment* StandingSteering = Manager->GetFragmentDataPtr<FMassStandingSteeringFragment>(Entity);
		if (StandingSteering)
		{
			ImGui::Spacing();
			ImGui::Text("Standing Steering:");
			ImGui::Text("  Target Location: (%.0f, %.0f, %.0f)", StandingSteering->TargetLocation.X, StandingSteering->TargetLocation.Y, StandingSteering->TargetLocation.Z);
			ImGui::Text("  Tracked Speed: %.1f", StandingSteering->TrackedTargetSpeed);
			ImGui::Text("  Selection Cooldown: %.2f", StandingSteering->TargetSelectionCooldown);
			ImGui::Text("  Updating Target: %s", StandingSteering->bIsUpdatingTarget ? "true" : "false");
			ImGui::Text("  Entered From Move: %s", StandingSteering->bEnteredFromMoveAction ? "true" : "false");
		}
	}

	// === FORCES ===
	if (ImGui::CollapsingHeader("Forces"))
	{
		const FMassForceFragment* Force = Manager->GetFragmentDataPtr<FMassForceFragment>(Entity);
		if (Force)
		{
			ImGui::Text("Force: (%.1f, %.1f, %.1f)", Force->Value.X, Force->Value.Y, Force->Value.Z);
			ImGui::Text("Magnitude: %.1f", Force->Value.Size());
		}
		else
		{
			ImGui::TextDisabled("No FMassForceFragment");
		}
	}

	// === GHOST LOCATION (Avoidance) ===
	if (ImGui::CollapsingHeader("Ghost Location"))
	{
		const FMassGhostLocationFragment* Ghost = Manager->GetFragmentDataPtr<FMassGhostLocationFragment>(Entity);
		if (Ghost)
		{
			ImGui::Text("Location: (%.0f, %.0f, %.0f)", Ghost->Location.X, Ghost->Location.Y, Ghost->Location.Z);
			ImGui::Text("Velocity: (%.1f, %.1f, %.1f)", Ghost->Velocity.X, Ghost->Velocity.Y, Ghost->Velocity.Z);
			ImGui::Text("Last Seen Action ID: %d", Ghost->LastSeenActionID);

			const FMassMoveTargetFragment* MoveTarget = Manager->GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
			if (MoveTarget)
			{
				bool bValid = Ghost->IsValid(MoveTarget->GetCurrentActionID());
				ImGui::Text("Valid For Current Action: %s", bValid ? "true" : "false");
			}
		}
		else
		{
			ImGui::TextDisabled("No FMassGhostLocationFragment");
		}
	}

	// === AVOIDANCE COLLIDER ===
	if (ImGui::CollapsingHeader("Avoidance Collider"))
	{
		const FMassAvoidanceColliderFragment* Collider = Manager->GetFragmentDataPtr<FMassAvoidanceColliderFragment>(Entity);
		if (Collider)
		{
			if (Collider->Type == EMassColliderType::Circle)
			{
				FMassCircleCollider Circle = Collider->GetCircleCollider();
				ImGui::Text("Type: Circle");
				ImGui::Text("Radius: %.1f", Circle.Radius);
			}
			else if (Collider->Type == EMassColliderType::Pill)
			{
				FMassPillCollider Pill = Collider->GetPillCollider();
				ImGui::Text("Type: Pill");
				ImGui::Text("Radius: %.1f", Pill.Radius);
				ImGui::Text("Half Length: %.1f", Pill.HalfLength);
			}
		}
		else
		{
			ImGui::TextDisabled("No FMassAvoidanceColliderFragment");
		}
	}

	// === NAVMESH SHORT PATH ===
	if (ImGui::CollapsingHeader("NavMesh Short Path"))
	{
		const FMassNavMeshShortPathFragment* ShortPath = Manager->GetFragmentDataPtr<FMassNavMeshShortPathFragment>(Entity);
		if (ShortPath)
		{
			ImGui::Text("Initialized: %s", ShortPath->bInitialized ? "true" : "false");
			ImGui::Text("Done: %s", ShortPath->bDone ? "true" : "false");
			ImGui::Text("Partial Result: %s", ShortPath->bPartialResult ? "true" : "false");
			ImGui::Text("Num Points: %d / %d", ShortPath->NumPoints, FMassNavMeshShortPathFragment::MaxPoints);
			ImGui::Text("Progress Distance: %.1f", ShortPath->MoveTargetProgressDistance);
			ImGui::Text("End Reached Distance: %.1f", ShortPath->EndReachedDistance);
			ImGui::Text("End Of Path Intent: %s", TCHAR_TO_ANSI(*ArcPathDebuggerLocal::GetMovementActionName(ShortPath->EndOfPathIntent)));

			if (ShortPath->NumPoints > 0)
			{
				ImGui::Spacing();
				if (ImGui::BeginTable("PathPointsTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
				{
					ImGui::TableSetupColumn("Idx", ImGuiTableColumnFlags_WidthFixed, 30.f);
					ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Distance", ImGuiTableColumnFlags_WidthFixed, 80.f);
					ImGui::TableSetupColumn("Tangent", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableHeadersRow();

					for (uint8 i = 0; i < ShortPath->NumPoints; ++i)
					{
						const FMassNavMeshPathPoint& Point = ShortPath->Points[i];
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%d", i);
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("(%.0f, %.0f, %.0f)", Point.Position.X, Point.Position.Y, Point.Position.Z);
						ImGui::TableSetColumnIndex(2);
						ImGui::Text("%.1f", (float)Point.Distance.Get());
						ImGui::TableSetColumnIndex(3);
						FVector Tangent = Point.Tangent.GetVector();
						ImGui::Text("(%.2f, %.2f)", Tangent.X, Tangent.Y);
					}

					ImGui::EndTable();
				}
			}
		}
		else
		{
			ImGui::TextDisabled("No FMassNavMeshShortPathFragment");
		}
	}

	// === NAVMESH CACHED PATH ===
	if (ImGui::CollapsingHeader("NavMesh Cached Path"))
	{
		const FMassNavMeshCachedPathFragment* CachedPath = Manager->GetFragmentDataPtr<FMassNavMeshCachedPathFragment>(Entity);
		if (CachedPath)
		{
			const char* PathSourceName = "Unset";
			switch (CachedPath->PathSource)
			{
			case EMassNavigationPathSource::NavMesh: PathSourceName = "NavMesh"; break;
			case EMassNavigationPathSource::Spline: PathSourceName = "Spline"; break;
			default: break;
			}
			ImGui::Text("Path Source: %s", PathSourceName);
			ImGui::Text("Next Start Index: %d", CachedPath->NavPathNextStartIndex);
			ImGui::Text("Has NavPath: %s", CachedPath->NavPath.IsValid() ? "true" : "false");
			ImGui::Text("Has Corridor: %s", CachedPath->Corridor.IsValid() ? "true" : "false");

			if (CachedPath->NavPath.IsValid())
			{
				ImGui::Text("Path Points: %d", CachedPath->NavPath->GetPathPoints().Num());
				ImGui::Text("Path Valid: %s", CachedPath->NavPath->IsValid() ? "true" : "false");
				ImGui::Text("Path Partial: %s", CachedPath->NavPath->IsPartial() ? "true" : "false");
			}
		}
		else
		{
			ImGui::TextDisabled("No FMassNavMeshCachedPathFragment");
		}
	}

	// === MOVEMENT PARAMETERS (ConstShared) ===
	if (ImGui::CollapsingHeader("Movement Parameters"))
	{
		const FMassMovementParameters* MovementParams = Manager->GetConstSharedFragmentDataPtr<FMassMovementParameters>(Entity);
		if (MovementParams)
		{
			ImGui::Text("Max Speed: %.1f cm/s", MovementParams->MaxSpeed);
			ImGui::Text("Max Acceleration: %.1f cm/s^2", MovementParams->MaxAcceleration);
			ImGui::Text("Default Desired Speed: %.1f cm/s", MovementParams->DefaultDesiredSpeed);
			ImGui::Text("Default Speed Variance: %.2f", MovementParams->DefaultDesiredSpeedVariance);
			ImGui::Text("Height Smoothing Time: %.2f s", MovementParams->HeightSmoothingTime);
			ImGui::Text("Code Driven Movement: %s", MovementParams->bIsCodeDrivenMovement ? "true" : "false");
			ImGui::Text("Movement Styles: %d", MovementParams->MovementStyles.Num());
		}
		else
		{
			ImGui::TextDisabled("No FMassMovementParameters");
		}
	}
#endif
}

void FArcPathDebugger::DrawPathInWorld()
{
#if WITH_MASSENTITY_DEBUG
	UWorld* World = ArcPathDebuggerLocal::GetDebugWorld();
	if (!World)
	{
		if (DrawComponent.IsValid()) { DrawComponent->ClearShapes(); }
		return;
	}

	const bool bHasSelectedEntity = SelectedEntityIndex != INDEX_NONE && CachedEntities.IsValidIndex(SelectedEntityIndex);
	const bool bNeedsDraw = bHasSelectedEntity || bDrawNavMesh;

	if (!bNeedsDraw)
	{
		if (DrawComponent.IsValid()) { DrawComponent->ClearShapes(); }
		return;
	}

	FArcPathDebugDrawData Data;

	// --- NavMesh overlay (independent of entity selection) ---
	if (bDrawNavMesh)
	{
		DrawNavMeshOverlay(World, Data);
	}

	// --- Entity-specific drawing ---
	if (!bHasSelectedEntity)
	{
		EnsureDrawActor(World);
		if (DrawComponent.IsValid()) { DrawComponent->UpdatePathData(Data); }
		return;
	}

	FMassEntityManager* Manager = ArcPathDebuggerLocal::GetEntityManager();
	if (!Manager)
	{
		EnsureDrawActor(World);
		if (DrawComponent.IsValid()) { DrawComponent->UpdatePathData(Data); }
		return;
	}

	const FEntityEntry& Entry = CachedEntities[SelectedEntityIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		EnsureDrawActor(World);
		if (DrawComponent.IsValid()) { DrawComponent->UpdatePathData(Data); }
		return;
	}

	const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity);
	if (!TransformFrag)
	{
		EnsureDrawActor(World);
		if (DrawComponent.IsValid()) { DrawComponent->UpdatePathData(Data); }
		return;
	}

	const FVector EntityLoc = TransformFrag->GetTransform().GetLocation();
	const FVector EntityForward = TransformFrag->GetTransform().GetRotation().GetForwardVector();
	constexpr float ZOffset = 50.f;
	const FVector BasePos = EntityLoc + FVector(0, 0, ZOffset);

	Data.BasePos = BasePos;
	Data.EntityForward = EntityForward;

	// --- Move Target ---
	if (bDrawMoveTarget)
	{
		const FMassMoveTargetFragment* MoveTarget = Manager->GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
		if (MoveTarget)
		{
			Data.bDrawMoveTarget = true;
			Data.MoveTargetPos = MoveTarget->Center + FVector(0, 0, ZOffset);
			Data.MoveTargetForward = MoveTarget->Forward;
			Data.MoveTargetSlackRadius = MoveTarget->SlackRadius;
			Data.MoveTargetLabel = FString::Printf(TEXT("Dist: %.0f | Spd: %.0f | %s"),
				MoveTarget->DistanceToGoal, (float)MoveTarget->DesiredSpeed.Get(),
				*ArcPathDebuggerLocal::GetMovementActionName(MoveTarget->GetCurrentAction()));
		}
	}

	// --- Velocity ---
	if (bDrawVelocity)
	{
		const FMassVelocityFragment* Velocity = Manager->GetFragmentDataPtr<FMassVelocityFragment>(Entity);
		if (Velocity && !Velocity->Value.IsNearlyZero())
		{
			Data.bDrawVelocity = true;
			Data.Velocity = Velocity->Value;
		}
	}

	// --- Steering ---
	if (bDrawSteering)
	{
		const FMassSteeringFragment* Steering = Manager->GetFragmentDataPtr<FMassSteeringFragment>(Entity);
		if (Steering && !Steering->DesiredVelocity.IsNearlyZero())
		{
			Data.bDrawSteering = true;
			Data.DesiredVelocity = Steering->DesiredVelocity;
		}

		const FMassStandingSteeringFragment* StandingSteering = Manager->GetFragmentDataPtr<FMassStandingSteeringFragment>(Entity);
		if (StandingSteering)
		{
			Data.bDrawSteering = true;
			Data.bHasStandTarget = true;
			Data.StandTarget = StandingSteering->TargetLocation + FVector(0, 0, ZOffset);
		}
	}

	// --- Forces ---
	if (bDrawForces)
	{
		const FMassForceFragment* Force = Manager->GetFragmentDataPtr<FMassForceFragment>(Entity);
		if (Force && !Force->Value.IsNearlyZero())
		{
			Data.bDrawForces = true;
			Data.ForceValue = Force->Value;
		}
	}

	// --- Ghost + Avoidance Collider ---
	if (bDrawAvoidance)
	{
		const FMassGhostLocationFragment* Ghost = Manager->GetFragmentDataPtr<FMassGhostLocationFragment>(Entity);
		if (Ghost)
		{
			const FMassMoveTargetFragment* MoveTarget = Manager->GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
			if (MoveTarget && Ghost->IsValid(MoveTarget->GetCurrentActionID()))
			{
				Data.bHasGhost = true;
				Data.GhostPos = Ghost->Location + FVector(0, 0, ZOffset);
				Data.GhostVelocity = Ghost->Velocity;
			}
		}

		const FMassAvoidanceColliderFragment* Collider = Manager->GetFragmentDataPtr<FMassAvoidanceColliderFragment>(Entity);
		if (Collider)
		{
			Data.bDrawAvoidance = true;
			if (Collider->Type == EMassColliderType::Circle)
			{
				Data.ColliderShape = FArcPathDebugDrawData::EColliderShape::Circle;
				Data.ColliderRadius = Collider->GetCircleCollider().Radius;
			}
			else if (Collider->Type == EMassColliderType::Pill)
			{
				const FMassPillCollider Pill = Collider->GetPillCollider();
				Data.ColliderShape = FArcPathDebugDrawData::EColliderShape::Pill;
				Data.ColliderRadius = Pill.Radius;
				Data.PillHalfLength = Pill.HalfLength;
			}
		}
	}

	// --- NavMesh Short Path ---
	if (bDrawShortPath)
	{
		const FMassNavMeshShortPathFragment* ShortPath = Manager->GetFragmentDataPtr<FMassNavMeshShortPathFragment>(Entity);
		if (ShortPath && ShortPath->bInitialized && ShortPath->NumPoints > 0)
		{
			Data.bDrawShortPath = true;
			Data.PathZOffset = ZOffset;
			Data.PathPoints.Reserve(ShortPath->NumPoints);

			for (uint8 i = 0; i < ShortPath->NumPoints; ++i)
			{
				const FMassNavMeshPathPoint& SrcPoint = ShortPath->Points[i];
				FArcPathDebugNavPoint& DstPoint = Data.PathPoints.AddDefaulted_GetRef();
				DstPoint.Position = SrcPoint.Position + FVector(0, 0, ZOffset);
				DstPoint.Tangent = SrcPoint.Tangent.GetVector();
				DstPoint.Left = SrcPoint.Left;
				DstPoint.Right = SrcPoint.Right;
				DstPoint.Label = FString::Printf(TEXT("%d (%.0f)"), i, (float)SrcPoint.Distance.Get());
			}
		}
	}

	EnsureDrawActor(World);
	if (DrawComponent.IsValid())
	{
		DrawComponent->UpdatePathData(Data);
	}
#endif
}

void FArcPathDebugger::DrawNavMeshOverlay(UWorld* World, FArcPathDebugDrawData& OutData)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return;
	}

	ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
	if (!RecastNavMesh)
	{
		return;
	}

	TArray<FNavTileRef> TileRefs;
	RecastNavMesh->GetAllNavMeshTiles(TileRefs);

	const FVector ZOff(0.0, 0.0, NavMeshZOffset);

	TArray<FNavPoly> TilePolys;
	TArray<FVector> PolyVerts;

	for (const FNavTileRef& TileRef : TileRefs)
	{
		TilePolys.Reset();
		if (!RecastNavMesh->GetPolysInTile(TileRef, TilePolys))
		{
			continue;
		}

		for (const FNavPoly& Poly : TilePolys)
		{
			PolyVerts.Reset();
			if (!RecastNavMesh->GetPolyVerts(Poly.Ref, PolyVerts) || PolyVerts.Num() < 3)
			{
				continue;
			}

			FArcPathDebugDrawData::FNavMeshPoly& OutPoly = OutData.NavMeshPolys.AddDefaulted_GetRef();
			OutPoly.Verts.Reserve(PolyVerts.Num());
			for (const FVector& Vert : PolyVerts)
			{
				OutPoly.Verts.Add(Vert + ZOff);
			}
		}
	}

	OutData.bDrawNavMesh = OutData.NavMeshPolys.Num() > 0;
}

void FArcPathDebugger::EnsureDrawActor(UWorld* World)
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
	NewActor->SetActorLabel(TEXT("PathDebuggerDraw"));
#endif

	UArcPathDebuggerDrawComponent* NewComponent = NewObject<UArcPathDebuggerDrawComponent>(NewActor, UArcPathDebuggerDrawComponent::StaticClass());
	NewComponent->RegisterComponent();
	NewActor->AddInstanceComponent(NewComponent);

	DrawActor = NewActor;
	DrawComponent = NewComponent;
}

void FArcPathDebugger::DestroyDrawActor()
{
	if (DrawActor.IsValid())
	{
		DrawActor->Destroy();
	}
	DrawActor.Reset();
	DrawComponent.Reset();
}
