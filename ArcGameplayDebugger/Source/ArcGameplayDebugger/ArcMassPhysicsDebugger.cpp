// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsDebugger.h"

#include "imgui.h"
#include "ArcMass/Physics/ArcMassPhysicsEntityLink.h"
#include "ArcMass/Physics/ArcMassPhysicsForce.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"

namespace Arcx::GameplayDebugger::MassPhysics
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

	bool GetPlayerViewPoint(FVector& OutLocation, FVector& OutDirection)
	{
		UWorld* World = GetDebugWorld();
		if (!World)
		{
			return false;
		}
		APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		if (!PC)
		{
			return false;
		}
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(OutLocation, ViewRotation);
		OutDirection = ViewRotation.Vector();
		return true;
	}
}

void FArcMassPhysicsDebugger::Initialize()
{
	CachedHitResult = FHitResult();
	DetectedEntity = FMassEntityHandle();
	DetectedBody = nullptr;
	bHasHit = false;
}

void FArcMassPhysicsDebugger::Uninitialize()
{
	DetectedEntity = FMassEntityHandle();
	DetectedBody = nullptr;
	bHasHit = false;
}

void FArcMassPhysicsDebugger::PerformLineTrace()
{
	namespace MP = Arcx::GameplayDebugger::MassPhysics;

	DetectedEntity = FMassEntityHandle();
	DetectedBody = nullptr;
	bHasHit = false;

	FVector ViewLocation;
	FVector ViewDirection;
	if (!MP::GetPlayerViewPoint(ViewLocation, ViewDirection))
	{
		return;
	}

	UWorld* World = MP::GetDebugWorld();
	if (!World)
	{
		return;
	}

	FVector TraceEnd = ViewLocation + ViewDirection * TraceDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.bTraceComplex = false;

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (PC && PC->GetPawn())
	{
		QueryParams.AddIgnoredActor(PC->GetPawn());
	}

	bHasHit = World->LineTraceSingleByChannel(
		CachedHitResult,
		ViewLocation,
		TraceEnd,
		ECC_Visibility,
		QueryParams);

	if (!bHasHit)
	{
		return;
	}

	DetectedEntity = ArcMassPhysicsEntityLink::ResolveHitFromPhysicsObject(CachedHitResult);
	DetectedBody = ArcMassPhysicsEntityLink::ResolveHitToBody(CachedHitResult);
}

void FArcMassPhysicsDebugger::Draw()
{
	PerformLineTrace();

	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Mass Physics Debugger", &bShow))
	{
		ImGui::End();
		DrawWorldVisualization();
		return;
	}

	DrawInfoPanel();
	ImGui::Separator();
	DrawForceControls();
	ImGui::Separator();
	DrawVisualizationToggles();

	ImGui::End();

	DrawWorldVisualization();
}

void FArcMassPhysicsDebugger::DrawInfoPanel()
{
	namespace MP = Arcx::GameplayDebugger::MassPhysics;

	if (!bHasHit)
	{
		ImGui::TextDisabled("No hit -- aim at a physics object");
		return;
	}

	ImGui::SeparatorText("Hit Info");
	FVector HitLoc = CachedHitResult.ImpactPoint;
	FVector HitNorm = CachedHitResult.ImpactNormal;
	ImGui::Text("Location: (%.1f, %.1f, %.1f)", HitLoc.X, HitLoc.Y, HitLoc.Z);
	ImGui::Text("Normal:   (%.2f, %.2f, %.2f)", HitNorm.X, HitNorm.Y, HitNorm.Z);

	ImGui::SeparatorText("Entity");
	if (DetectedEntity.IsValid())
	{
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Entity %d (SN:%d)",
			DetectedEntity.Index, DetectedEntity.SerialNumber);

		FMassEntityManager* Manager = MP::GetEntityManager();
		if (Manager && Manager->IsEntityActive(DetectedEntity))
		{
			const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(DetectedEntity);
			if (TransformFrag)
			{
				FVector Loc = TransformFrag->GetTransform().GetLocation();
				FRotator Rot = TransformFrag->GetTransform().Rotator();
				ImGui::Text("Pos: (%.1f, %.1f, %.1f)", Loc.X, Loc.Y, Loc.Z);
				ImGui::Text("Rot: (P=%.1f, Y=%.1f, R=%.1f)", Rot.Pitch, Rot.Yaw, Rot.Roll);
			}

			FMassEntityView EntityView(*Manager, DetectedEntity);
			bool bSimulating = EntityView.HasTag<FArcMassPhysicsSimulatingTag>();
			ImGui::Text("Simulating: %s", bSimulating ? "YES" : "no");
		}
	}
	else
	{
		ImGui::TextDisabled("No entity linked to hit body");
	}

	ImGui::SeparatorText("Physics Body");
	if (DetectedBody)
	{
		bool bAwake = DetectedBody->IsInstanceAwake();
		bool bSimPhys = DetectedBody->IsInstanceSimulatingPhysics();
		float Mass = DetectedBody->GetBodyMass();
		FVector Velocity = DetectedBody->GetUnrealWorldVelocity();

		ImGui::Text("Mass:       %.2f kg", Mass);
		ImGui::Text("Velocity:   (%.1f, %.1f, %.1f)", Velocity.X, Velocity.Y, Velocity.Z);
		ImGui::Text("Speed:      %.1f", Velocity.Size());
		ImGui::Text("Awake:      %s", bAwake ? "YES" : "no");
		ImGui::Text("Simulating: %s", bSimPhys ? "YES" : "no");
	}
	else
	{
		ImGui::TextDisabled("No body instance");
	}
}

void FArcMassPhysicsDebugger::DrawForceControls()
{
	namespace MP = Arcx::GameplayDebugger::MassPhysics;

	ImGui::SeparatorText("Force Push");
	ImGui::SliderFloat("Trace Distance", &TraceDistance, 1000.f, 50000.f, "%.0f");
	ImGui::SliderFloat("Impulse Magnitude", &ImpulseMagnitude, 1000.f, 100000.f, "%.0f");
	ImGui::SliderFloat("Force Magnitude", &ForceMagnitude, 1000.f, 100000.f, "%.0f");

	bool bCanApply = DetectedEntity.IsValid() && DetectedBody != nullptr;
	if (!bCanApply)
	{
		ImGui::BeginDisabled();
	}

	FVector ViewLocation;
	FVector ViewDirection;
	MP::GetPlayerViewPoint(ViewLocation, ViewDirection);

	if (ImGui::Button("Apply Impulse"))
	{
		FMassEntityManager* Manager = MP::GetEntityManager();
		if (Manager && bCanApply)
		{
			FVector Direction = (CachedHitResult.ImpactPoint - ViewLocation).GetSafeNormal();
			FVector Impulse = Direction * ImpulseMagnitude;
			ArcMassPhysicsForce::ApplyImpulse(*Manager, DetectedEntity, *DetectedBody, Impulse);
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Add Force"))
	{
		FMassEntityManager* Manager = MP::GetEntityManager();
		if (Manager && bCanApply)
		{
			FVector Direction = (CachedHitResult.ImpactPoint - ViewLocation).GetSafeNormal();
			FVector Force = Direction * ForceMagnitude;
			ArcMassPhysicsForce::AddForce(*Manager, DetectedEntity, *DetectedBody, Force);
		}
	}

	if (!bCanApply)
	{
		ImGui::EndDisabled();
	}
}

void FArcMassPhysicsDebugger::DrawVisualizationToggles()
{
	ImGui::SeparatorText("Visualization");
	ImGui::Checkbox("Draw Hit Line", &bDrawHitLine);
	ImGui::Checkbox("Draw Bounds", &bDrawBounds);
	ImGui::Checkbox("Draw Collision Shapes", &bDrawCollisionShapes);
}

void FArcMassPhysicsDebugger::DrawWorldVisualization()
{
	namespace MP = Arcx::GameplayDebugger::MassPhysics;

	UWorld* World = MP::GetDebugWorld();
	if (!World)
	{
		return;
	}

	if (!bHasHit)
	{
		return;
	}

	FVector HitPoint = CachedHitResult.ImpactPoint;

	if (bDrawHitLine)
	{
		FVector ViewLocation;
		FVector ViewDirection;
		if (MP::GetPlayerViewPoint(ViewLocation, ViewDirection))
		{
			DrawDebugLine(World, ViewLocation, HitPoint, FColor::Yellow, false, -1.f, 0, 1.f);
		}

		FColor SphereColor = DetectedEntity.IsValid() ? FColor::Green : FColor::Red;
		DrawDebugSphere(World, HitPoint, 15.f, 8, SphereColor, false, -1.f, 0, 1.5f);
	}

	if (DetectedEntity.IsValid())
	{
		FMassEntityManager* Manager = MP::GetEntityManager();
		if (Manager && Manager->IsEntityActive(DetectedEntity))
		{
			const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(DetectedEntity);
			if (TransformFrag)
			{
				FVector EntityLoc = TransformFrag->GetTransform().GetLocation();

				if (bDrawBounds)
				{
					DrawDebugBox(World, EntityLoc, FVector(50.f), FColor::Cyan, false, -1.f, 0, 2.f);
				}
			}
		}
	}

	if (bDrawCollisionShapes && DetectedBody)
	{
		FTransform BodyTransform = DetectedBody->GetUnrealWorldTransform();
		DrawCollisionShapes(World, BodyTransform);
	}
}

void FArcMassPhysicsDebugger::DrawCollisionShapes(UWorld* World, const FTransform& BodyTransform)
{
	UBodySetup* BodySetup = DetectedBody->GetBodySetup();
	if (!BodySetup)
	{
		return;
	}

	const FKAggregateGeom& AggGeom = BodySetup->AggGeom;
	const FColor ShapeColor = FColor::Magenta;

	for (const FKSphereElem& Sphere : AggGeom.SphereElems)
	{
		FTransform ShapeTransform = Sphere.GetTransform() * BodyTransform;
		FVector Center = ShapeTransform.GetLocation();
		DrawDebugSphere(World, Center, Sphere.Radius, 16, ShapeColor, false, -1.f, 0, 1.f);
	}

	for (const FKBoxElem& Box : AggGeom.BoxElems)
	{
		FTransform ShapeTransform = Box.GetTransform() * BodyTransform;
		FVector Center = ShapeTransform.GetLocation();
		FQuat Rot = ShapeTransform.GetRotation();
		FVector Extent(Box.X, Box.Y, Box.Z);
		DrawDebugBox(World, Center, Extent, Rot, ShapeColor, false, -1.f, 0, 1.f);
	}

	for (const FKSphylElem& Capsule : AggGeom.SphylElems)
	{
		FTransform ShapeTransform = Capsule.GetTransform() * BodyTransform;
		FVector Center = ShapeTransform.GetLocation();
		FQuat Rot = ShapeTransform.GetRotation();
		DrawDebugCapsule(World, Center, Capsule.Length * 0.5f + Capsule.Radius, Capsule.Radius, Rot, ShapeColor, false, -1.f, 0, 1.f);
	}

	for (const FKConvexElem& Convex : AggGeom.ConvexElems)
	{
		const TArray<FVector>& Vertices = Convex.VertexData;
		if (Vertices.Num() < 2)
		{
			continue;
		}

		// Draw convex hull edges as wireframe approximation
		for (int32 i = 0; i < Vertices.Num(); ++i)
		{
			FVector V0 = BodyTransform.TransformPosition(Vertices[i]);
			FVector V1 = BodyTransform.TransformPosition(Vertices[(i + 1) % Vertices.Num()]);
			DrawDebugLine(World, V0, V1, ShapeColor, false, -1.f, 0, 1.f);
		}
	}
}
