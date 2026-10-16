// Copyright Lukasz Baran. All Rights Reserved.

#include "Collision/ArcTerrainCollisionComponent.h"
#include "ArcTerrainDiagnostics.h"
#include "AI/NavigationSystemBase.h"
#include "AI/NavigationSystemHelpers.h"
#include "Chaos/ImplicitObjectTransformed.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/ShapeInstance.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/PhysicsFiltering.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceScene.h"
#include "Physics/Experimental/PhysicsUserData_Chaos.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PhysicsPublic.h"

namespace ArcTerrainCollisionComponentTiming_d182
{
	double Begin()
	{
		return ArcTerrainDiag::IsTiming() ? FPlatformTime::Seconds() : 0.0;
	}

	void End(ArcTerrainDiag::ELeafTiming Leaf, double StartSeconds)
	{
		if (StartSeconds != 0.0)
		{
			ArcTerrainDiag::Frame().Tick.LeafSeconds[static_cast<int32>(Leaf)] +=
				FPlatformTime::Seconds() - StartSeconds;
		}
	}
}

UArcTerrainCollisionComponent::UArcTerrainCollisionComponent()
{
	// Landscape collision component defaults (LandscapeCollision.cpp constructor), minus the
	// rendering/net bits we don't need. Mobility stays Movable — see class comment.
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetGenerateOverlapEvents(false);
	CastShadow = false;
	bCanEverAffectNavigation = true;
	// EvenIfNotCollidable: the navmesh export must run off our custom heightfield geometry, never
	// off the (absent) body setup.
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::EvenIfNotCollidable;
	LocalBounds.Init();
}

void UArcTerrainCollisionComponent::SetHeightfield(TRefCountPtr<FArcTerrainHeightfieldRef> InHeightfieldRef)
{
	// Double-buffer: hold the outgoing ref across the physics recreate. (The outgoing physics actor
	// also holds its own Chaos ref to the geometry, but keeping the wrapper alive makes the lifetime
	// explicit and independent of physics-interface internals.)
	TRefCountPtr<FArcTerrainHeightfieldRef> PreviousRef = HeightfieldRef;
	HeightfieldRef = MoveTemp(InHeightfieldRef);

	// Cache local bounds from the BASE-scale geometry bounds (SetHeightfield always runs before the
	// physics state bakes the world scale into the geometry's internal scale).
	LocalBounds.Init();
	if (HeightfieldRef.IsValid() && HeightfieldRef->Heightfield.IsValid())
	{
		const Chaos::FAABB3 GeomBounds = HeightfieldRef->Heightfield->BoundingBox();
		LocalBounds = FBox(FVector(GeomBounds.Min()), FVector(GeomBounds.Max()));
	}

	const double PhysicsStartSeconds = ArcTerrainCollisionComponentTiming_d182::Begin();
	RecreatePhysicsState();
	ArcTerrainCollisionComponentTiming_d182::End(ArcTerrainDiag::ELeafTiming::SetHeightfieldPhysics,
		PhysicsStartSeconds);
	const double BoundsStartSeconds = ArcTerrainCollisionComponentTiming_d182::Begin();
	UpdateBounds();
	ArcTerrainCollisionComponentTiming_d182::End(ArcTerrainDiag::ELeafTiming::SetHeightfieldBounds,
		BoundsStartSeconds);
	const double NavigationStartSeconds = ArcTerrainCollisionComponentTiming_d182::Begin();
	FNavigationSystem::UpdateComponentData(*this);
	ArcTerrainCollisionComponentTiming_d182::End(ArcTerrainDiag::ELeafTiming::SetHeightfieldNavigation,
		NavigationStartSeconds);

	// PreviousRef released here — after the new physics state exists.
}

void UArcTerrainCollisionComponent::OnCreatePhysicsState()
{
	// Route OnCreatePhysicsState, skip the UPrimitiveComponent implementation: there is no
	// UBodySetup path for heightfields (Landscape bypass, LandscapeCollision.cpp:357).
	USceneComponent::OnCreatePhysicsState();

	if (BodyInstance.IsValidBodyInstance()
		|| !HeightfieldRef.IsValid() || !HeightfieldRef->Heightfield.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	FPhysScene* PhysScene = World ? World->GetPhysicsScene() : nullptr;
	if (PhysScene == nullptr)
	{
		return;
	}

	const FTransform ComponentTransform = GetComponentToWorld();
	const FVector WorldScale = ComponentTransform.GetScale3D();

	// Chaos actor transforms are rigid: bake sample spacing + component world scale into the
	// geometry scale (Landscape's FinalScale). SetScale is absolute, so repeated recreates are
	// idempotent; the geometry is per-cell single-owner and has no live physics state right now.
	const FVector BaseScale = HeightfieldRef->BaseScale;
	HeightfieldRef->Heightfield->SetScale(Chaos::FVec3(
		BaseScale.X * WorldScale.X,
		BaseScale.Y * WorldScale.Y,
		BaseScale.Z * WorldScale.Z));

	FActorCreationParams Params;
	Params.InitialTM = ComponentTransform;
	Params.InitialTM.SetScale3D(FVector(0));
	Params.bQueryOnly = false;
	Params.bStatic = true;
	Params.Scene = PhysScene;

	FPhysicsActorHandle PhysHandle;
	FPhysicsInterface::CreateActor(Params, PhysHandle);
	Chaos::FRigidBodyHandle_External& Body_External = PhysHandle->GetGameThreadAPI();

	Chaos::FImplicitObjectPtr ImplicitHeightField(HeightfieldRef->Heightfield);
	Chaos::FImplicitObjectPtr TransformedHeightField =
		MakeImplicitObjectPtr<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>>(
			ImplicitHeightField, Chaos::FRigidTransform3(FTransform::Identity));

	Chaos::FShapesArray ShapeArray;
	TUniquePtr<Chaos::FPerShapeData> NewShape =
		Chaos::FShapeInstanceProxy::Make(ShapeArray.Num(), TransformedHeightField);

	// Filter data from the component's collision setup (BlockAll profile by default). One shape
	// serves simple AND complex collision (no separate simple geometry).
	FPhysicsFilterBuilder Builder;
	Builder.SetOwnerID(GetOwner() ? GetOwner()->GetUniqueID() : 0);
	Builder.SetComponentID(GetUniqueID());
	Builder.SetCollisionChannelIndex(GetCollisionObjectType());
	Builder.SetResponses(GetCollisionResponseToChannels());
	Builder.SetFlags(Chaos::EFilterFlags::SimpleCollision, true);
	Builder.SetFlags(Chaos::EFilterFlags::ComplexCollision, true);
	Builder.SetFlags(Chaos::EFilterFlags::CCD, true);
	Builder.SetFlags(Chaos::EFilterFlags::StaticShape, true);
	NewShape->SetFilterInstanceData(Builder.BuildInstanceData());
	NewShape->SetShapeFilterData(Builder.BuildShapeFilterData());

	TArray<Chaos::FMaterialHandle> Materials;
	if (GEngine != nullptr && GEngine->DefaultPhysMaterial != nullptr)
	{
		Materials.Add(GEngine->DefaultPhysMaterial->GetPhysicsMaterial());
	}
	NewShape->SetMaterials(Materials);

	Body_External.SetGeometry(TransformedHeightField);
	ShapeArray.Emplace(MoveTemp(NewShape));

	const Chaos::FRigidTransform3 WorldPose(Body_External.X(), Body_External.R());
	for (TUniquePtr<Chaos::FPerShapeData>& Shape : ShapeArray)
	{
		Shape->UpdateShapeBounds(WorldPose);
	}
	Body_External.MergeShapesArray(MoveTemp(ShapeArray));

	BodyInstance.PhysicsUserData = FPhysicsUserData(&BodyInstance);
	BodyInstance.OwnerComponent = this;
	BodyInstance.SetPhysicsActor(PhysHandle);
	Body_External.SetUserData(&BodyInstance.PhysicsUserData);

	FPhysicsCommand::ExecuteWrite(PhysScene, [&PhysHandle, PhysScene]()
	{
		TArray<FPhysicsActorHandle> Actors = { PhysHandle };
		const bool bImmediateAccelStructureInsertion = true;
		PhysScene->AddActorsToScene_AssumesLocked(Actors, bImmediateAccelStructureInsertion);
	});

	PhysScene->AddToComponentMaps(this, PhysHandle);
}

void UArcTerrainCollisionComponent::OnDestroyPhysicsState()
{
	// Landscape ordering: Super (UPrimitiveComponent -> BodyInstance.TermBody destroys the Chaos
	// actor we hand-created) first, then unmap.
	Super::OnDestroyPhysicsState();

	UWorld* World = GetWorld();
	if (FPhysScene* PhysScene = World ? World->GetPhysicsScene() : nullptr)
	{
		FPhysicsActorHandle ActorHandle = BodyInstance.GetPhysicsActor();
		if (FPhysicsInterface::IsValid(ActorHandle))
		{
			PhysScene->RemoveFromComponentMaps(ActorHandle);
		}
	}
}

bool UArcTerrainCollisionComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	check(IsInGameThread());
	if (HeightfieldRef.IsValid() && HeightfieldRef->Heightfield.IsValid())
	{
		// The exporter reads RAW sample coordinates (X, Y, Height) and ignores the geometry's
		// internal scale — the sample->local scale must ride on the transform (Landscape pattern,
		// LandscapeCollision.cpp:1890-1907).
		FTransform HFToW = GetComponentTransform();
		HFToW.MultiplyScale3D(HeightfieldRef->BaseScale);
		GeomExport.ExportChaosHeightField(HeightfieldRef->Heightfield.GetReference(), HFToW);
	}
	return false;
}

FBoxSphereBounds UArcTerrainCollisionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	const FBox Box = LocalBounds.IsValid
		? LocalBounds
		: FBox(FVector::ZeroVector, FVector::ZeroVector);
	return FBoxSphereBounds(Box.TransformBy(LocalToWorld));
}
