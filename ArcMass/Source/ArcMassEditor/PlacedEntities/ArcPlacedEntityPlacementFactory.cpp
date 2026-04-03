// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcPlacedEntityPlacementFactory.h"

#include "Editor.h"
#include "Subsystems/PlacementSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "ArcMass/Visualization/ArcMeshEntityVisualizationTrait.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPlacedEntityPlacementFactory)

bool UArcPlacedEntityPlacementFactory::CanPlaceElementsFromAssetData(const FAssetData& InAssetData)
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

	const UArcMeshEntityVisualizationTrait* MeshTrait = Cast<UArcMeshEntityVisualizationTrait>(
		ConfigAsset->FindTrait(UArcMeshEntityVisualizationTrait::StaticClass()));

	return MeshTrait && MeshTrait->IsValid();
}

bool UArcPlacedEntityPlacementFactory::PrePlaceAsset(FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions)
{
	return true;
}

TArray<FTypedElementHandle> UArcPlacedEntityPlacementFactory::PlaceAsset(const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions)
{
	UMassEntityConfigAsset* ConfigAsset = Cast<UMassEntityConfigAsset>(InPlacementInfo.AssetToPlace.GetAsset());
	if (!ConfigAsset)
	{
		return {};
	}

	const UArcMeshEntityVisualizationTrait* MeshTrait = Cast<UArcMeshEntityVisualizationTrait>(
		ConfigAsset->FindTrait(UArcMeshEntityVisualizationTrait::StaticClass()));
	if (!MeshTrait || !MeshTrait->IsValid())
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

	// Preview path: spawn a temporary AStaticMeshActor so the viewport can track and reposition it
	if (InPlacementOptions.bIsCreatingPreviewElements)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags = RF_Transient;
		SpawnParams.bTemporaryEditorActor = true;

		AStaticMeshActor* PreviewActor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), InPlacementInfo.FinalizedTransform, SpawnParams);
		if (!PreviewActor)
		{
			return {};
		}

		UStaticMeshComponent* MeshComponent = PreviewActor->GetStaticMeshComponent();
		MeshComponent->UnregisterComponent();
		MeshComponent->SetStaticMesh(MeshTrait->GetMesh());
		MeshComponent->SetRelativeTransform(MeshTrait->GetComponentTransform());

		const TArray<TObjectPtr<UMaterialInterface>>& MaterialOverrides = MeshTrait->GetMaterialOverrides();
		for (int32 Index = 0; Index < MaterialOverrides.Num(); ++Index)
		{
			if (MaterialOverrides[Index])
			{
				MeshComponent->SetMaterial(Index, MaterialOverrides[Index]);
			}
		}

		MeshComponent->RegisterComponent();
		PreviewActor->SetActorEnableCollision(false);

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

void UArcPlacedEntityPlacementFactory::PostPlaceAsset(TArrayView<const FTypedElementHandle> InHandle, const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions)
{
}

FAssetData UArcPlacedEntityPlacementFactory::GetAssetDataFromElementHandle(const FTypedElementHandle& InHandle)
{
	return FAssetData();
}

void UArcPlacedEntityPlacementFactory::BeginPlacement(const FPlacementOptions& InPlacementOptions)
{
}

void UArcPlacedEntityPlacementFactory::EndPlacement(TArrayView<const FTypedElementHandle> InPlacedElements, const FPlacementOptions& InPlacementOptions)
{
}

UInstancedPlacemenClientSettings* UArcPlacedEntityPlacementFactory::FactorySettingsObjectForPlacement(const FAssetData& InAssetData, const FPlacementOptions& InPlacementOptions)
{
	return nullptr;
}
