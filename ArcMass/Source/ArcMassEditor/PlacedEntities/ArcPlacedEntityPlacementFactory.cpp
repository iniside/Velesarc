// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcPlacedEntityPlacementFactory.h"

#include "Editor.h"
#include "Subsystems/PlacementSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "ArcMass/Visualization/ArcUnifiedMeshVisualizationTrait.h"
#include "ArcMass/Spatial/ArcWorkAreaBoundsTrait.h"
#include "Components/SkeletalMeshComponent.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityGridTrait.h"
#include "Misc/ArchiveMD5.h"
#include "ArcMass/PlacedEntities/ArcAlwaysLoadedEntityTrait.h"
#include "ArcMass/PlacedEntities/ArcAlwaysLoadedEntityActor.h"
#include "EngineUtils.h"
#include "ArcMass/Visualization/ArcFastGeoVisualizationTrait.h"
#include "ArcMass/PlacedEntities/ArcFastGeoPartitionActor.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementUtils.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementId.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPlacedEntityPlacementFactory)

DEFINE_LOG_CATEGORY_STATIC(LogArcPlacedEntityPlacementFactory, Log, All);

bool UArcPlacedEntityPlacementFactory::CanPlaceElementsFromAssetData(const FAssetData& InAssetData)
{
	if (!InAssetData.GetClass() || !InAssetData.GetClass()->IsChildOf(UMassEntityConfigAsset::StaticClass()))
	{
		return false;
	}

	UMassEntityConfigAsset* ConfigAsset = Cast<UMassEntityConfigAsset>(InAssetData.GetAsset());
	if (!ConfigAsset)
	{
		return false;
	}

	const UArcFastGeoVisualizationTrait* FastGeoTrait = Cast<UArcFastGeoVisualizationTrait>(
		ConfigAsset->FindTrait(UArcFastGeoVisualizationTrait::StaticClass()));
	if (FastGeoTrait && FastGeoTrait->IsValid())
	{
		return true;
	}

	const UArcUnifiedMeshVisualizationTrait* UnifiedTrait = Cast<UArcUnifiedMeshVisualizationTrait>(
		ConfigAsset->FindTrait(UArcUnifiedMeshVisualizationTrait::StaticClass()));
	if (UnifiedTrait && UnifiedTrait->IsValid())
	{
		return true;
	}

	const UArcWorkAreaBoundsTrait* WorkAreaTrait = Cast<UArcWorkAreaBoundsTrait>(
		ConfigAsset->FindTrait(UArcWorkAreaBoundsTrait::StaticClass()));
	if (WorkAreaTrait && WorkAreaTrait->IsValid())
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

	const UArcFastGeoVisualizationTrait* FastGeoTrait = Cast<UArcFastGeoVisualizationTrait>(
		ConfigAsset->FindTrait(UArcFastGeoVisualizationTrait::StaticClass()));
	const bool bHasFastGeo = FastGeoTrait && FastGeoTrait->IsValid();

	const UArcUnifiedMeshVisualizationTrait* UnifiedTrait = Cast<UArcUnifiedMeshVisualizationTrait>(
		ConfigAsset->FindTrait(UArcUnifiedMeshVisualizationTrait::StaticClass()));
	const bool bHasUnified = UnifiedTrait && UnifiedTrait->IsValid();
	const EArcUnifiedMeshVisMode UnifiedMode = bHasUnified ? UnifiedTrait->GetInferredMode() : EArcUnifiedMeshVisMode::None;

	const UArcWorkAreaBoundsTrait* WorkAreaTrait = Cast<UArcWorkAreaBoundsTrait>(
		ConfigAsset->FindTrait(UArcWorkAreaBoundsTrait::StaticClass()));
	const bool bHasWorkArea = WorkAreaTrait && WorkAreaTrait->IsValid();

	if (!bHasFastGeo && !bHasUnified && !bHasWorkArea)
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

		if (bHasUnified)
		{
			if (UnifiedMode == EArcUnifiedMeshVisMode::Static)
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
				MeshComponent->SetStaticMesh(UnifiedTrait->GetSingleStaticMeshDirect());
				MeshComponent->SetRelativeTransform(UnifiedTrait->GetSingleRelativeTransform());

				const TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides = UnifiedTrait->GetSingleMaterialOverridesDirect();
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
			else if (UnifiedMode == EArcUnifiedMeshVisMode::Skinned)
			{
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
				SkelMeshComp->SetSkinnedAssetAndUpdate(UnifiedTrait->GetSingleSkinnedAssetDirect());
				SkelMeshComp->SetRelativeTransform(UnifiedTrait->GetSingleRelativeTransform());

				const TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides = UnifiedTrait->GetSingleMaterialOverridesDirect();
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
			else
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.ObjectFlags = RF_Transient;
				SpawnParams.bTemporaryEditorActor = true;

				AActor* PreviewActor = World->SpawnActor<AActor>(
					AActor::StaticClass(), InPlacementInfo.FinalizedTransform, SpawnParams);
				if (!PreviewActor)
				{
					return {};
				}

				USceneComponent* RootComp = NewObject<USceneComponent>(PreviewActor);
				PreviewActor->SetRootComponent(RootComp);
				RootComp->RegisterComponent();

				const TArray<FArcUnifiedMeshEntry>& MeshEntries = UnifiedTrait->GetMeshEntries();
				for (int32 EntryIdx = 0; EntryIdx < MeshEntries.Num(); ++EntryIdx)
				{
					const FArcUnifiedMeshEntry& Entry = MeshEntries[EntryIdx];
					if (!Entry.IsValidEntry())
					{
						continue;
					}

					if (Entry.IsSkinnedEntry())
					{
						USkeletalMeshComponent* SkelComp = NewObject<USkeletalMeshComponent>(PreviewActor);
						SkelComp->SetSkinnedAssetAndUpdate(Entry.SkinnedAsset.LoadSynchronous());
						SkelComp->SetRelativeTransform(Entry.RelativeTransform);

						for (int32 MatIdx = 0; MatIdx < Entry.MaterialOverrides.Num(); ++MatIdx)
						{
							UMaterialInterface* Mat = Entry.MaterialOverrides[MatIdx].LoadSynchronous();
							if (Mat)
							{
								SkelComp->SetMaterial(MatIdx, Mat);
							}
						}

						SkelComp->SetupAttachment(RootComp);
						SkelComp->RegisterComponent();
						SkelComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					}
					else
					{
						UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(PreviewActor);
						MeshComp->SetStaticMesh(Entry.StaticMesh.LoadSynchronous());
						MeshComp->SetRelativeTransform(Entry.RelativeTransform);

						for (int32 MatIdx = 0; MatIdx < Entry.MaterialOverrides.Num(); ++MatIdx)
						{
							UMaterialInterface* Mat = Entry.MaterialOverrides[MatIdx].LoadSynchronous();
							if (Mat)
							{
								MeshComp->SetMaterial(MatIdx, Mat);
							}
						}

						MeshComp->SetupAttachment(RootComp);
						MeshComp->RegisterComponent();
						MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					}
				}

				TArray<FTypedElementHandle> Handles;
				FTypedElementHandle Handle = UEngineElementsLibrary::AcquireEditorActorElementHandle(PreviewActor);
				if (Handle.IsSet())
				{
					Handles.Add(Handle);
				}
				return Handles;
			}
		}
		else if (bHasWorkArea)
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

			UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (!CubeMesh)
			{
				UE_LOGF(LogArcPlacedEntityPlacementFactory, Error,"Failed to load /Engine/BasicShapes/Cube.Cube for work-area preview");
				World->DestroyActor(PreviewActor);
				return {};
			}

			MeshComponent->SetStaticMesh(CubeMesh);
			// BasicShapes/Cube spans 100 uu per axis, so its half-extent of 50 converts Extent to scale.
			MeshComponent->SetRelativeScale3D(WorkAreaTrait->GetExtent() / 50.0);
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
	}

	// Always-loaded entity path: route to singleton actor.
	// Unified Composite mode must use partition actors only — skip through to the partition path below.
	const UArcAlwaysLoadedEntityTrait* AlwaysLoadedTrait = Cast<UArcAlwaysLoadedEntityTrait>(
		ConfigAsset->FindTrait(UArcAlwaysLoadedEntityTrait::StaticClass()));
	const bool bUnifiedCompositeForcePartition = bHasUnified && (UnifiedMode == EArcUnifiedMeshVisMode::Composite);
	if (AlwaysLoadedTrait != nullptr && !bUnifiedCompositeForcePartition)
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

		// Unified Static/Skinned modes via always-loaded actor
		if (bHasUnified)
		{
			if (InstanceIndex == INDEX_NONE)
			{
				return {};
			}

			FArcMassEntityElementId MassElementId;
			MassElementId.PartitionActor = AlwaysLoadedActor;
			MassElementId.EntryIndex = EntryIndex;
			MassElementId.TransformIndex = InstanceIndex;

			TArray<FTypedElementHandle> Handles;
			FTypedElementHandle Handle = ArcMassEntityElementUtils::AcquireEditorArcMassEntityElementHandle(MassElementId);
			if (Handle.IsSet())
			{
				Handles.Add(Handle);
			}
			return Handles;
		}

		if (bHasWorkArea)
		{
			if (InstanceIndex == INDEX_NONE)
			{
				return {};
			}

			FArcMassEntityElementId MassElementId;
			MassElementId.PartitionActor = AlwaysLoadedActor;
			MassElementId.EntryIndex = EntryIndex;
			MassElementId.TransformIndex = InstanceIndex;

			TArray<FTypedElementHandle> Handles;
			FTypedElementHandle Handle = ArcMassEntityElementUtils::AcquireEditorArcMassEntityElementHandle(MassElementId);
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

	// Unified Static, Skinned, and Composite — all route to partition actor at this point
	if (bHasUnified)
	{
		if (InstanceIndex == INDEX_NONE)
		{
			return {};
		}

		FArcMassEntityElementId MassElementId;
		MassElementId.PartitionActor = PlacedEntityActor;
		MassElementId.EntryIndex = EntryIndex;
		MassElementId.TransformIndex = InstanceIndex;

		TArray<FTypedElementHandle> Handles;
		FTypedElementHandle Handle = ArcMassEntityElementUtils::AcquireEditorArcMassEntityElementHandle(MassElementId);
		if (Handle.IsSet())
		{
			Handles.Add(Handle);
		}
		return Handles;
	}

	if (bHasWorkArea)
	{
		if (InstanceIndex == INDEX_NONE)
		{
			return {};
		}

		FArcMassEntityElementId MassElementId;
		MassElementId.PartitionActor = PlacedEntityActor;
		MassElementId.EntryIndex = EntryIndex;
		MassElementId.TransformIndex = InstanceIndex;

		TArray<FTypedElementHandle> Handles;
		FTypedElementHandle Handle = ArcMassEntityElementUtils::AcquireEditorArcMassEntityElementHandle(MassElementId);
		if (Handle.IsSet())
		{
			Handles.Add(Handle);
		}
		return Handles;
	}

	return {};
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
