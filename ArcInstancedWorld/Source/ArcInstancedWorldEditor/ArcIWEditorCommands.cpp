// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWEditorCommands.h"

#include "ArcInstancedWorld/ArcIWMassConfigComponent.h"
#include "ArcInstancedWorld/ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Misc/ArchiveMD5.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

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
}

void ConvertActorsToPartition(UWorld* World, const TArray<AActor*>& Actors)
{
	if (!World || Actors.IsEmpty())
	{
		return;
	}

	UActorPartitionSubsystem* PartitionSubsystem = World->GetSubsystem<UActorPartitionSubsystem>();
	if (!PartitionSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIW: UActorPartitionSubsystem not available. Cannot convert actors to partition."));
		return;
	}

	const bool bIsPartitionedWorld = World->IsPartitionedWorld();

	UClass* PartitionClass = AArcIWMassISMPartitionActor::StaticClass();

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

		// Read config from component, fall back to project settings
		UMassEntityConfigAsset* AdditionalConfig = nullptr;
		FName RuntimeGridName = UArcIWSettings::Get()->DefaultGridName;
		UArcIWMassConfigComponent* ConfigComp = Actor->FindComponentByClass<UArcIWMassConfigComponent>();
		if (ConfigComp)
		{
			AdditionalConfig = ConfigComp->EntityConfig;
			RuntimeGridName = ConfigComp->RuntimeGridName;
		}

		// Query cell size using the CDO
		const uint32 GridSize = AArcIWMassISMPartitionActor::GetGridCellSize(World, RuntimeGridName);

		// Generate a stable GUID from the grid name
		FArchiveMD5 ArMD5;
		ArMD5 << RuntimeGridName;
		const FGuid ManagerGuid = bIsPartitionedWorld ? ArMD5.GetGuidFromHash() : FGuid();

		// Compute cell coordinate
		UActorPartitionSubsystem::FCellCoord CellCoord = UActorPartitionSubsystem::FCellCoord::GetCellCoord(
			Actor->GetActorLocation(),
			Actor->GetLevel(),
			GridSize);

		// Compute cell center for actor placement
		FVector CellCenter = FVector::ZeroVector;
		if (bIsPartitionedWorld)
		{
			FBox CellBounds = UActorPartitionSubsystem::FCellCoord::GetCellBounds(CellCoord, GridSize);
			CellCenter = CellBounds.GetCenter();
		}

		// Get or create the partition actor for this cell
		APartitionActor* RawPartitionActor = PartitionSubsystem->GetActor(
			PartitionClass,
			CellCoord,
			/*bInCreate=*/true,
			/*InGuid=*/ManagerGuid,
			/*InGridSize=*/GridSize,
			/*bInBoundsSearch=*/true,
			/*InActorCreated=*/[bIsPartitionedWorld, &CellCenter, RuntimeGridName](APartitionActor* NewPartitionActor)
			{
				AArcIWMassISMPartitionActor* MassISMActor = CastChecked<AArcIWMassISMPartitionActor>(NewPartitionActor);
				MassISMActor->SetGridName(RuntimeGridName);
				if (bIsPartitionedWorld)
				{
					NewPartitionActor->SetRuntimeGrid(RuntimeGridName);
				}
				NewPartitionActor->SetActorLocation(CellCenter);
			});

		// Add actor instance to partition
		AArcIWMassISMPartitionActor* MassISMActor = CastChecked<AArcIWMassISMPartitionActor>(RawPartitionActor);
		MassISMActor->AddActorInstance(
			Actor->GetClass(),
			Actor->GetActorTransform(),
			MeshEntries,
			AdditionalConfig);

		ModifiedPartitionActors.Add(RawPartitionActor);

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
