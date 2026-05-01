// Copyright Lukasz Baran. All Rights Reserved.

#include "PCG/PCGSpawnMassEntitiesSettings.h"

#include "PCG/ArcPCGPlacedEntityPartitionActor.h"
#include "PCG/PCGMassPlacedEntityManagedResource.h"
#include "PCG/PCGMassFragmentOverrideHelpers.h"

#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "Data/PCGBasePointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityGridTrait.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityTypes.h"

#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Engine/World.h"
#include "Misc/ArchiveMD5.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSpawnMassEntitiesSettings)

#define LOCTEXT_NAMESPACE "PCGSpawnMassEntities"

FPCGElementPtr UPCGSpawnMassEntitiesSettings::CreateElement() const
{
	return MakeShared<FPCGSpawnMassEntitiesElement>();
}

#if WITH_EDITOR
FText UPCGSpawnMassEntitiesSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Spawns Mass entities from MassEntityConfig assets into ArcPlacedEntityPartitionActors. "
		"Supports per-point config overrides and FragmentName.PropertyName attribute-driven fragment overrides. "
		"Editor-only.");
}
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace PCGSpawnMassEntitiesPrivate
{

FName GetGridNameFromConfig(const UMassEntityConfigAsset* ConfigAsset)
{
#if WITH_EDITORONLY_DATA
	if (!ConfigAsset)
	{
		return NAME_None;
	}

	const FMassEntityConfig& Config = ConfigAsset->GetConfig();
	for (UMassEntityTraitBase* Trait : Config.GetTraits())
	{
		const UArcPlacedEntityGridTrait* GridTrait = Cast<UArcPlacedEntityGridTrait>(Trait);
		if (GridTrait)
		{
			return GridTrait->GridName;
		}
	}
#endif
	return NAME_None;
}

AArcPCGPlacedEntityPartitionActor* FindOrCreatePartitionActor(
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
		return nullptr;
	}

	const bool bIsPartitionedWorld = World->IsPartitionedWorld();
	UClass* PartitionClass = AArcPCGPlacedEntityPartitionActor::StaticClass();

	AArcPCGPlacedEntityPartitionActor* CDO = GetMutableDefault<AArcPCGPlacedEntityPartitionActor>();
	const uint32 GridSize = CDO->GetDefaultGridSize(World);

	FArchiveMD5 ArMD5;
	FString GridStr = GridName.ToString();
	ArMD5 << GridStr;
	const FGuid ManagerGuid = bIsPartitionedWorld ? ArMD5.GetGuidFromHash() : FGuid();

	UActorPartitionSubsystem::FCellCoord CellCoord = UActorPartitionSubsystem::FCellCoord::GetCellCoord(
		WorldLocation,
		World->PersistentLevel,
		GridSize);

	APartitionActor* RawPartitionActor = PartitionSubsystem->GetActor(
		PartitionClass,
		CellCoord,
		/*bInCreate=*/true,
		/*InGuid=*/ManagerGuid,
		/*InGridSize=*/GridSize,
		/*bInBoundsSearch=*/true,
		/*InActorCreated=*/[GridName, &ManagerGuid](APartitionActor* NewPartitionActor)
		{
			AArcPCGPlacedEntityPartitionActor* PlacedActor =
				CastChecked<AArcPCGPlacedEntityPartitionActor>(NewPartitionActor);
			PlacedActor->SetGridName(GridName);
			PlacedActor->SetGridGuid(ManagerGuid);
		});

	return Cast<AArcPCGPlacedEntityPartitionActor>(RawPartitionActor);
}

struct FPartitionConfigKey
{
	AArcPCGPlacedEntityPartitionActor* PartitionActor;
	UMassEntityConfigAsset* Config;

	bool operator==(const FPartitionConfigKey& Other) const
	{
		return PartitionActor == Other.PartitionActor && Config == Other.Config;
	}

	friend uint32 GetTypeHash(const FPartitionConfigKey& Key)
	{
		return HashCombine(GetTypeHash(Key.PartitionActor), GetTypeHash(Key.Config));
	}
};

} // namespace PCGSpawnMassEntitiesPrivate

// ---------------------------------------------------------------------------
// Element execution
// ---------------------------------------------------------------------------

bool FPCGSpawnMassEntitiesElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpawnMassEntitiesElement::Execute);
#if WITH_EDITOR
	check(Context);

	const UPCGSpawnMassEntitiesSettings* Settings = Context->GetInputSettings<UPCGSpawnMassEntitiesSettings>();
	check(Settings);

	if (PCGHelpers::IsRuntimeOrPIE())
	{
		PCGLog::LogErrorOnGraph(
			LOCTEXT("CannotSpawnAtRuntime", "Mass placed entities cannot be spawned at runtime via PCG."),
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

	if (!Settings->DefaultEntityConfig && Settings->EntityConfigAttributeName.IsNone())
	{
		PCGLog::LogErrorOnGraph(
			LOCTEXT("NoConfig", "No DefaultEntityConfig set and no config attribute name specified."),
			Context);
		return true;
	}

	using namespace PCGSpawnMassEntitiesPrivate;

	// Caches
	TMap<UMassEntityConfigAsset*, FName> ConfigToGridName;
	TMap<FName, UScriptStruct*> ResolvedFragmentStructs;
	TMap<FPartitionConfigKey, FArcPlacedEntityEntry> GroupedEntries;
	TMap<const UPCGMetadata*, TMap<FName, TArray<FName>>> MetadataFragmentMaps;

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(Input.Data);
		if (!PointData)
		{
			continue;
		}

		const UPCGMetadata* Metadata = PointData->ConstMetadata();

		// Discover fragment attributes for this data set (cached)
		TMap<FName, TArray<FName>>& FragmentAttrMap = MetadataFragmentMaps.FindOrAdd(Metadata);
		if (FragmentAttrMap.IsEmpty() && Metadata)
		{
			FragmentAttrMap = PCGMassFragmentOverrides::DiscoverFragmentAttributes(Metadata);

			for (const TPair<FName, TArray<FName>>& Pair : FragmentAttrMap)
			{
				if (!ResolvedFragmentStructs.Contains(Pair.Key))
				{
					UScriptStruct* Resolved = PCGMassFragmentOverrides::ResolveFragmentStruct(Pair.Key, Context);
					ResolvedFragmentStructs.Add(Pair.Key, Resolved);
				}
			}
		}

		// Per-point config attribute
		const FPCGMetadataAttribute<FSoftObjectPath>* ConfigAttr = nullptr;
		if (!Settings->EntityConfigAttributeName.IsNone() && Metadata)
		{
			ConfigAttr = Metadata->GetConstTypedAttribute<FSoftObjectPath>(Settings->EntityConfigAttributeName);
		}

		TConstPCGValueRange<FTransform> PointTransforms = PointData->GetConstTransformValueRange();

		for (int32 Index = 0; Index < PointTransforms.Num(); ++Index)
		{
			// Resolve config for this point
			UMassEntityConfigAsset* ConfigAsset = nullptr;

			if (ConfigAttr)
			{
				const int32 MetadataEntry = PointData->GetMetadataEntry(Index);
				if (MetadataEntry != INDEX_NONE)
				{
					FSoftObjectPath ConfigPath = ConfigAttr->GetValue(MetadataEntry);
					if (!ConfigPath.IsNull())
					{
						ConfigAsset = Cast<UMassEntityConfigAsset>(ConfigPath.TryLoad());
					}
				}
			}

			if (!ConfigAsset)
			{
				ConfigAsset = Settings->DefaultEntityConfig;
			}

			if (!ConfigAsset)
			{
				continue;
			}

			// Resolve grid name (cached per config)
			FName GridName;
			FName* CachedGrid = ConfigToGridName.Find(ConfigAsset);
			if (CachedGrid)
			{
				GridName = *CachedGrid;
			}
			else
			{
				GridName = GetGridNameFromConfig(ConfigAsset);
				ConfigToGridName.Add(ConfigAsset, GridName);
			}

			const FTransform& PointTransform = PointTransforms[Index];

			// Find or create partition actor for this location + grid
			AArcPCGPlacedEntityPartitionActor* PartitionActor =
				FindOrCreatePartitionActor(World, PointTransform.GetLocation(), GridName);

			if (!PartitionActor)
			{
				continue;
			}

			// Group by {PartitionActor, Config} — correct per-cell splitting
			FPartitionConfigKey Key{PartitionActor, ConfigAsset};
			FArcPlacedEntityEntry& Entry = GroupedEntries.FindOrAdd(Key);
			if (!Entry.EntityConfig)
			{
				Entry.EntityConfig = ConfigAsset;
			}

			int32 InstanceIndex = Entry.InstanceTransforms.Num();
			Entry.InstanceTransforms.Add(PointTransform);

			// Assemble fragment overrides for this point
			if (!FragmentAttrMap.IsEmpty())
			{
				FArcPlacedEntityFragmentOverrides PointOverrides =
					PCGMassFragmentOverrides::AssembleOverridesForPoint(
						PointData, Index, FragmentAttrMap, ResolvedFragmentStructs, Context);

				if (PointOverrides.FragmentValues.Num() > 0)
				{
					Entry.PerInstanceOverrides.Add(InstanceIndex, MoveTemp(PointOverrides));
				}
			}
		}
	}

	// Assign entries to partition actors and collect unique actors
	TSet<AArcPCGPlacedEntityPartitionActor*> CreatedActors;
	for (TPair<FPartitionConfigKey, FArcPlacedEntityEntry>& Pair : GroupedEntries)
	{
		if (Pair.Value.InstanceTransforms.IsEmpty())
		{
			continue;
		}

		Pair.Key.PartitionActor->Entries.Add(MoveTemp(Pair.Value));
		Pair.Key.PartitionActor->Modify();
		CreatedActors.Add(Pair.Key.PartitionActor);
	}

	// Rebuild previews
	for (AArcPCGPlacedEntityPartitionActor* Actor : CreatedActors)
	{
		Actor->RebuildEditorPreviewISMCs();
	}

	// Create managed resource
	if (!CreatedActors.IsEmpty())
	{
		UPCGMassPlacedEntityManagedResource* Resource =
			FPCGContext::NewObject_AnyThread<UPCGMassPlacedEntityManagedResource>(Context, SourceComponent);

		for (AArcPCGPlacedEntityPartitionActor* Actor : CreatedActors)
		{
			Resource->TrackedActors.Add(Actor);
		}

		SourceComponent->AddToManagedResources(Resource);
	}
#else
	PCGLog::LogErrorOnGraph(
		LOCTEXT("EditorOnly", "Mass placed entities cannot be spawned in non-editor builds."),
		Context);
#endif
	return true;
}

#undef LOCTEXT_NAMESPACE
