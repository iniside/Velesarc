// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcFastGeoPartitionActor.h"
#include "MassEntityConfigAsset.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "FastGeoContainer.h"
#include "FastGeoInstancedStaticMeshComponent.h"
#include "FastGeoComponentCluster.h"
#include "Engine/StaticMesh.h"

#include "ArcMass/Visualization/ArcFastGeoVisualizationTrait.h"

class FInstancedStaticMeshComponentHelper
{
public:
	static void InitializeFromTransforms(
		FFastGeoInstancedStaticMeshComponent& Comp,
		UStaticMesh* InMesh,
		TArray<FTransform3f>&& InTransforms,
		const FBox& InWorldBounds,
		TArray<TObjectPtr<UMaterialInterface>>&& InMaterials)
	{
		Comp.SceneProxyDesc.StaticMesh = InMesh;
		Comp.SceneProxyDesc.bIsInstancedStaticMesh = true;
		Comp.bUseHighPrecisionPerInstanceSMData = false;
		Comp.LowPrecisionPerInstanceSMData = MoveTemp(InTransforms);
		Comp.OverrideMaterials = MoveTemp(InMaterials);
		Comp.LocalBounds = InWorldBounds.InverseTransformBy(Comp.WorldTransform);
		Comp.WorldBounds = InWorldBounds;
	}

	static FInstancedStaticMeshSceneProxyDesc& GetSceneProxyDesc(FFastGeoInstancedStaticMeshComponent& Comp)
	{
		return Comp.SceneProxyDesc;
	}
};

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcFastGeoPartitionActor)

AArcFastGeoPartitionActor::AArcFastGeoPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// -----------------------------------------------------------------------------
// Runtime
// -----------------------------------------------------------------------------

void AArcFastGeoPartitionActor::BeginPlay()
{
	Super::BeginPlay();
	CreateFastGeoContainer();
}

void AArcFastGeoPartitionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyFastGeoContainer();
	Super::EndPlay(EndPlayReason);
}

void AArcFastGeoPartitionActor::CreateFastGeoContainer()
{
	// Per-mesh rendering properties gathered from the visualization trait.
	struct FMeshGroupKey
	{
		UStaticMesh* Mesh;
		TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;
		bool bCastShadow;
		bool bCastShadowAsTwoSided;
		bool bCastHiddenShadow;
		bool bCastContactShadow;
		bool bReceivesDecals;
		bool bRenderCustomDepth;
		int32 CustomDepthStencilValue;
		int32 TranslucencySortPriority;
	};

	struct FMeshGroup
	{
		FMeshGroupKey Key;
		TArray<FTransform3f> InstanceTransforms;
		FBox WorldBounds{ ForceInit };
	};

	TArray<FMeshGroup> MeshGroups;

	const FTransform ActorTransform = GetActorTransform();

	for (const FArcPlacedEntityEntry& Entry : Entries)
	{
		if (Entry.EntityConfig == nullptr || Entry.InstanceTransforms.Num() == 0)
		{
			continue;
		}

		const UArcFastGeoVisualizationTrait* FastGeoTrait = Cast<UArcFastGeoVisualizationTrait>(
			Entry.EntityConfig->GetConfig().FindTrait(UArcFastGeoVisualizationTrait::StaticClass()));
		if (FastGeoTrait == nullptr || !FastGeoTrait->IsValid())
		{
			continue;
		}

		UStaticMesh* Mesh = FastGeoTrait->GetMesh();
		const FTransform& ComponentTransform = FastGeoTrait->GetComponentTransform();
		const FBox MeshLocalBounds = Mesh->GetBoundingBox();

		// Find or create a mesh group for this mesh pointer.
		// Separate groups per mesh so each FFastGeoInstancedStaticMeshComponent holds
		// instances of a single mesh type.
		FMeshGroup* Group = nullptr;
		for (FMeshGroup& Existing : MeshGroups)
		{
			if (Existing.Key.Mesh == Mesh)
			{
				Group = &Existing;
				break;
			}
		}

		if (Group == nullptr)
		{
			Group = &MeshGroups.AddDefaulted_GetRef();
			Group->Key.Mesh = Mesh;
			Group->Key.MaterialOverrides = FastGeoTrait->GetMaterialOverrides();
			Group->Key.bCastShadow = FastGeoTrait->GetCastShadow();
			Group->Key.bCastShadowAsTwoSided = FastGeoTrait->GetCastShadowAsTwoSided();
			Group->Key.bCastHiddenShadow = FastGeoTrait->GetCastHiddenShadow();
			Group->Key.bCastContactShadow = FastGeoTrait->GetCastContactShadow();
			Group->Key.bReceivesDecals = FastGeoTrait->GetReceivesDecals();
			Group->Key.bRenderCustomDepth = FastGeoTrait->GetRenderCustomDepth();
			Group->Key.CustomDepthStencilValue = FastGeoTrait->GetCustomDepthStencilValue();
			Group->Key.TranslucencySortPriority = FastGeoTrait->GetTranslucencySortPriority();
		}

		for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
		{
			// Instance transforms are local to the component's WorldTransform.
			// Since we leave WorldTransform at identity, these are world-space.
			const FTransform WorldTransform = ComponentTransform * InstanceTransform * ActorTransform;
			Group->InstanceTransforms.Emplace(FTransform3f(WorldTransform));
			Group->WorldBounds += MeshLocalBounds.TransformBy(WorldTransform);
		}
	}

	if (MeshGroups.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FFastGeoCreateRuntimeResult Result = UFastGeoContainer::CreateRuntime(
		World,
		*FString::Printf(TEXT("ArcFastGeo_%s"), *GetName()),
		[this, &MeshGroups](FFastGeoComponentCluster& InCluster)
		{
			for (FMeshGroup& Group : MeshGroups)
			{
				const FMeshGroupKey& Key = Group.Key;

				FFastGeoInstancedStaticMeshComponent& FastGeoComp = static_cast<FFastGeoInstancedStaticMeshComponent&>(
					InCluster.AddComponent(FFastGeoInstancedStaticMeshComponent::Type));

				TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides = Key.MaterialOverrides;
				FInstancedStaticMeshComponentHelper::InitializeFromTransforms(
					FastGeoComp,
					Key.Mesh,
					MoveTemp(Group.InstanceTransforms),
					Group.WorldBounds,
					MoveTemp(MaterialOverrides));

				FInstancedStaticMeshSceneProxyDesc& Desc = FInstancedStaticMeshComponentHelper::GetSceneProxyDesc(FastGeoComp);
				Desc.bCollisionEnabled = false;
				Desc.Mobility = EComponentMobility::Static;
				Desc.CastShadow = Key.bCastShadow;
				Desc.bCastShadowAsTwoSided = Key.bCastShadowAsTwoSided;
				Desc.bCastHiddenShadow = Key.bCastHiddenShadow;
				Desc.bCastContactShadow = Key.bCastContactShadow;
				Desc.bReceivesDecals = Key.bReceivesDecals;
				Desc.bRenderCustomDepth = Key.bRenderCustomDepth;
				Desc.CustomDepthStencilValue = Key.CustomDepthStencilValue;
				Desc.TranslucencySortPriority = Key.TranslucencySortPriority;
			}
		},
		/*bCollectReferences=*/false);

	FastGeoContainerPtr = Result.Container;
}

void AArcFastGeoPartitionActor::DestroyFastGeoContainer()
{
	if (FastGeoContainerPtr != nullptr)
	{
		UFastGeoContainer::DestroyRuntime(FastGeoContainerPtr);
		FastGeoContainerPtr = nullptr;
	}
}

void AArcFastGeoPartitionActor::BeginDestroy()
{
#if WITH_EDITOR
	FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEHandle);
#endif
	Super::BeginDestroy();
}

// -----------------------------------------------------------------------------
// ISMInstanceManager
// -----------------------------------------------------------------------------

FText AArcFastGeoPartitionActor::GetSMInstanceDisplayName(const FSMInstanceId& InstanceId) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return FText();
	}

	const UMassEntityConfigAsset* ConfigAsset = Entries[EntryIndex].EntityConfig;
	return FText::FromString(FString::Printf(TEXT("%s [%d]"), *GetNameSafe(ConfigAsset), TransformIndex));
#else
	return FText();
#endif
}

FText AArcFastGeoPartitionActor::GetSMInstanceTooltip(const FSMInstanceId& InstanceId) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return FText();
	}

	const UMassEntityConfigAsset* ConfigAsset = Entries[EntryIndex].EntityConfig;
	return FText::FromString(FString::Printf(TEXT("%s (Instance %d of %d)"),
		*GetPathNameSafe(ConfigAsset), TransformIndex, Entries[EntryIndex].InstanceTransforms.Num()));
#else
	return FText();
#endif
}

void AArcFastGeoPartitionActor::ForEachSMInstanceInSelectionGroup(const FSMInstanceId& InstanceId, TFunctionRef<bool(FSMInstanceId)> Callback)
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return;
	}

	// Single ISMC per entry — just callback with the same ID
	Callback(InstanceId);
#endif
}

bool AArcFastGeoPartitionActor::CanEditSMInstance(const FSMInstanceId& InstanceId) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	return ResolveInstanceId(InstanceId, EntryIndex, TransformIndex);
#else
	return false;
#endif
}

bool AArcFastGeoPartitionActor::CanMoveSMInstance(const FSMInstanceId& InstanceId, const ETypedElementWorldType WorldType) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	return ResolveInstanceId(InstanceId, EntryIndex, TransformIndex);
#else
	return false;
#endif
}

bool AArcFastGeoPartitionActor::GetSMInstanceTransform(const FSMInstanceId& InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return false;
	}

	OutInstanceTransform = Entries[EntryIndex].InstanceTransforms[TransformIndex];
	if (bWorldSpace)
	{
		OutInstanceTransform = OutInstanceTransform * GetActorTransform();
	}
	return true;
#else
	return false;
#endif
}

bool AArcFastGeoPartitionActor::SetSMInstanceTransform(const FSMInstanceId& InstanceId, const FTransform& InstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return false;
	}

	FTransform LocalTransform = InstanceTransform;
	if (bWorldSpace)
	{
		LocalTransform = InstanceTransform.GetRelativeTransform(GetActorTransform());
	}

	Entries[EntryIndex].InstanceTransforms[TransformIndex] = LocalTransform;

	// Update the ISMC for this entry
	if (EditorPreviewISMCs.IsValidIndex(EntryIndex))
	{
		UInstancedStaticMeshComponent* ISMComp = EditorPreviewISMCs[EntryIndex];
		if (ISMComp != nullptr)
		{
			// Retrieve the component-relative transform from the trait
			FTransform CompTransform = FTransform::Identity;
			if (Entries[EntryIndex].EntityConfig != nullptr)
			{
				const UArcFastGeoVisualizationTrait* FastGeoTrait = Cast<UArcFastGeoVisualizationTrait>(
					Entries[EntryIndex].EntityConfig->GetConfig().FindTrait(UArcFastGeoVisualizationTrait::StaticClass()));
				if (FastGeoTrait != nullptr)
				{
					CompTransform = FastGeoTrait->GetComponentTransform();
				}
			}

			FTransform FinalTransform = CompTransform * LocalTransform;
			ISMComp->UpdateInstanceTransform(TransformIndex, FinalTransform, /*bWorldSpace=*/false, bMarkRenderStateDirty, bTeleport);
		}
	}

	return true;
#else
	return false;
#endif
}

void AArcFastGeoPartitionActor::NotifySMInstanceMovementStarted(const FSMInstanceId& InstanceId)
{
	// No-op
}

void AArcFastGeoPartitionActor::NotifySMInstanceMovementOngoing(const FSMInstanceId& InstanceId)
{
	// No-op
}

void AArcFastGeoPartitionActor::NotifySMInstanceMovementEnded(const FSMInstanceId& InstanceId)
{
#if WITH_EDITOR
	Modify();
#endif
}

void AArcFastGeoPartitionActor::NotifySMInstanceSelectionChanged(const FSMInstanceId& InstanceId, const bool bIsSelected)
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return;
	}

	if (EditorPreviewISMCs.IsValidIndex(EntryIndex))
	{
		UInstancedStaticMeshComponent* ISMComp = EditorPreviewISMCs[EntryIndex];
		if (ISMComp != nullptr)
		{
			ISMComp->SelectInstance(bIsSelected, TransformIndex);
			ISMComp->MarkRenderStateDirty();
		}
	}
#endif
}

bool AArcFastGeoPartitionActor::DeleteSMInstances(TArrayView<const FSMInstanceId> InstanceIds)
{
#if WITH_EDITOR
	if (InstanceIds.Num() == 0)
	{
		return false;
	}

	// Pass 1: Resolve all instance IDs and group by entry index
	TMap<int32, TArray<int32>> TransformIndicesToRemoveByEntry;
	for (const FSMInstanceId& InstanceId : InstanceIds)
	{
		int32 EntryIndex = INDEX_NONE;
		int32 TransformIndex = INDEX_NONE;
		if (ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
		{
			TransformIndicesToRemoveByEntry.FindOrAdd(EntryIndex).AddUnique(TransformIndex);
		}
	}

	if (TransformIndicesToRemoveByEntry.IsEmpty())
	{
		return false;
	}

	Modify();

	// Pass 2: Remove transforms from back to front per entry
	for (TPair<int32, TArray<int32>>& Pair : TransformIndicesToRemoveByEntry)
	{
		TArray<int32>& IndicesToRemove = Pair.Value;
		IndicesToRemove.Sort(TGreater<int32>());

		FArcPlacedEntityEntry& Entry = Entries[Pair.Key];
		TArray<FTransform>& Transforms = Entry.InstanceTransforms;

		for (int32 TransformIndex : IndicesToRemove)
		{
			Entry.PerInstanceOverrides.Remove(TransformIndex);
			Transforms.RemoveAt(TransformIndex);
		}

		// Re-index remaining override keys
		TArray<TTuple<int32, FArcPlacedEntityFragmentOverrides>> ReindexedPairs;
		ReindexedPairs.Reserve(Entry.PerInstanceOverrides.Num());
		for (TPair<int32, FArcPlacedEntityFragmentOverrides>& OverridePair : Entry.PerInstanceOverrides)
		{
			int32 OriginalKey = OverridePair.Key;
			int32 Shift = 0;
			for (int32 RemovedIndex : IndicesToRemove)
			{
				if (RemovedIndex < OriginalKey)
				{
					++Shift;
				}
			}
			ReindexedPairs.Emplace(OriginalKey - Shift, MoveTemp(OverridePair.Value));
		}
		Entry.PerInstanceOverrides.Reset();
		for (TTuple<int32, FArcPlacedEntityFragmentOverrides>& ReindexedPair : ReindexedPairs)
		{
			Entry.PerInstanceOverrides.Emplace(ReindexedPair.Get<0>(), MoveTemp(ReindexedPair.Get<1>()));
		}
	}

	// Prune empty entries (iterate back to front)
	for (int32 EntryIndex = Entries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		if (Entries[EntryIndex].InstanceTransforms.Num() == 0)
		{
			Entries.RemoveAt(EntryIndex);
		}
	}

	// Self-destruct if no entries remain
	if (Entries.Num() == 0)
	{
		UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
		if (EditorActorSubsystem != nullptr)
		{
			EditorActorSubsystem->DestroyActor(this);
		}
		return true;
	}

	RebuildEditorPreviewISMCs();
	return true;
#else
	return false;
#endif
}

bool AArcFastGeoPartitionActor::DuplicateSMInstances(TArrayView<const FSMInstanceId> InstanceIds, TArray<FSMInstanceId>& OutNewInstanceIds)
{
#if WITH_EDITOR
	if (InstanceIds.Num() == 0)
	{
		return false;
	}

	// Pass 1: Resolve and deduplicate
	struct FSourceInstance
	{
		int32 EntryIndex;
		int32 TransformIndex;
	};
	TArray<FSourceInstance> UniqueSources;

	for (const FSMInstanceId& InstanceId : InstanceIds)
	{
		int32 EntryIndex = INDEX_NONE;
		int32 TransformIndex = INDEX_NONE;
		if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
		{
			continue;
		}

		bool bAlreadyPresent = false;
		for (const FSourceInstance& Existing : UniqueSources)
		{
			if (Existing.EntryIndex == EntryIndex && Existing.TransformIndex == TransformIndex)
			{
				bAlreadyPresent = true;
				break;
			}
		}

		if (!bAlreadyPresent)
		{
			UniqueSources.Add({ EntryIndex, TransformIndex });
		}
	}

	if (UniqueSources.IsEmpty())
	{
		return false;
	}

	Modify();

	// Pass 2: Copy transforms
	struct FNewInstance
	{
		int32 EntryIndex;
		int32 NewTransformIndex;
	};
	TArray<FNewInstance> NewInstances;
	NewInstances.Reserve(UniqueSources.Num());

	for (const FSourceInstance& Source : UniqueSources)
	{
		FArcPlacedEntityEntry& Entry = Entries[Source.EntryIndex];
		FTransform SourceTransform = Entry.InstanceTransforms[Source.TransformIndex];
		int32 NewIndex = Entry.InstanceTransforms.Add(SourceTransform);

		// Copy overrides if source instance has any
		const FArcPlacedEntityFragmentOverrides* SourceOverrides = Entry.PerInstanceOverrides.Find(Source.TransformIndex);
		if (SourceOverrides != nullptr)
		{
			Entry.PerInstanceOverrides.Add(NewIndex, *SourceOverrides);
		}

		NewInstances.Add({ Source.EntryIndex, NewIndex });
	}

	// Rebuild ISMCs to include new instances
	RebuildEditorPreviewISMCs();

	// Pass 3: Build output IDs
	OutNewInstanceIds.Reserve(NewInstances.Num());
	for (const FNewInstance& NewInst : NewInstances)
	{
		if (EditorPreviewISMCs.IsValidIndex(NewInst.EntryIndex))
		{
			UInstancedStaticMeshComponent* ISMComp = EditorPreviewISMCs[NewInst.EntryIndex];
			if (ISMComp != nullptr)
			{
				FSMInstanceId NewId;
				NewId.ISMComponent = ISMComp;
				NewId.InstanceIndex = NewInst.NewTransformIndex;
				OutNewInstanceIds.Add(NewId);
			}
		}
	}

	return OutNewInstanceIds.Num() > 0;
#else
	return false;
#endif
}

// -----------------------------------------------------------------------------
// ISMInstanceManagerProvider
// -----------------------------------------------------------------------------

ISMInstanceManager* AArcFastGeoPartitionActor::GetSMInstanceManager(const FSMInstanceId& InstanceId)
{
#if WITH_EDITORONLY_DATA
	UInstancedStaticMeshComponent* Component = InstanceId.ISMComponent;
	if (Component != nullptr)
	{
		if (EditorPreviewISMCs.Contains(Component))
		{
			return this;
		}
	}
#endif
	return nullptr;
}

// -----------------------------------------------------------------------------
// Editor
// -----------------------------------------------------------------------------

#if WITH_EDITOR

uint32 AArcFastGeoPartitionActor::GetDefaultGridSize(UWorld* InWorld) const
{
	return 6400;
}

void AArcFastGeoPartitionActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	UWorld* World = GetWorld();
	if (World != nullptr && !World->IsGameWorld())
	{
		RebuildEditorPreviewISMCs();

		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
		FEditorDelegates::EndPIE.Remove(EndPIEHandle);
		PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddUObject(this, &AArcFastGeoPartitionActor::OnPreBeginPIE);
		EndPIEHandle = FEditorDelegates::EndPIE.AddUObject(this, &AArcFastGeoPartitionActor::OnEndPIE);
	}
}

void AArcFastGeoPartitionActor::OnPreBeginPIE(const bool bIsSimulating)
{
	DestroyEditorPreviewISMCs();
}

void AArcFastGeoPartitionActor::OnEndPIE(const bool bIsSimulating)
{
	RebuildEditorPreviewISMCs();
}

void AArcFastGeoPartitionActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildEditorPreviewISMCs();
}

void AArcFastGeoPartitionActor::AddInstance(UMassEntityConfigAsset* ConfigAsset, const FTransform& Transform, int32& OutEntryIndex, int32& OutInstanceIndex)
{
	OutEntryIndex = INDEX_NONE;
	OutInstanceIndex = INDEX_NONE;

	if (ConfigAsset == nullptr)
	{
		return;
	}

	Modify();

	// Find existing entry for this config
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
		if (Entry.EntityConfig == ConfigAsset)
		{
			OutInstanceIndex = Entry.InstanceTransforms.Add(Transform);
			OutEntryIndex = EntryIndex;
			RebuildEditorPreviewISMCs();
			return;
		}
	}

	// Create new entry
	OutEntryIndex = Entries.Num();
	FArcPlacedEntityEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.EntityConfig = ConfigAsset;
	OutInstanceIndex = NewEntry.InstanceTransforms.Add(Transform);
	RebuildEditorPreviewISMCs();
}

void AArcFastGeoPartitionActor::RebuildEditorPreviewISMCs()
{
	DestroyEditorPreviewISMCs();

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FArcPlacedEntityEntry& Entry = Entries[EntryIndex];

		if (Entry.EntityConfig == nullptr)
		{
			EditorPreviewISMCs.Add(nullptr);
			continue;
		}

		const UArcFastGeoVisualizationTrait* FastGeoTrait = Cast<UArcFastGeoVisualizationTrait>(
			Entry.EntityConfig->GetConfig().FindTrait(UArcFastGeoVisualizationTrait::StaticClass()));
		if (FastGeoTrait == nullptr || !FastGeoTrait->IsValid())
		{
			EditorPreviewISMCs.Add(nullptr);
			continue;
		}

		UInstancedStaticMeshComponent* ISMComponent = NewObject<UInstancedStaticMeshComponent>(this);
		ISMComponent->SetStaticMesh(FastGeoTrait->GetMesh());
		ISMComponent->bIsEditorOnly = true;
		ISMComponent->bHasPerInstanceHitProxies = true;
		ISMComponent->SetCanEverAffectNavigation(false);

		const TArray<TObjectPtr<UMaterialInterface>>& MaterialOverrides = FastGeoTrait->GetMaterialOverrides();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialOverrides.Num(); ++MaterialIndex)
		{
			if (MaterialOverrides[MaterialIndex] != nullptr)
			{
				ISMComponent->SetMaterial(MaterialIndex, MaterialOverrides[MaterialIndex]);
			}
		}

		ISMComponent->SetupAttachment(GetRootComponent());
		ISMComponent->RegisterComponent();
		AddInstanceComponent(ISMComponent);

		const FTransform& CompTransform = FastGeoTrait->GetComponentTransform();
		for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
		{
			FTransform FinalTransform = CompTransform * InstanceTransform;
			ISMComponent->AddInstance(FinalTransform, /*bWorldSpace=*/false);
		}

		EditorPreviewISMCs.Add(ISMComponent);
	}
}

UInstancedStaticMeshComponent* AArcFastGeoPartitionActor::GetEditorPreviewISMC(int32 EntryIndex) const
{
	if (!EditorPreviewISMCs.IsValidIndex(EntryIndex))
	{
		return nullptr;
	}
	return EditorPreviewISMCs[EntryIndex];
}

void AArcFastGeoPartitionActor::DestroyEditorPreviewISMCs()
{
	for (UInstancedStaticMeshComponent* ISMComp : EditorPreviewISMCs)
	{
		if (ISMComp != nullptr)
		{
			ISMComp->DestroyComponent();
		}
	}
	EditorPreviewISMCs.Empty();
}

bool AArcFastGeoPartitionActor::ResolveInstanceId(const FSMInstanceId& InstanceId, int32& OutEntryIndex, int32& OutTransformIndex) const
{
	UInstancedStaticMeshComponent* Component = InstanceId.ISMComponent;
	if (Component == nullptr)
	{
		return false;
	}

	for (int32 Idx = 0; Idx < EditorPreviewISMCs.Num(); ++Idx)
	{
		if (EditorPreviewISMCs[Idx] == Component)
		{
			if (!Entries.IsValidIndex(Idx))
			{
				return false;
			}

			int32 InstanceIndex = InstanceId.InstanceIndex;
			if (!Entries[Idx].InstanceTransforms.IsValidIndex(InstanceIndex))
			{
				return false;
			}

			OutEntryIndex = Idx;
			OutTransformIndex = InstanceIndex;
			return true;
		}
	}

	return false;
}

#endif // WITH_EDITOR
