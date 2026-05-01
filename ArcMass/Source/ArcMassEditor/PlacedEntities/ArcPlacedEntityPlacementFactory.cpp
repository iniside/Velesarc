// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcPlacedEntityPlacementFactory.h"

#include "Editor.h"
#include "Subsystems/PlacementSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "ArcMass/Visualization/ArcMeshEntityVisualizationTrait.h"
#include "ArcMass/Visualization/ArcSkinnedMeshEntityVisualizationTrait.h"
#include "Components/SkeletalMeshComponent.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityGridTrait.h"
#include "Misc/ArchiveMD5.h"
#include "ArcMass/Elements/SkMInstance/ArcSkMInstanceElementUtils.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "ArcMass/PlacedEntities/ArcAlwaysLoadedEntityTrait.h"
#include "ArcMass/PlacedEntities/ArcAlwaysLoadedEntityActor.h"
#include "EngineUtils.h"
#include "ArcMass/Visualization/ArcFastGeoVisualizationTrait.h"
#include "ArcMass/PlacedEntities/ArcFastGeoPartitionActor.h"

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
	if (MeshTrait && MeshTrait->IsValid())
	{
		return true;
	}

	const UArcSkinnedMeshEntityVisualizationTrait* SkinnedTrait = Cast<UArcSkinnedMeshEntityVisualizationTrait>(
		ConfigAsset->FindTrait(UArcSkinnedMeshEntityVisualizationTrait::StaticClass()));
	if (SkinnedTrait && SkinnedTrait->IsValid())
	{
		return true;
	}

	const UArcFastGeoVisualizationTrait* FastGeoTrait = Cast<UArcFastGeoVisualizationTrait>(
		ConfigAsset->FindTrait(UArcFastGeoVisualizationTrait::StaticClass()));
	if (FastGeoTrait && FastGeoTrait->IsValid())
	{
		return true;
	}

	return false;
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

	// Resolve which visualization trait is present — static mesh takes priority
	const UArcMeshEntityVisualizationTrait* MeshTrait = Cast<UArcMeshEntityVisualizationTrait>(
		ConfigAsset->FindTrait(UArcMeshEntityVisualizationTrait::StaticClass()));
	const bool bHasStaticMesh = MeshTrait && MeshTrait->IsValid();

	const UArcSkinnedMeshEntityVisualizationTrait* SkinnedTrait = nullptr;
	bool bHasSkinnedMesh = false;
	if (!bHasStaticMesh)
	{
		SkinnedTrait = Cast<UArcSkinnedMeshEntityVisualizationTrait>(
			ConfigAsset->FindTrait(UArcSkinnedMeshEntityVisualizationTrait::StaticClass()));
		bHasSkinnedMesh = SkinnedTrait && SkinnedTrait->IsValid();
	}

	const UArcFastGeoVisualizationTrait* FastGeoTrait = Cast<UArcFastGeoVisualizationTrait>(
		ConfigAsset->FindTrait(UArcFastGeoVisualizationTrait::StaticClass()));
	const bool bHasFastGeo = FastGeoTrait && FastGeoTrait->IsValid();

	if (!bHasStaticMesh && !bHasSkinnedMesh && !bHasFastGeo)
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

	// Preview path: spawn a temporary actor so the viewport can track and reposition it
	if (InPlacementOptions.bIsCreatingPreviewElements)
	{
		if (bHasFastGeo)
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
			MeshComponent->SetStaticMesh(FastGeoTrait->GetMesh());
			MeshComponent->SetRelativeTransform(FastGeoTrait->GetComponentTransform());

			const TArray<TObjectPtr<UMaterialInterface>>& MaterialOverrides = FastGeoTrait->GetMaterialOverrides();
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

		if (bHasStaticMesh)
		{
			// Existing static mesh preview path
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
		else
		{
			// Skinned mesh preview path
			FActorSpawnParameters SpawnParams;
			SpawnParams.ObjectFlags = RF_Transient;
			SpawnParams.bTemporaryEditorActor = true;

			ASkeletalMeshActor* PreviewActor = World->SpawnActor<ASkeletalMeshActor>(
				ASkeletalMeshActor::StaticClass(), InPlacementInfo.FinalizedTransform, SpawnParams);
			if (!PreviewActor)
			{
				return {};
			}

			USkeletalMeshComponent* SkelMeshComp = PreviewActor->GetSkeletalMeshComponent();
			SkelMeshComp->SetSkinnedAssetAndUpdate(SkinnedTrait->GetSkinnedAsset());
			SkelMeshComp->SetRelativeTransform(SkinnedTrait->GetComponentTransform());

			const TArray<TObjectPtr<UMaterialInterface>>& MaterialOverrides = SkinnedTrait->GetMaterialOverrides();
			for (int32 Index = 0; Index < MaterialOverrides.Num(); ++Index)
			{
				if (MaterialOverrides[Index])
				{
					SkelMeshComp->SetMaterial(Index, MaterialOverrides[Index]);
				}
			}

			PreviewActor->SetActorEnableCollision(false);

			TArray<FTypedElementHandle> Handles;
			FTypedElementHandle Handle = UEngineElementsLibrary::AcquireEditorActorElementHandle(PreviewActor);
			if (Handle.IsSet())
			{
				Handles.Add(Handle);
			}
			return Handles;
		}
	}

	// Always-loaded entity path: route to singleton actor
	const UArcAlwaysLoadedEntityTrait* AlwaysLoadedTrait = Cast<UArcAlwaysLoadedEntityTrait>(
		ConfigAsset->FindTrait(UArcAlwaysLoadedEntityTrait::StaticClass()));
	if (AlwaysLoadedTrait != nullptr)
	{
		AArcAlwaysLoadedEntityActor* AlwaysLoadedActor = nullptr;

		// Find existing singleton
		for (TActorIterator<AArcAlwaysLoadedEntityActor> It(World); It; ++It)
		{
			AlwaysLoadedActor = *It;
			break;
		}

		// Spawn singleton if not found
		if (AlwaysLoadedActor == nullptr)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Name = FName(TEXT("AlwaysLoadedEntityActor"));
			AlwaysLoadedActor = World->SpawnActor<AArcAlwaysLoadedEntityActor>(
				AArcAlwaysLoadedEntityActor::StaticClass(), FTransform::Identity, SpawnParams);
		}

		if (AlwaysLoadedActor == nullptr)
		{
			return {};
		}

		int32 EntryIndex = INDEX_NONE;
		int32 InstanceIndex = INDEX_NONE;
		const FTransform AlwaysLoadedActorTransform = AlwaysLoadedActor->GetActorTransform();
		const FTransform LocalTransform = InPlacementInfo.FinalizedTransform.GetRelativeTransform(AlwaysLoadedActorTransform);
		AlwaysLoadedActor->AddInstance(ConfigAsset, LocalTransform, EntryIndex, InstanceIndex);

		if (bHasStaticMesh)
		{
			UInstancedStaticMeshComponent* ISMComponent = AlwaysLoadedActor->GetEditorPreviewISMC(EntryIndex);
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

		if (bHasSkinnedMesh)
		{
			UInstancedSkinnedMeshComponent* SkMComponent = AlwaysLoadedActor->GetEditorPreviewSkMC(EntryIndex);
			if (!SkMComponent || InstanceIndex == INDEX_NONE)
			{
				return {};
			}

			TArray<FTypedElementHandle> Handles;
			FTypedElementHandle Handle = ArcSkMInstanceElementUtils::AcquireEditorSkMInstanceElementHandle(SkMComponent, InstanceIndex);
			if (Handle.IsSet())
			{
				Handles.Add(Handle);
			}
			return Handles;
		}

		return {};
	}

	// FastGeo dual-actor placement path
	if (bHasFastGeo)
	{
		UActorPartitionSubsystem* PartitionSubsystem = World->GetSubsystem<UActorPartitionSubsystem>();
		if (!PartitionSubsystem)
		{
			return {};
		}

		const FVector PlacementLocation = InPlacementInfo.FinalizedTransform.GetLocation();
		const bool bIsPartitionedWorld = World->IsPartitionedWorld();

		// --- Entity partition actor (same as normal path) ---
		{
			UClass* EntityPartitionClass = AArcPlacedEntityPartitionActor::StaticClass();
			const uint32 EntityGridSize = EntityPartitionClass->GetDefaultObject<AArcPlacedEntityPartitionActor>()->GetDefaultGridSize(World);

			FName EntityGridName = NAME_None;
			const UArcPlacedEntityGridTrait* GridTrait = Cast<UArcPlacedEntityGridTrait>(
				ConfigAsset->FindTrait(UArcPlacedEntityGridTrait::StaticClass()));
			if (GridTrait != nullptr)
			{
#if WITH_EDITORONLY_DATA
				EntityGridName = GridTrait->GridName;
#endif
			}

			FGuid EntityManagerGuid;
			if (!EntityGridName.IsNone() && bIsPartitionedWorld)
			{
				FArchiveMD5 ArMD5;
				ArMD5 << EntityGridName;
				EntityManagerGuid = ArMD5.GetGuidFromHash();
			}

			UActorPartitionSubsystem::FCellCoord EntityCellCoord = UActorPartitionSubsystem::FCellCoord::GetCellCoord(
				PlacementLocation,
				World->PersistentLevel,
				EntityGridSize);

			AArcPlacedEntityPartitionActor* PlacedEntityActor = Cast<AArcPlacedEntityPartitionActor>(
				PartitionSubsystem->GetActor(
					EntityPartitionClass,
					EntityCellCoord,
					/*bInCreate=*/true,
					/*InGuid=*/EntityManagerGuid,
					/*InGridSize=*/EntityGridSize,
					/*bInBoundsSearch=*/true,
					/*InActorCreated=*/[bIsPartitionedWorld, EntityGridName, &EntityManagerGuid](APartitionActor* NewPartitionActor)
					{
						AArcPlacedEntityPartitionActor* PlacedActor = CastChecked<AArcPlacedEntityPartitionActor>(NewPartitionActor);
						PlacedActor->SetGridName(EntityGridName);
						if (bIsPartitionedWorld && !EntityGridName.IsNone())
						{
							NewPartitionActor->SetRuntimeGrid(EntityGridName);
							PlacedActor->SetGridGuid(EntityManagerGuid);
						}
					}));

			if (PlacedEntityActor)
			{
				int32 EntryIndex = INDEX_NONE;
				int32 InstanceIndex = INDEX_NONE;
				const FTransform LocalTransform = InPlacementInfo.FinalizedTransform.GetRelativeTransform(PlacedEntityActor->GetActorTransform());
				PlacedEntityActor->AddInstance(ConfigAsset, LocalTransform, EntryIndex, InstanceIndex);
			}
		}

		// --- FastGeo partition actor ---
		{
			UClass* FastGeoPartitionClass = AArcFastGeoPartitionActor::StaticClass();
			const uint32 FastGeoGridSize = FastGeoPartitionClass->GetDefaultObject<AArcFastGeoPartitionActor>()->GetDefaultGridSize(World);

#if WITH_EDITORONLY_DATA
			FName FastGeoGridName = FastGeoTrait->FastGeoGridName;
#else
			FName FastGeoGridName = NAME_None;
#endif

			FGuid FastGeoManagerGuid;
			if (!FastGeoGridName.IsNone() && bIsPartitionedWorld)
			{
				FArchiveMD5 ArMD5;
				ArMD5 << FastGeoGridName;
				FastGeoManagerGuid = ArMD5.GetGuidFromHash();
			}

			UActorPartitionSubsystem::FCellCoord FastGeoCellCoord = UActorPartitionSubsystem::FCellCoord::GetCellCoord(
				PlacementLocation,
				World->PersistentLevel,
				FastGeoGridSize);

			AArcFastGeoPartitionActor* FastGeoActor = Cast<AArcFastGeoPartitionActor>(
				PartitionSubsystem->GetActor(
					FastGeoPartitionClass,
					FastGeoCellCoord,
					/*bInCreate=*/true,
					/*InGuid=*/FastGeoManagerGuid,
					/*InGridSize=*/FastGeoGridSize,
					/*bInBoundsSearch=*/true,
					/*InActorCreated=*/[bIsPartitionedWorld, FastGeoGridName, &FastGeoManagerGuid](APartitionActor* NewPartitionActor)
					{
						AArcFastGeoPartitionActor* FastGeoPartActor = CastChecked<AArcFastGeoPartitionActor>(NewPartitionActor);
						FastGeoPartActor->SetGridName(FastGeoGridName);
						if (bIsPartitionedWorld && !FastGeoGridName.IsNone())
						{
							NewPartitionActor->SetRuntimeGrid(FastGeoGridName);
							FastGeoPartActor->SetGridGuid(FastGeoManagerGuid);
						}
					}));

			if (!FastGeoActor)
			{
				return {};
			}

			int32 EntryIndex = INDEX_NONE;
			int32 InstanceIndex = INDEX_NONE;
			const FTransform FastGeoActorTransform = FastGeoActor->GetActorTransform();
			const FTransform LocalTransform = InPlacementInfo.FinalizedTransform.GetRelativeTransform(FastGeoActorTransform);
			FastGeoActor->AddInstance(ConfigAsset, LocalTransform, EntryIndex, InstanceIndex);

			UInstancedStaticMeshComponent* ISMComponent = FastGeoActor->GetEditorPreviewISMC(EntryIndex);
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

	// Check for grid trait
	FName GridName = NAME_None;
	const UArcPlacedEntityGridTrait* GridTrait = Cast<UArcPlacedEntityGridTrait>(
		ConfigAsset->FindTrait(UArcPlacedEntityGridTrait::StaticClass()));
	if (GridTrait != nullptr)
	{
#if WITH_EDITORONLY_DATA
		GridName = GridTrait->GridName;
#endif
	}

	const bool bIsPartitionedWorld = World->IsPartitionedWorld();
	FGuid ManagerGuid;
	if (!GridName.IsNone() && bIsPartitionedWorld)
	{
		FArchiveMD5 ArMD5;
		ArMD5 << GridName;
		ManagerGuid = ArMD5.GetGuidFromHash();
	}

	UActorPartitionSubsystem::FCellCoord CellCoord = UActorPartitionSubsystem::FCellCoord::GetCellCoord(
		PlacementLocation,
		World->PersistentLevel,
		GridSize);

	AArcPlacedEntityPartitionActor* PlacedEntityActor = Cast<AArcPlacedEntityPartitionActor>(
		PartitionSubsystem->GetActor(
			PartitionClass,
			CellCoord,
			/*bInCreate=*/true,
			/*InGuid=*/ManagerGuid,
			/*InGridSize=*/GridSize,
			/*bInBoundsSearch=*/true,
			/*InActorCreated=*/[bIsPartitionedWorld, GridName, &ManagerGuid](APartitionActor* NewPartitionActor)
			{
				AArcPlacedEntityPartitionActor* PlacedActor = CastChecked<AArcPlacedEntityPartitionActor>(NewPartitionActor);
				PlacedActor->SetGridName(GridName);
				if (bIsPartitionedWorld && !GridName.IsNone())
				{
					NewPartitionActor->SetRuntimeGrid(GridName);
					PlacedActor->SetGridGuid(ManagerGuid);
				}
			}));

	if (!PlacedEntityActor)
	{
		return {};
	}

	int32 EntryIndex = INDEX_NONE;
	int32 InstanceIndex = INDEX_NONE;
	const FTransform LocalTransform = InPlacementInfo.FinalizedTransform.GetRelativeTransform(PlacedEntityActor->GetActorTransform());
	PlacedEntityActor->AddInstance(ConfigAsset, LocalTransform, EntryIndex, InstanceIndex);

	// For static mesh entries, return an SM instance element handle (supports per-instance selection)
	if (bHasStaticMesh)
	{
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

	// For skinned mesh entries, return a SkMInstance element handle (supports per-instance selection)
	UInstancedSkinnedMeshComponent* SkMComponent = PlacedEntityActor->GetEditorPreviewSkMC(EntryIndex);
	if (SkMComponent == nullptr)
	{
		return {};
	}

	const FPrimitiveInstanceId PrimId = SkMComponent->GetInstanceId(InstanceIndex);

	TArray<FTypedElementHandle> Handles;
	FTypedElementHandle Handle = ArcSkMInstanceElementUtils::AcquireEditorSkMInstanceElementHandle(SkMComponent, PrimId.GetAsIndex());
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
