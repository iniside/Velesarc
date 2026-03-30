// Copyright Lukasz Baran. All Rights Reserved.

#include "PCGSpawnArcIWEntities.h"

#include "PCGArcIWInteropModule.h"
#include "PCGArcIWManagedResource.h"
#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "Data/PCGBasePointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "ArcInstancedWorld/Components/ArcIWMassConfigComponent.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "Animation/TransformProviderData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#if WITH_EDITOR
#include "ArcIWEditorCommands.h"
#endif

#define LOCTEXT_NAMESPACE "PCGSpawnArcIWEntities"

FPCGElementPtr UPCGSpawnArcIWEntitiesSettings::CreateElement() const
{
	return MakeShared<FPCGSpawnArcIWEntitiesElement>();
}

TArray<FPCGPinProperties> UPCGSpawnArcIWEntitiesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& DependencyPin = PinProperties.Emplace_GetRef(
		PCGPinConstants::DefaultExecutionDependencyLabel,
		EPCGDataType::Any,
		/*bInAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true);
	DependencyPin.Usage = EPCGPinUsage::DependencyOnly;
	return PinProperties;
}

#if WITH_EDITOR
FText UPCGSpawnArcIWEntitiesSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Spawns ArcInstancedWorld entities from input point data. "
		"Actor classes are used as templates for mesh and collision extraction. "
		"Editor-only.");
}
#endif

bool FPCGSpawnArcIWEntitiesElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpawnArcIWEntitiesElement::Execute);
#if WITH_EDITOR
	check(Context);

	const UPCGSpawnArcIWEntitiesSettings* Settings = Context->GetInputSettings<UPCGSpawnArcIWEntitiesSettings>();
	check(Settings);

	if (PCGHelpers::IsRuntimeOrPIE())
	{
		PCGLog::LogErrorOnGraph(
			LOCTEXT("CannotSpawnAtRuntime", "ArcIW entities cannot be spawned at runtime."),
			Context);
		return true;
	}

	UWorld* World = Context->ExecutionSource.Get()
		? Context->ExecutionSource->GetExecutionState().GetWorld()
		: nullptr;

	UPCGComponent* SourceComponent = Cast<UPCGComponent>(Context->ExecutionSource.Get());
	if (!World || !SourceComponent)
	{
		return true;
	}

	TSubclassOf<AActor> StaticActorClass = Settings->ActorClass;

	if (!Settings->bSpawnByAttribute && !StaticActorClass)
	{
		if (!Settings->bMuteOnEmptyClass)
		{
			PCGLog::LogErrorOnGraph(
				LOCTEXT("InvalidActorClass", "Invalid actor class. Nothing will be spawned."),
				Context);
		}
		return true;
	}

	FName GridName = Settings->GridName;
	if (GridName.IsNone())
	{
		GridName = UArcIWSettings::Get()->DefaultGridName;
	}

	// Per-class cached capture data.
	struct FClassCaptureData
	{
		TArray<FArcIWMeshEntry> MeshEntries;
		UBodySetup* CollisionBodySetup = nullptr;
		FCollisionProfileName CollisionProfile;
		FBodyInstance BodyTemplate;
		int32 RespawnTimeSeconds = 0;
		EArcMassPhysicsBodyType PhysicsBodyType = EArcMassPhysicsBodyType::Static;
	};
	TMap<TSubclassOf<AActor>, FClassCaptureData> ClassCaptureCache;

	// Tracking for managed resource.
	TMap<AArcIWMassISMPartitionActor*, TMap<TSubclassOf<AActor>, TArray<int32>>> TrackingData;
	TSet<AArcIWMassISMPartitionActor*> CleanedPartitions;

	// Process inputs.
	TArray<FSoftClassPath> ActorClassPaths;
	TMap<FSoftClassPath, TSubclassOf<AActor>> ActorClassesMap;
	const bool bUseAttribute = Settings->bSpawnByAttribute;

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGBasePointData* Data = Cast<UPCGBasePointData>(Input.Data);
		if (!Data)
		{
			continue;
		}

		const FPCGAttributePropertyInputSelector Selector = Settings->SpawnAttributeSelector.CopyAndFixLast(Data);
		ActorClassPaths.Reset();

		if (bUseAttribute)
		{
			if (!PCGAttributeAccessorHelpers::ExtractAllValues(Data, Selector, ActorClassPaths, Context))
			{
				continue;
			}

			for (const FSoftClassPath& ClassPath : ActorClassPaths)
			{
				if (!ActorClassesMap.Contains(ClassPath))
				{
					TSubclassOf<AActor> LoadedClass = ClassPath.TryLoadClass<AActor>();
					ActorClassesMap.Add(ClassPath, LoadedClass);

					if (!LoadedClass && !Settings->bMuteOnEmptyClass)
					{
						PCGLog::LogWarningOnGraph(
							FText::Format(
								LOCTEXT("InvalidClassPath", "Invalid actor class from path '{0}'."),
								FText::FromString(ClassPath.ToString())),
							Context);
					}
				}
			}
		}

		TConstPCGValueRange<FTransform> PointTransforms = Data->GetConstTransformValueRange();

		for (int32 Index = 0; Index < PointTransforms.Num(); ++Index)
		{
			const FTransform& PointTransform = PointTransforms[Index];
			TSubclassOf<AActor> CurrentClass = bUseAttribute
				? ActorClassesMap[ActorClassPaths[Index]]
				: StaticActorClass;

			if (!CurrentClass)
			{
				continue;
			}

			// Capture mesh/collision data once per class.
			FClassCaptureData* CaptureData = ClassCaptureCache.Find(CurrentClass);
			if (!CaptureData)
			{
				FClassCaptureData NewCapture;

				// Spawn a temporary actor so Blueprint-added components are instantiated.
				// CDO doesn't have SCS components, so GetComponents would return nothing for BPs.
				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				SpawnParams.bNoFail = true;
				SpawnParams.ObjectFlags = RF_Transient;
				AActor* TempActor = World->SpawnActor(CurrentClass, nullptr, SpawnParams);
				if (TempActor)
				{
					UE::ArcIW::Editor::CaptureActorMeshData(TempActor, NewCapture.MeshEntries);
					UE::ArcIW::Editor::CaptureActorCollisionData(
						TempActor,
						SourceComponent,
						NewCapture.CollisionBodySetup,
						NewCapture.CollisionProfile,
						NewCapture.BodyTemplate);
					UArcIWMassConfigComponent* ConfigComp = TempActor->FindComponentByClass<UArcIWMassConfigComponent>();
					if (ConfigComp)
					{
						NewCapture.RespawnTimeSeconds = ConfigComp->RespawnTimeSeconds;
						NewCapture.PhysicsBodyType = ConfigComp->PhysicsBodyType;
					}
					TempActor->Destroy();
				}

				// Apply TransformProvider override chain to skinned entries:
				// captured-from-component (already set by CaptureActorMeshData) > node setting > settings default
				UTransformProviderData* NodeProvider = Settings->TransformProvider;
				UTransformProviderData* DefaultProvider =
					UArcIWSettings::Get()->DefaultSkinnedMeshTransformProvider.LoadSynchronous();

				for (FArcIWMeshEntry& MeshEntry : NewCapture.MeshEntries)
				{
					if (MeshEntry.IsSkinned() && !MeshEntry.TransformProvider)
					{
						MeshEntry.TransformProvider = NodeProvider ? NodeProvider : DefaultProvider;
					}
				}

				CaptureData = &ClassCaptureCache.Add(CurrentClass, MoveTemp(NewCapture));
			}

			// Find or create partition actor.
			AArcIWMassISMPartitionActor* PartitionActor =
				UE::ArcIW::Editor::FindOrCreatePartitionActor(World, PointTransform.GetLocation(), GridName);

			if (!PartitionActor)
			{
				continue;
			}

			// Purge previous entries from this source on first encounter (handles regeneration).
			if (!CleanedPartitions.Contains(PartitionActor))
			{
				PartitionActor->RemoveEntriesBySource(SourceComponent);
				CleanedPartitions.Add(PartitionActor);
			}

			// Record pre-add count to know the index of the new transform.
			const TArray<FArcIWActorClassData>& ClassEntries = PartitionActor->GetActorClassEntries();
			int32 PreAddCount = 0;
			for (const FArcIWActorClassData& Entry : ClassEntries)
			{
				if (Entry.ActorClass == CurrentClass && Entry.SourcePCGComponent.Get() == SourceComponent)
				{
					PreAddCount = Entry.InstanceTransforms.Num();
					break;
				}
			}

			// Per-point TransformProvider attribute override
			const TArray<FArcIWMeshEntry>* MeshEntriesToUse = &CaptureData->MeshEntries;
			TArray<FArcIWMeshEntry> OverriddenMeshEntries;

			const FPCGMetadataAttribute<FSoftObjectPath>* TransformProviderAttr =
				Data->ConstMetadata() ? Data->ConstMetadata()->GetConstTypedAttribute<FSoftObjectPath>(TEXT("TransformProvider")) : nullptr;
			if (TransformProviderAttr)
			{
				const int32 MetadataEntry = Data->GetMetadataEntry(Index);
				if (MetadataEntry != INDEX_NONE)
				{
					FSoftObjectPath ProviderPath = TransformProviderAttr->GetValue(MetadataEntry);

					UTransformProviderData* PointProvider = Cast<UTransformProviderData>(ProviderPath.TryLoad());
					if (PointProvider)
					{
						OverriddenMeshEntries = CaptureData->MeshEntries;
						for (FArcIWMeshEntry& MeshEntry : OverriddenMeshEntries)
						{
							if (MeshEntry.IsSkinned())
							{
								MeshEntry.TransformProvider = PointProvider;
							}
						}
						MeshEntriesToUse = &OverriddenMeshEntries;
					}
				}
			}

			PartitionActor->AddActorInstance(
				CurrentClass,
				PointTransform,
				*MeshEntriesToUse,
				Settings->AdditionalEntityConfig,
				CaptureData->CollisionBodySetup,
				CaptureData->CollisionProfile,
				CaptureData->BodyTemplate,
				CaptureData->RespawnTimeSeconds,
				CaptureData->PhysicsBodyType,
				SourceComponent);

			TrackingData.FindOrAdd(PartitionActor).FindOrAdd(CurrentClass).Add(PreAddCount);
		}
	}

	// Create managed resource.
	if (!TrackingData.IsEmpty())
	{
		UPCGArcIWManagedResource* Resource = FPCGContext::NewObject_AnyThread<UPCGArcIWManagedResource>(Context, SourceComponent);

		for (TPair<AArcIWMassISMPartitionActor*, TMap<TSubclassOf<AActor>, TArray<int32>>>& PartitionPair : TrackingData)
		{
			for (TPair<TSubclassOf<AActor>, TArray<int32>>& ClassPair : PartitionPair.Value)
			{
				FPCGArcIWTrackedPartitionEntry& Entry = Resource->TrackedEntries.AddDefaulted_GetRef();
				Entry.PartitionActor = PartitionPair.Key;
				Entry.ActorClass = ClassPair.Key;
				Entry.TransformIndices = MoveTemp(ClassPair.Value);
			}
		}

		for (TPair<AArcIWMassISMPartitionActor*, TMap<TSubclassOf<AActor>, TArray<int32>>>& PartitionPair : TrackingData)
		{
			PartitionPair.Key->RebuildEditorPreviewISMCs();
			PartitionPair.Key->Modify();
		}

		SourceComponent->AddToManagedResources(Resource);
	}
#else
	PCGLog::LogErrorOnGraph(
		LOCTEXT("EditorOnly", "ArcIW entities cannot be spawned in non-editor builds."),
		Context);
#endif
	return true;
}

#undef LOCTEXT_NAMESPACE
