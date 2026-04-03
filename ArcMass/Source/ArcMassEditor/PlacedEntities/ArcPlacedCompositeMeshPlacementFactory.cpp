// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcPlacedCompositeMeshPlacementFactory.h"

#include "Editor.h"
#include "Subsystems/PlacementSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "ArcMass/CompositeVisualization/ArcCompositeMeshVisualizationTrait.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPlacedCompositeMeshPlacementFactory)

bool UArcPlacedCompositeMeshPlacementFactory::CanPlaceElementsFromAssetData(const FAssetData& InAssetData)
{
	if (InAssetData.GetClass() != UMassEntityConfigAsset::StaticClass())
	{
		return false;
	}

	UMassEntityConfigAsset* ConfigAsset = Cast<UMassEntityConfigAsset>(InAssetData.GetAsset());
	if (!ConfigAsset)
	{
		return false;
	}

	const UArcCompositeMeshVisualizationTrait* CompositeTrait = Cast<UArcCompositeMeshVisualizationTrait>(
		ConfigAsset->FindTrait(UArcCompositeMeshVisualizationTrait::StaticClass()));

	return CompositeTrait && CompositeTrait->IsValid();
}

bool UArcPlacedCompositeMeshPlacementFactory::PrePlaceAsset(FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions)
{
	return true;
}

TArray<FTypedElementHandle> UArcPlacedCompositeMeshPlacementFactory::PlaceAsset(const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions)
{
	UMassEntityConfigAsset* ConfigAsset = Cast<UMassEntityConfigAsset>(InPlacementInfo.AssetToPlace.GetAsset());
	if (!ConfigAsset)
	{
		return {};
	}

	const UArcCompositeMeshVisualizationTrait* CompositeTrait = Cast<UArcCompositeMeshVisualizationTrait>(
		ConfigAsset->FindTrait(UArcCompositeMeshVisualizationTrait::StaticClass()));
	if (!CompositeTrait || !CompositeTrait->IsValid())
	{
		return {};
	}

	UWorld* World = nullptr;
	if (InPlacementInfo.PreferredLevel.IsValid())
	{
		World = InPlacementInfo.PreferredLevel->GetWorld();
	}
	if (!World && GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
	if (!World)
	{
		return {};
	}

	// Preview path: spawn a temporary actor with one StaticMeshComponent per mesh entry
	if (InPlacementOptions.bIsCreatingPreviewElements)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags = RF_Transient;
		SpawnParams.bTemporaryEditorActor = true;

		AActor* PreviewActor = World->SpawnActor<AActor>(AActor::StaticClass(), InPlacementInfo.FinalizedTransform, SpawnParams);
		if (!PreviewActor)
		{
			return {};
		}

		USceneComponent* RootComp = NewObject<USceneComponent>(PreviewActor);
		PreviewActor->SetRootComponent(RootComp);
		RootComp->RegisterComponent();

		const TArray<FArcCompositeMeshEntry>& MeshEntries = CompositeTrait->GetMeshEntries();
		for (int32 MeshIndex = 0; MeshIndex < MeshEntries.Num(); ++MeshIndex)
		{
			const FArcCompositeMeshEntry& MeshEntry = MeshEntries[MeshIndex];
			if (!MeshEntry.Mesh)
			{
				continue;
			}

			UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(PreviewActor);
			MeshComponent->SetStaticMesh(MeshEntry.Mesh);
			MeshComponent->SetRelativeTransform(MeshEntry.RelativeTransform);

			for (int32 MatIndex = 0; MatIndex < MeshEntry.MaterialOverrides.Num(); ++MatIndex)
			{
				if (MeshEntry.MaterialOverrides[MatIndex])
				{
					MeshComponent->SetMaterial(MatIndex, MeshEntry.MaterialOverrides[MatIndex]);
				}
			}

			MeshComponent->SetupAttachment(RootComp);
			MeshComponent->RegisterComponent();
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		TArray<FTypedElementHandle> Handles;
		FTypedElementHandle Handle = UEngineElementsLibrary::AcquireEditorActorElementHandle(PreviewActor);
		if (Handle.IsSet())
		{
			Handles.Add(Handle);
		}
		return Handles;
	}

	// Final placement path: add instance to partition actor
	UActorPartitionSubsystem* PartitionSubsystem = World->GetSubsystem<UActorPartitionSubsystem>();
	if (!PartitionSubsystem)
	{
		return {};
	}

	const FVector PlacementLocation = InPlacementInfo.FinalizedTransform.GetLocation();
	UClass* PartitionClass = AArcPlacedEntityPartitionActor::StaticClass();
	const uint32 GridSize = PartitionClass->GetDefaultObject<AArcPlacedEntityPartitionActor>()->GetDefaultGridSize(World);

	UActorPartitionSubsystem::FCellCoord CellCoord = UActorPartitionSubsystem::FCellCoord::GetCellCoord(
		PlacementLocation,
		World->PersistentLevel,
		GridSize);

	AArcPlacedEntityPartitionActor* PlacedEntityActor = Cast<AArcPlacedEntityPartitionActor>(
		PartitionSubsystem->GetActor(
			PartitionClass,
			CellCoord,
			/*bInCreate=*/true,
			/*InGuid=*/FGuid(),
			/*InGridSize=*/GridSize,
			/*bInBoundsSearch=*/true));

	if (!PlacedEntityActor)
	{
		return {};
	}

	int32 EntryIndex = INDEX_NONE;
	int32 InstanceIndex = INDEX_NONE;
	const FTransform LocalTransform = InPlacementInfo.FinalizedTransform.GetRelativeTransform(PlacedEntityActor->GetActorTransform());
	PlacedEntityActor->AddInstance(ConfigAsset, LocalTransform, EntryIndex, InstanceIndex);

	UInstancedStaticMeshComponent* ISMComponent = PlacedEntityActor->GetEditorPreviewISMC(EntryIndex);
	if (!ISMComponent || InstanceIndex == INDEX_NONE)
	{
		return {};
	}

	TArray<FTypedElementHandle> Handles;
	FTypedElementHandle Handle = UEngineElementsLibrary::AcquireEditorSMInstanceElementHandle(ISMComponent, InstanceIndex);
	if (Handle.IsSet())
	{
		Handles.Add(Handle);
	}
	return Handles;
}

void UArcPlacedCompositeMeshPlacementFactory::PostPlaceAsset(TArrayView<const FTypedElementHandle> InHandle, const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions)
{
}

FAssetData UArcPlacedCompositeMeshPlacementFactory::GetAssetDataFromElementHandle(const FTypedElementHandle& InHandle)
{
	return FAssetData();
}

void UArcPlacedCompositeMeshPlacementFactory::BeginPlacement(const FPlacementOptions& InPlacementOptions)
{
}

void UArcPlacedCompositeMeshPlacementFactory::EndPlacement(TArrayView<const FTypedElementHandle> InPlacedElements, const FPlacementOptions& InPlacementOptions)
{
}

UInstancedPlacemenClientSettings* UArcPlacedCompositeMeshPlacementFactory::FactorySettingsObjectForPlacement(const FAssetData& InAssetData, const FPlacementOptions& InPlacementOptions)
{
	return nullptr;
}
