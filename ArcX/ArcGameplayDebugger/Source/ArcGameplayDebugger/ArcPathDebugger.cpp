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
#include "DrawDebugHelpers.h"
#include "NavigationPath.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

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
	if (SelectedEntityIndex == INDEX_NONE || !CachedEntities.IsValidIndex(SelectedEntityIndex))
	{
		return;
	}

	UWorld* World = ArcPathDebuggerLocal::GetDebugWorld();
	if (!World)
	{
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
		return;
	}

	const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity);
	if (!TransformFrag)
	{
		return;
	}

	const FVector EntityLoc = TransformFrag->GetTransform().GetLocation();
	const FVector EntityForward = TransformFrag->GetTransform().GetRotation().GetForwardVector();
	constexpr float ZOffset = 50.f;
	const FVector BasePos = EntityLoc + FVector(0, 0, ZOffset);

	// Draw entity marker
	DrawDebugSphere(World, BasePos, 25.f, 12, FColor::Cyan, false, -1.f, 0, 2.f);
	DrawDebugDirectionalArrow(World, BasePos, BasePos + EntityForward * 80.f, 20.f, FColor::Cyan, false, -1.f, 0, 2.f);

	// --- Move Target ---
	if (bDrawMoveTarget)
	{
		const FMassMoveTargetFragment* MoveTarget = Manager->GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
		if (MoveTarget)
		{
			FVector TargetPos = MoveTarget->Center + FVector(0, 0, ZOffset);

			// Draw move target sphere
			DrawDebugSphere(World, TargetPos, 20.f, 8, FColor(255, 0, 200), false, -1.f, 0, 2.f); // Magenta
			// Draw forward direction at move target
			DrawDebugDirectionalArrow(World, TargetPos, TargetPos + MoveTarget->Forward * 60.f, 15.f, FColor(255, 0, 200), false, -1.f, 0, 2.f);
			// Draw line from entity to move target
			DrawDebugLine(World, BasePos, TargetPos, FColor(255, 140, 0), false, -1.f, 0, 2.f); // Orange line

			// Draw slack radius
			if (MoveTarget->SlackRadius > 0.f)
			{
				DrawDebugCircle(World, TargetPos, MoveTarget->SlackRadius, 24, FColor(255, 0, 200, 80), false, -1.f, 0, 1.f, FVector(0, 1, 0), FVector(1, 0, 0), false);
			}

			// Text label
			FString MoveLabel = FString::Printf(TEXT("Dist: %.0f | Spd: %.0f | %s"),
				MoveTarget->DistanceToGoal, (float)MoveTarget->DesiredSpeed.Get(),
				*ArcPathDebuggerLocal::GetMovementActionName(MoveTarget->GetCurrentAction()));
			DrawDebugString(World, TargetPos + FVector(0, 0, 30.f), MoveLabel, nullptr, FColor::White, -1.f, true, 0.9f);
		}
	}

	// --- Velocity ---
	if (bDrawVelocity)
	{
		const FMassVelocityFragment* Velocity = Manager->GetFragmentDataPtr<FMassVelocityFragment>(Entity);
		if (Velocity && !Velocity->Value.IsNearlyZero())
		{
			DrawDebugDirectionalArrow(World, BasePos + FVector(0, 0, 2), BasePos + FVector(0, 0, 2) + Velocity->Value, 15.f, FColor::Yellow, false, -1.f, 0, 2.5f);
		}
	}

	// --- Steering ---
	if (bDrawSteering)
	{
		const FMassSteeringFragment* Steering = Manager->GetFragmentDataPtr<FMassSteeringFragment>(Entity);
		if (Steering && !Steering->DesiredVelocity.IsNearlyZero())
		{
			DrawDebugDirectionalArrow(World, BasePos + FVector(0, 0, 4), BasePos + FVector(0, 0, 4) + Steering->DesiredVelocity, 12.f, FColor(255, 105, 180), false, -1.f, 0, 2.f); // Pink
		}

		const FMassStandingSteeringFragment* StandingSteering = Manager->GetFragmentDataPtr<FMassStandingSteeringFragment>(Entity);
		if (StandingSteering)
		{
			FVector StandTarget = StandingSteering->TargetLocation + FVector(0, 0, ZOffset);
			DrawDebugSphere(World, StandTarget, 15.f, 8, FColor::Orange, false, -1.f, 0, 1.5f);
		}
	}

	// --- Forces ---
	if (bDrawForces)
	{
		const FMassForceFragment* Force = Manager->GetFragmentDataPtr<FMassForceFragment>(Entity);
		if (Force && !Force->Value.IsNearlyZero())
		{
			DrawDebugDirectionalArrow(World, BasePos + FVector(0, 0, 6), BasePos + FVector(0, 0, 6) + Force->Value, 10.f, FColor::Red, false, -1.f, 0, 2.f);
		}
	}

	// --- Avoidance (Ghost) ---
	if (bDrawAvoidance)
	{
		const FMassGhostLocationFragment* Ghost = Manager->GetFragmentDataPtr<FMassGhostLocationFragment>(Entity);
		if (Ghost)
		{
			const FMassMoveTargetFragment* MoveTarget = Manager->GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
			if (MoveTarget && Ghost->IsValid(MoveTarget->GetCurrentActionID()))
			{
				FVector GhostPos = Ghost->Location + FVector(0, 0, ZOffset);
				DrawDebugSphere(World, GhostPos, 15.f, 8, FColor(180, 180, 180), false, -1.f, 0, 1.5f); // Light grey
				if (!Ghost->Velocity.IsNearlyZero())
				{
					DrawDebugDirectionalArrow(World, GhostPos, GhostPos + Ghost->Velocity, 10.f, FColor(180, 180, 180), false, -1.f, 0, 1.5f);
				}
			}
		}

		const FMassAvoidanceColliderFragment* Collider = Manager->GetFragmentDataPtr<FMassAvoidanceColliderFragment>(Entity);
		if (Collider)
		{
			if (Collider->Type == EMassColliderType::Circle)
			{
				float Radius = Collider->GetCircleCollider().Radius;
				DrawDebugCircle(World, BasePos + FVector(0, 0, 1), Radius, 24, FColor::Blue, false, -1.f, 0, 1.f, FVector(0, 1, 0), FVector(1, 0, 0), false);
			}
			else if (Collider->Type == EMassColliderType::Pill)
			{
				FMassPillCollider Pill = Collider->GetPillCollider();
				DrawDebugCircle(World, BasePos + EntityForward * Pill.HalfLength + FVector(0, 0, 1), Pill.Radius, 24, FColor::Blue, false, -1.f, 0, 1.f, FVector(0, 1, 0), FVector(1, 0, 0), false);
				DrawDebugCircle(World, BasePos - EntityForward * Pill.HalfLength + FVector(0, 0, 1), Pill.Radius, 24, FColor::Blue, false, -1.f, 0, 1.f, FVector(0, 1, 0), FVector(1, 0, 0), false);
			}
		}
	}

	// --- NavMesh Short Path ---
	if (bDrawShortPath)
	{
		const FMassNavMeshShortPathFragment* ShortPath = Manager->GetFragmentDataPtr<FMassNavMeshShortPathFragment>(Entity);
		if (ShortPath && ShortPath->bInitialized && ShortPath->NumPoints > 0)
		{
			FColor PathColor = ShortPath->bDone ? FColor::Green : FColor(0, 255, 100);
			FColor CorridorColor = FColor(255, 255, 0, 180); // Yellow

			// Determine corridor color based on path source
			const FMassNavMeshCachedPathFragment* CachedPath = Manager->GetFragmentDataPtr<FMassNavMeshCachedPathFragment>(Entity);
			if (CachedPath)
			{
				switch (CachedPath->PathSource)
				{
				case EMassNavigationPathSource::NavMesh: CorridorColor = FColor::Cyan; break;
				case EMassNavigationPathSource::Spline: CorridorColor = FColor::Blue; break;
				default: CorridorColor = FColor(100, 100, 100); break;
				}
			}

			// Draw path segments
			for (uint8 i = 0; i < ShortPath->NumPoints - 1; ++i)
			{
				const FMassNavMeshPathPoint& Curr = ShortPath->Points[i];
				const FMassNavMeshPathPoint& Next = ShortPath->Points[i + 1];

				FVector CurrPos = Curr.Position + FVector(0, 0, ZOffset);
				FVector NextPos = Next.Position + FVector(0, 0, ZOffset);

				// Center path line
				DrawDebugLine(World, CurrPos, NextPos, PathColor, false, -1.f, 0, 3.f);

				// Corridor edges
				DrawDebugLine(World, Curr.Left + FVector(0, 0, ZOffset), Next.Left + FVector(0, 0, ZOffset), CorridorColor, false, -1.f, 0, 1.5f);
				DrawDebugLine(World, Curr.Right + FVector(0, 0, ZOffset), Next.Right + FVector(0, 0, ZOffset), CorridorColor, false, -1.f, 0, 1.5f);
			}

			// Draw path points and tangents
			for (uint8 i = 0; i < ShortPath->NumPoints; ++i)
			{
				const FMassNavMeshPathPoint& Point = ShortPath->Points[i];
				FVector PointPos = Point.Position + FVector(0, 0, ZOffset);

				DrawDebugSphere(World, PointPos, 10.f, 6, PathColor, false, -1.f, 0, 1.5f);

				// Tangent direction
				FVector Tangent = Point.Tangent.GetVector();
				DrawDebugDirectionalArrow(World, PointPos, PointPos + Tangent * 50.f, 8.f, FColor(200, 200, 200), false, -1.f, 0, 1.f);

				// Point index label
				FString PointLabel = FString::Printf(TEXT("%d (%.0f)"), i, (float)Point.Distance.Get());
				DrawDebugString(World, PointPos + FVector(0, 0, 15.f), PointLabel, nullptr, FColor::White, -1.f, true, 0.7f);

				// Draw corridor cross-lines at each point
				DrawDebugLine(World, Point.Left + FVector(0, 0, ZOffset), Point.Right + FVector(0, 0, ZOffset), CorridorColor, false, -1.f, 0, 1.f);
			}
		}
	}
#endif
}
