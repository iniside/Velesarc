// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWEditorCommands.h"

#include "ArcInstancedWorld/Components/ArcIWMassConfigComponent.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Misc/ArchiveMD5.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Animation/TransformProviderData.h"

#define LOCTEXT_NAMESPACE "ArcIWEditorCommands"

namespace UE::ArcIW::Editor
{

void CaptureActorMeshData(AActor* Actor, TArray<FArcIWMeshEntry>& OutEntries)
{
	if (!IsValid(Actor))
	{
		return;
	}

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);

	const FTransform ActorTransformInverse = Actor->GetActorTransform().Inverse();

	for (UStaticMeshComponent* SMC : StaticMeshComponents)
	{
		if (!IsValid(SMC) || !SMC->GetStaticMesh())
		{
			continue;
		}

		UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(SMC);
		if (ISMC)
		{
			// For ISMCs: capture each instance separately
			const int32 InstanceCount = ISMC->GetInstanceCount();
			for (int32 InstanceIdx = 0; InstanceIdx < InstanceCount; ++InstanceIdx)
			{
				FTransform InstanceWorldTransform;
				ISMC->GetInstanceTransform(InstanceIdx, InstanceWorldTransform, /*bWorldSpace=*/true);

				FArcIWMeshEntry Entry;
				Entry.Mesh = ISMC->GetStaticMesh();
				Entry.RelativeTransform = InstanceWorldTransform * ActorTransformInverse;
				Entry.bCastShadows = ISMC->CastShadow;

				const int32 NumMaterials = ISMC->GetNumMaterials();
				Entry.Materials.Reserve(NumMaterials);
				for (int32 MatIdx = 0; MatIdx < NumMaterials; ++MatIdx)
				{
					Entry.Materials.Add(ISMC->GetMaterial(MatIdx));
				}

				OutEntries.Add(MoveTemp(Entry));
			}
		}
		else
		{
			// Regular static mesh component
			FArcIWMeshEntry Entry;
			Entry.Mesh = SMC->GetStaticMesh();
			Entry.RelativeTransform = SMC->GetComponentTransform() * ActorTransformInverse;
			Entry.bCastShadows = SMC->CastShadow;

			const int32 NumMaterials = SMC->GetNumMaterials();
			Entry.Materials.Reserve(NumMaterials);
			for (int32 MatIdx = 0; MatIdx < NumMaterials; ++MatIdx)
			{
				Entry.Materials.Add(SMC->GetMaterial(MatIdx));
			}

			OutEntries.Add(MoveTemp(Entry));
		}
	}

	// Capture skinned mesh components (skeletal meshes used for instanced skinned rendering).
	// Skip any that are also static mesh components (already handled above).
	TArray<USkinnedMeshComponent*> SkinnedMeshComponents;
	Actor->GetComponents<USkinnedMeshComponent>(SkinnedMeshComponents);

	for (USkinnedMeshComponent* SkinnedComp : SkinnedMeshComponents)
	{
		if (!IsValid(SkinnedComp))
		{
			continue;
		}

		USkinnedAsset* Asset = SkinnedComp->GetSkinnedAsset();
		if (!Asset)
		{
			continue;
		}

		FArcIWMeshEntry Entry;
		Entry.SkinnedAsset = Asset;
		Entry.RelativeTransform = SkinnedComp->GetComponentTransform() * ActorTransformInverse;
		Entry.bCastShadows = SkinnedComp->CastShadow;

		// Capture TransformProvider from InstancedSkinnedMeshComponent if available
		UInstancedSkinnedMeshComponent* ISMSkinned = Cast<UInstancedSkinnedMeshComponent>(SkinnedComp);
		if (ISMSkinned)
		{
			Entry.TransformProvider = ISMSkinned->GetTransformProvider();
		}

		const int32 NumMaterials = SkinnedComp->GetNumMaterials();
		Entry.Materials.Reserve(NumMaterials);
		for (int32 MatIdx = 0; MatIdx < NumMaterials; ++MatIdx)
		{
			Entry.Materials.Add(SkinnedComp->GetMaterial(MatIdx));
		}

		OutEntries.Add(MoveTemp(Entry));
	}
}

void CaptureActorCollisionData(
	TSubclassOf<AActor> ActorClass,
	UObject* Outer,
	UBodySetup*& OutBodySetup,
	FCollisionProfileName& OutCollisionProfile,
	FBodyInstance& OutBodyTemplate)
{
	OutBodySetup = nullptr;

	if (!ActorClass)
	{
		return;
	}

	AActor* CDO = ActorClass->GetDefaultObject<AActor>();
	if (!CDO)
	{
		return;
	}

	USceneComponent* RootComponent = CDO->GetRootComponent();

	// Collect all collision sources with their transforms relative to the actor root.
	struct FCollisionSource
	{
		UPrimitiveComponent* Primitive;
		FTransform RelativeTransform;
	};

	TArray<FCollisionSource> CollisionSources;
	bool bProfileTaken = false;

	// Check root component first.
	// Use BodyInstance.GetCollisionEnabled(false) to check the component's own collision setting,
	// bypassing Actor::GetActorEnableCollision() which may disable collision at the actor level
	// even though the component itself has valid collision shapes we want to capture.
	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(RootComponent);
	if (RootPrimitive && RootPrimitive->BodyInstance.GetCollisionEnabled(/*bCheckOwner=*/false) != ECollisionEnabled::NoCollision)
	{
		FCollisionSource Source;
		Source.Primitive = RootPrimitive;
		Source.RelativeTransform = FTransform::Identity;
		CollisionSources.Add(Source);

		OutCollisionProfile = RootPrimitive->GetCollisionProfileName();
		OutBodyTemplate = RootPrimitive->BodyInstance;
		OutBodyTemplate.OwnerComponent = nullptr;
		OutBodyTemplate.BodySetup = nullptr;
		bProfileTaken = true;
	}

	// Walk all children recursively
	if (RootComponent)
	{
		TArray<USceneComponent*> AllChildren;
		RootComponent->GetChildrenComponents(/*bIncludeAllDescendants=*/true, AllChildren);

		for (USceneComponent* Child : AllChildren)
		{
			UPrimitiveComponent* ChildPrimitive = Cast<UPrimitiveComponent>(Child);
			if (!ChildPrimitive || ChildPrimitive->BodyInstance.GetCollisionEnabled(/*bCheckOwner=*/false) == ECollisionEnabled::NoCollision)
			{
				continue;
			}

			// Compute relative transform to root by walking up the attach chain
			FTransform RelativeToRoot = FTransform::Identity;
			USceneComponent* Current = ChildPrimitive;
			while (Current && Current != RootComponent)
			{
				RelativeToRoot = RelativeToRoot * Current->GetRelativeTransform();
				Current = Current->GetAttachParent();
			}

			FCollisionSource Source;
			Source.Primitive = ChildPrimitive;
			Source.RelativeTransform = RelativeToRoot;
			CollisionSources.Add(Source);

			// Use first collision-enabled child's profile as fallback if root was not primitive
			if (!bProfileTaken)
			{
				OutCollisionProfile = ChildPrimitive->GetCollisionProfileName();
				bProfileTaken = true;
			}
		}
	}

	if (CollisionSources.IsEmpty())
	{
		return;
	}

	UBodySetup* MergedSetup = NewObject<UBodySetup>(Outer, NAME_None, RF_Transactional);
	MergedSetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;

	for (const FCollisionSource& Source : CollisionSources)
	{
		UBodySetup* SourceSetup = Source.Primitive->GetBodySetup();
		if (!SourceSetup)
		{
			continue;
		}

		const FKAggregateGeom& SrcAgg = SourceSetup->AggGeom;
		FKAggregateGeom& DstAgg = MergedSetup->AggGeom;

		const FTransform& T = Source.RelativeTransform;

		for (const FKBoxElem& Box : SrcAgg.BoxElems)
		{
			FKBoxElem NewBox = Box;
			NewBox.SetTransform(NewBox.GetTransform() * T);
			DstAgg.BoxElems.Add(NewBox);
		}

		for (const FKSphereElem& Sphere : SrcAgg.SphereElems)
		{
			FKSphereElem NewSphere = Sphere;
			FTransform SphereTransform(FRotator::ZeroRotator, FVector(NewSphere.Center));
			FTransform TransformedSphere = SphereTransform * T;
			NewSphere.Center = TransformedSphere.GetLocation();
			DstAgg.SphereElems.Add(NewSphere);
		}

		for (const FKSphylElem& Sphyl : SrcAgg.SphylElems)
		{
			FKSphylElem NewSphyl = Sphyl;
			NewSphyl.SetTransform(NewSphyl.GetTransform() * T);
			DstAgg.SphylElems.Add(NewSphyl);
		}

		for (const FKConvexElem& Convex : SrcAgg.ConvexElems)
		{
			FKConvexElem NewConvex = Convex;
			for (FVector& Vertex : NewConvex.VertexData)
			{
				Vertex = T.TransformPosition(Vertex);
			}
			NewConvex.UpdateElemBox();
			DstAgg.ConvexElems.Add(NewConvex);
		}
	}

	MergedSetup->CreatePhysicsMeshes();
	OutBodySetup = MergedSetup;
}

void CaptureActorCollisionData(
	AActor* Actor,
	UObject* Outer,
	UBodySetup*& OutBodySetup,
	FCollisionProfileName& OutCollisionProfile,
	FBodyInstance& OutBodyTemplate)
{
	OutBodySetup = nullptr;

	if (!IsValid(Actor))
	{
		return;
	}

	USceneComponent* RootComponent = Actor->GetRootComponent();

	struct FCollisionSource
	{
		UPrimitiveComponent* Primitive;
		FTransform RelativeTransform;
	};

	TArray<FCollisionSource> CollisionSources;
	bool bProfileTaken = false;

	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(RootComponent);
	if (RootPrimitive && RootPrimitive->BodyInstance.GetCollisionEnabled(/*bCheckOwner=*/false) != ECollisionEnabled::NoCollision)
	{
		FCollisionSource Source;
		Source.Primitive = RootPrimitive;
		Source.RelativeTransform = FTransform::Identity;
		CollisionSources.Add(Source);

		OutCollisionProfile = RootPrimitive->GetCollisionProfileName();
		OutBodyTemplate = RootPrimitive->BodyInstance;
		OutBodyTemplate.OwnerComponent = nullptr;
		OutBodyTemplate.BodySetup = nullptr;
		bProfileTaken = true;
	}

	if (RootComponent)
	{
		TArray<USceneComponent*> AllChildren;
		RootComponent->GetChildrenComponents(/*bIncludeAllDescendants=*/true, AllChildren);

		for (USceneComponent* Child : AllChildren)
		{
			UPrimitiveComponent* ChildPrimitive = Cast<UPrimitiveComponent>(Child);
			if (!ChildPrimitive || ChildPrimitive->BodyInstance.GetCollisionEnabled(/*bCheckOwner=*/false) == ECollisionEnabled::NoCollision)
			{
				continue;
			}

			FTransform RelativeToRoot = FTransform::Identity;
			USceneComponent* Current = ChildPrimitive;
			while (Current && Current != RootComponent)
			{
				RelativeToRoot = RelativeToRoot * Current->GetRelativeTransform();
				Current = Current->GetAttachParent();
			}

			FCollisionSource Source;
			Source.Primitive = ChildPrimitive;
			Source.RelativeTransform = RelativeToRoot;
			CollisionSources.Add(Source);

			if (!bProfileTaken)
			{
				OutCollisionProfile = ChildPrimitive->GetCollisionProfileName();
				bProfileTaken = true;
			}
		}
	}

	if (CollisionSources.IsEmpty())
	{
		return;
	}

	UBodySetup* MergedSetup = NewObject<UBodySetup>(Outer, NAME_None, RF_Transactional);
	MergedSetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;

	for (const FCollisionSource& Source : CollisionSources)
	{
		UBodySetup* SourceSetup = Source.Primitive->GetBodySetup();
		if (!SourceSetup)
		{
			continue;
		}

		const FKAggregateGeom& SrcAgg = SourceSetup->AggGeom;
		FKAggregateGeom& DstAgg = MergedSetup->AggGeom;

		const FTransform& T = Source.RelativeTransform;

		for (const FKBoxElem& Box : SrcAgg.BoxElems)
		{
			FKBoxElem NewBox = Box;
			NewBox.SetTransform(NewBox.GetTransform() * T);
			DstAgg.BoxElems.Add(NewBox);
		}

		for (const FKSphereElem& Sphere : SrcAgg.SphereElems)
		{
			FKSphereElem NewSphere = Sphere;
			FTransform SphereTransform(FRotator::ZeroRotator, FVector(NewSphere.Center));
			FTransform TransformedSphere = SphereTransform * T;
			NewSphere.Center = TransformedSphere.GetLocation();
			DstAgg.SphereElems.Add(NewSphere);
		}

		for (const FKSphylElem& Sphyl : SrcAgg.SphylElems)
		{
			FKSphylElem NewSphyl = Sphyl;
			NewSphyl.SetTransform(NewSphyl.GetTransform() * T);
			DstAgg.SphylElems.Add(NewSphyl);
		}

		for (const FKConvexElem& Convex : SrcAgg.ConvexElems)
		{
			FKConvexElem NewConvex = Convex;
			for (FVector& Vertex : NewConvex.VertexData)
			{
				Vertex = T.TransformPosition(Vertex);
			}
			NewConvex.UpdateElemBox();
			DstAgg.ConvexElems.Add(NewConvex);
		}
	}

	MergedSetup->CreatePhysicsMeshes();
	OutBodySetup = MergedSetup;
}

AArcIWMassISMPartitionActor* FindOrCreatePartitionActor(
	UWorld* World,
	const FVector& WorldLocation,
	FName GridName)
{
	if (!World)
	{
		return nullptr;
	}

	UActorPartitionSubsystem* PartitionSubsystem = World->GetSubsystem<UActorPartitionSubsystem>();
	if (!PartitionSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIW: UActorPartitionSubsystem not available."));
		return nullptr;
	}

	const bool bIsPartitionedWorld = World->IsPartitionedWorld();
	UClass* PartitionClass = AArcIWMassISMPartitionActor::StaticClass();
	const uint32 GridSize = AArcIWMassISMPartitionActor::GetGridCellSize(World, GridName);

	FArchiveMD5 ArMD5;
	ArMD5 << GridName;
	const FGuid ManagerGuid = bIsPartitionedWorld ? ArMD5.GetGuidFromHash() : FGuid();

	UActorPartitionSubsystem::FCellCoord CellCoord = UActorPartitionSubsystem::FCellCoord::GetCellCoord(
		WorldLocation,
		World->PersistentLevel,
		GridSize);

	FVector CellCenter = FVector::ZeroVector;
	if (bIsPartitionedWorld)
	{
		FBox CellBounds = UActorPartitionSubsystem::FCellCoord::GetCellBounds(CellCoord, GridSize);
		CellCenter = CellBounds.GetCenter();
	}

	APartitionActor* RawPartitionActor = PartitionSubsystem->GetActor(
		PartitionClass,
		CellCoord,
		/*bInCreate=*/true,
		/*InGuid=*/ManagerGuid,
		/*InGridSize=*/GridSize,
		/*bInBoundsSearch=*/true,
		/*InActorCreated=*/[bIsPartitionedWorld, &CellCenter, GridName, &ManagerGuid](APartitionActor* NewPartitionActor)
		{
			AArcIWMassISMPartitionActor* MassISMActor = CastChecked<AArcIWMassISMPartitionActor>(NewPartitionActor);
			MassISMActor->SetGridName(GridName);
			if (bIsPartitionedWorld)
			{
				NewPartitionActor->SetRuntimeGrid(GridName);
				MassISMActor->SetGridGuid(ManagerGuid);
			}
			NewPartitionActor->SetActorLocation(CellCenter);
		});

	return Cast<AArcIWMassISMPartitionActor>(RawPartitionActor);
}

void ConvertActorsToPartition(UWorld* World, const TArray<AActor*>& Actors)
{
	if (!World || Actors.IsEmpty())
	{
		return;
	}

	TSet<APartitionActor*> ModifiedPartitionActors;

	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		// Capture mesh data
		TArray<FArcIWMeshEntry> MeshEntries;
		CaptureActorMeshData(Actor, MeshEntries);

		// Apply settings default TransformProvider to skinned entries without one
		UTransformProviderData* DefaultProvider =
			UArcIWSettings::Get()->DefaultSkinnedMeshTransformProvider.LoadSynchronous();
		if (DefaultProvider)
		{
			for (FArcIWMeshEntry& MeshEntry : MeshEntries)
			{
				if (MeshEntry.IsSkinned() && !MeshEntry.TransformProvider)
				{
					MeshEntry.TransformProvider = DefaultProvider;
				}
			}
		}

		// Read config from component, fall back to project settings
		UMassEntityConfigAsset* AdditionalConfig = nullptr;
		FName RuntimeGridName = UArcIWSettings::Get()->DefaultGridName;
		int32 RespawnTimeSeconds = 0;
		EArcMassPhysicsBodyType PhysicsBodyType = EArcMassPhysicsBodyType::Static;
		UArcIWMassConfigComponent* ConfigComp = Actor->FindComponentByClass<UArcIWMassConfigComponent>();
		if (ConfigComp)
		{
			AdditionalConfig = ConfigComp->EntityConfig;
			RuntimeGridName = ConfigComp->RuntimeGridName;
			RespawnTimeSeconds = ConfigComp->RespawnTimeSeconds;
			PhysicsBodyType = ConfigComp->PhysicsBodyType;
		}

		// Get or create the partition actor for this cell
		AArcIWMassISMPartitionActor* MassISMActor = FindOrCreatePartitionActor(World, Actor->GetActorLocation(), RuntimeGridName);
		if (!MassISMActor)
		{
			continue;
		}
		UBodySetup* CollisionBodySetup = nullptr;
		FCollisionProfileName CollisionProfile;
		FBodyInstance BodyTemplate;
		CaptureActorCollisionData(Actor, MassISMActor, CollisionBodySetup, CollisionProfile, BodyTemplate);
		MassISMActor->AddActorInstance(
			Actor->GetClass(),
			Actor->GetActorTransform(),
			MeshEntries,
			AdditionalConfig,
			CollisionBodySetup,
			CollisionProfile,
			BodyTemplate,
			RespawnTimeSeconds,
			PhysicsBodyType);

		ModifiedPartitionActors.Add(MassISMActor);

		// Destroy the original actor
		Actor->Destroy();
	}

	// Build editor preview and mark dirty
	for (APartitionActor* RawActor : ModifiedPartitionActors)
	{
		AArcIWMassISMPartitionActor* MassISMActor = CastChecked<AArcIWMassISMPartitionActor>(RawActor);
		MassISMActor->RebuildEditorPreviewISMCs();
		RawActor->Modify();
	}
}

void ConvertPartitionToActors(UWorld* World, AArcIWMassISMPartitionActor* PartitionActor)
{
	if (!World || !IsValid(PartitionActor))
	{
		return;
	}

	const TArray<FArcIWActorClassData>& ClassEntries = PartitionActor->GetActorClassEntries();

	for (const FArcIWActorClassData& ClassData : ClassEntries)
	{
		if (!ClassData.ActorClass)
		{
			continue;
		}

		for (const FTransform& Transform : ClassData.InstanceTransforms)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AActor* SpawnedActor = World->SpawnActor(ClassData.ActorClass, &Transform, SpawnParams);
			if (SpawnedActor)
			{
				SpawnedActor->Modify();
			}
		}
	}

	PartitionActor->Destroy();
}

} // namespace UE::ArcIW::Editor

// ---------------------------------------------------------------------------
// Blueprint Function Library
// ---------------------------------------------------------------------------

void UArcIWEditorCommandLibrary::ConvertActorsToPartition(UObject* WorldContextObject, const TArray<AActor*>& Actors)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	UE::ArcIW::Editor::ConvertActorsToPartition(World, Actors);
}

void UArcIWEditorCommandLibrary::ConvertPartitionToActors(UObject* WorldContextObject, APartitionActor* PartitionActor)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	AArcIWMassISMPartitionActor* MassISMActor = Cast<AArcIWMassISMPartitionActor>(PartitionActor);
	if (MassISMActor)
	{
		UE::ArcIW::Editor::ConvertPartitionToActors(World, MassISMActor);
	}
}

#undef LOCTEXT_NAMESPACE
