// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "Mass/EntityFragments.h"
#include "Components/InstancedStaticMeshComponent.h"

#if WITH_EDITOR
#include "ArcMass/Visualization/ArcMeshEntityVisualizationTrait.h"
#include "ArcMass/CompositeVisualization/ArcCompositeMeshVisualizationTrait.h"
#include "ArcMass/NiagaraVisualization/ArcNiagaraVisTrait.h"
#include "NiagaraComponent.h"
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPlacedEntityPartitionActor)

AArcPlacedEntityPartitionActor::AArcPlacedEntityPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// -----------------------------------------------------------------------------
// ISMInstanceManager
// -----------------------------------------------------------------------------

bool AArcPlacedEntityPartitionActor::CanEditSMInstance(const FSMInstanceId& InstanceId) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	return ResolveInstanceId(InstanceId, EntryIndex, TransformIndex);
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::CanMoveSMInstance(const FSMInstanceId& InstanceId, const ETypedElementWorldType WorldType) const
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	return ResolveInstanceId(InstanceId, EntryIndex, TransformIndex);
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::GetSMInstanceTransform(const FSMInstanceId& InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace) const
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

bool AArcPlacedEntityPartitionActor::SetSMInstanceTransform(const FSMInstanceId& InstanceId, const FTransform& InstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
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

	if (EditorPreviewGroups.IsValidIndex(EntryIndex))
	{
		const FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups[EntryIndex];
		for (int32 CompIndex = 0; CompIndex < Group.ISMComponents.Num(); ++CompIndex)
		{
			UInstancedStaticMeshComponent* ISMComp = Group.ISMComponents[CompIndex];
			if (ISMComp == nullptr)
			{
				continue;
			}
			FTransform FinalTransform = Group.ComponentRelativeTransforms[CompIndex] * LocalTransform;
			ISMComp->UpdateInstanceTransform(TransformIndex, FinalTransform, /*bWorldSpace=*/false, bMarkRenderStateDirty, bTeleport);
		}

		if (Group.NiagaraComponents.IsValidIndex(TransformIndex))
		{
			UNiagaraComponent* NiagaraComp = Group.NiagaraComponents[TransformIndex];
			if (NiagaraComp != nullptr)
			{
				FTransform NiagaraFinalTransform = Group.NiagaraComponentTransform * LocalTransform;
				NiagaraComp->SetRelativeTransform(NiagaraFinalTransform);
			}
		}
	}

	return true;
#else
	return false;
#endif
}

void AArcPlacedEntityPartitionActor::NotifySMInstanceMovementStarted(const FSMInstanceId& InstanceId)
{
	// No-op
}

void AArcPlacedEntityPartitionActor::NotifySMInstanceMovementOngoing(const FSMInstanceId& InstanceId)
{
	// No-op
}

void AArcPlacedEntityPartitionActor::NotifySMInstanceMovementEnded(const FSMInstanceId& InstanceId)
{
#if WITH_EDITOR
	Modify();
#endif
}

void AArcPlacedEntityPartitionActor::NotifySMInstanceSelectionChanged(const FSMInstanceId& InstanceId, const bool bIsSelected)
{
#if WITH_EDITOR
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
	{
		return;
	}

	if (EditorPreviewGroups.IsValidIndex(EntryIndex))
	{
		const FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups[EntryIndex];
		for (UInstancedStaticMeshComponent* ISMComp : Group.ISMComponents)
		{
			if (ISMComp != nullptr)
			{
				ISMComp->SelectInstance(bIsSelected, TransformIndex);
				ISMComp->MarkRenderStateDirty();
			}
		}
	}
#endif
}

bool AArcPlacedEntityPartitionActor::DeleteSMInstances(TArrayView<const FSMInstanceId> InstanceIds)
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

		TArray<FTransform>& Transforms = Entries[Pair.Key].InstanceTransforms;
		for (int32 TransformIndex : IndicesToRemove)
		{
			Transforms.RemoveAt(TransformIndex);
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

	// Rebuild ISMCs to reflect remaining instances
	RebuildEditorPreviewISMCs();
	return true;
#else
	return false;
#endif
}

bool AArcPlacedEntityPartitionActor::DuplicateSMInstances(TArrayView<const FSMInstanceId> InstanceIds, TArray<FSMInstanceId>& OutNewInstanceIds)
{
	// Not supported in v1
	return false;
}

// -----------------------------------------------------------------------------
// ISMInstanceManagerProvider
// -----------------------------------------------------------------------------

ISMInstanceManager* AArcPlacedEntityPartitionActor::GetSMInstanceManager(const FSMInstanceId& InstanceId)
{
#if WITH_EDITORONLY_DATA
	UInstancedStaticMeshComponent* Component = InstanceId.ISMComponent;
	if (Component != nullptr)
	{
		for (const FArcPlacedEntityPreviewGroup& Group : EditorPreviewGroups)
		{
			if (Group.ISMComponents.Contains(Component))
			{
				return this;
			}
		}
	}
#endif
	return nullptr;
}

// -----------------------------------------------------------------------------
// Runtime Entity Spawning
// -----------------------------------------------------------------------------

void AArcPlacedEntityPartitionActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnEntities();
}

void AArcPlacedEntityPartitionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DespawnEntities();
	Super::EndPlay(EndPlayReason);
}

void AArcPlacedEntityPartitionActor::SpawnEntities()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem == nullptr)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Count total entities to reserve
	int32 TotalCount = 0;
	for (const FArcPlacedEntityEntry& Entry : Entries)
	{
		if (Entry.EntityConfig != nullptr)
		{
			TotalCount += Entry.InstanceTransforms.Num();
		}
	}

	if (TotalCount == 0)
	{
		return;
	}

	SpawnedEntities.Reserve(TotalCount);

	// Single creation context across all entries — observers fire once when it drops
	TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager.GetOrMakeCreationContext();

	for (const FArcPlacedEntityEntry& Entry : Entries)
	{
		if (Entry.EntityConfig == nullptr || Entry.InstanceTransforms.Num() == 0)
		{
			continue;
		}

		const FMassEntityTemplate& Template = Entry.EntityConfig->GetOrCreateEntityTemplate(*World);
		const FMassArchetypeHandle& Archetype = Template.GetArchetype();
		if (!Archetype.IsValid())
		{
			UE_LOG(LogMass, Warning, TEXT("AArcPlacedEntityPartitionActor::SpawnEntities: Invalid archetype for config '%s'"),
				*GetNameSafe(Entry.EntityConfig));
			continue;
		}

		const FMassArchetypeSharedFragmentValues& SharedValues = Template.GetSharedFragmentValues();
		const int32 Count = Entry.InstanceTransforms.Num();

		TArray<FMassEntityHandle> EntryEntities;
		EntityManager.BatchCreateEntities(Archetype, SharedValues, Count, EntryEntities);

		TConstArrayView<FInstancedStruct> InitialFragmentValues = Template.GetInitialFragmentValues();

		for (int32 i = 0; i < Count; ++i)
		{
			FTransformFragment& TransformFragment = EntityManager.GetFragmentDataChecked<FTransformFragment>(EntryEntities[i]);
			TransformFragment.SetTransform(Entry.InstanceTransforms[i] * GetActorTransform());

			for (const FInstancedStruct& FragmentValue : InitialFragmentValues)
			{
				const UScriptStruct* FragmentType = FragmentValue.GetScriptStruct();
				if (FragmentType != nullptr && FragmentType != FTransformFragment::StaticStruct())
				{
					FStructView TargetFragment = EntityManager.GetFragmentDataStruct(EntryEntities[i], FragmentType);
					if (TargetFragment.IsValid())
					{
						FragmentType->CopyScriptStruct(TargetFragment.GetMemory(), FragmentValue.GetMemory());
					}
				}
			}
		}

		SpawnedEntities.Append(EntryEntities);
	}

	// CreationContext drops here — all observers fire once
}

void AArcPlacedEntityPartitionActor::DespawnEntities()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (EntitySubsystem == nullptr)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	TArray<FMassEntityHandle> ValidEntities;
	ValidEntities.Reserve(SpawnedEntities.Num());
	for (const FMassEntityHandle& EntityHandle : SpawnedEntities)
	{
		if (EntityManager.IsEntityValid(EntityHandle))
		{
			ValidEntities.Add(EntityHandle);
		}
	}

	if (ValidEntities.Num() > 0)
	{
		EntityManager.BatchDestroyEntities(MakeArrayView(ValidEntities));
	}

	SpawnedEntities.Reset();
}

void AArcPlacedEntityPartitionActor::BeginDestroy()
{
#if WITH_EDITOR
	FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEHandle);
#endif
	Super::BeginDestroy();
}

// -----------------------------------------------------------------------------
// Editor
// -----------------------------------------------------------------------------

#if WITH_EDITOR

uint32 AArcPlacedEntityPartitionActor::GetDefaultGridSize(UWorld* InWorld) const
{
	return 25600;
}

void AArcPlacedEntityPartitionActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	UWorld* World = GetWorld();
	if (World != nullptr && !World->IsGameWorld())
	{
		RebuildEditorPreviewISMCs();

		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
		FEditorDelegates::EndPIE.Remove(EndPIEHandle);
		PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddUObject(this, &AArcPlacedEntityPartitionActor::OnPreBeginPIE);
		EndPIEHandle = FEditorDelegates::EndPIE.AddUObject(this, &AArcPlacedEntityPartitionActor::OnEndPIE);
	}
}

void AArcPlacedEntityPartitionActor::OnPreBeginPIE(const bool bIsSimulating)
{
	DestroyEditorPreviewISMCs();
}

void AArcPlacedEntityPartitionActor::OnEndPIE(const bool bIsSimulating)
{
	RebuildEditorPreviewISMCs();
}

void AArcPlacedEntityPartitionActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildEditorPreviewISMCs();
}

void AArcPlacedEntityPartitionActor::AddInstance(UMassEntityConfigAsset* ConfigAsset, const FTransform& Transform, int32& OutEntryIndex, int32& OutInstanceIndex)
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

UInstancedStaticMeshComponent* AArcPlacedEntityPartitionActor::GetEditorPreviewISMC(int32 EntryIndex) const
{
	if (!EditorPreviewGroups.IsValidIndex(EntryIndex) || EditorPreviewGroups[EntryIndex].ISMComponents.Num() == 0)
	{
		return nullptr;
	}
	return EditorPreviewGroups[EntryIndex].ISMComponents[0];
}

void AArcPlacedEntityPartitionActor::RebuildEditorPreviewISMCs()
{
	if (bIsRebuildingPreviewISMCs)
	{
		return;
	}
	TGuardValue<bool> RebuildGuard(bIsRebuildingPreviewISMCs, true);

	DestroyEditorPreviewISMCs();

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
		FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups.AddDefaulted_GetRef();

		if (Entry.EntityConfig == nullptr)
		{
			continue;
		}

		// Try single-mesh trait
		const UArcMeshEntityVisualizationTrait* SingleMeshTrait = Cast<UArcMeshEntityVisualizationTrait>(
			Entry.EntityConfig->GetConfig().FindTrait(UArcMeshEntityVisualizationTrait::StaticClass()));
		if (SingleMeshTrait != nullptr && SingleMeshTrait->IsValid())
		{
			UInstancedStaticMeshComponent* ISMComponent = NewObject<UInstancedStaticMeshComponent>(this);
			ISMComponent->SetStaticMesh(SingleMeshTrait->GetMesh());
			ISMComponent->bIsEditorOnly = true;
			ISMComponent->bHasPerInstanceHitProxies = true;
			ISMComponent->SetCanEverAffectNavigation(false);

			const TArray<TObjectPtr<UMaterialInterface>>& MaterialOverrides = SingleMeshTrait->GetMaterialOverrides();
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

			const FTransform& CompTransform = SingleMeshTrait->GetComponentTransform();
			for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
			{
				FTransform FinalTransform = CompTransform * InstanceTransform;
				ISMComponent->AddInstance(FinalTransform, /*bWorldSpace=*/false);
			}

			Group.ISMComponents.Add(ISMComponent);
			Group.ComponentRelativeTransforms.Add(CompTransform);
		}

		// Try composite mesh trait
		const UArcCompositeMeshVisualizationTrait* CompositeTrait = Cast<UArcCompositeMeshVisualizationTrait>(
			Entry.EntityConfig->GetConfig().FindTrait(UArcCompositeMeshVisualizationTrait::StaticClass()));
		if (CompositeTrait != nullptr && CompositeTrait->IsValid())
		{
			const TArray<FArcCompositeMeshEntry>& MeshEntries = CompositeTrait->GetMeshEntries();
			for (int32 MeshIndex = 0; MeshIndex < MeshEntries.Num(); ++MeshIndex)
			{
				const FArcCompositeMeshEntry& MeshEntry = MeshEntries[MeshIndex];
				if (MeshEntry.Mesh == nullptr)
				{
					continue;
				}

				UInstancedStaticMeshComponent* ISMComponent = NewObject<UInstancedStaticMeshComponent>(this);
				ISMComponent->SetStaticMesh(MeshEntry.Mesh);
				ISMComponent->bIsEditorOnly = true;
				ISMComponent->bHasPerInstanceHitProxies = true;
				ISMComponent->SetCanEverAffectNavigation(false);

				for (int32 MaterialIndex = 0; MaterialIndex < MeshEntry.MaterialOverrides.Num(); ++MaterialIndex)
				{
					if (MeshEntry.MaterialOverrides[MaterialIndex] != nullptr)
					{
						ISMComponent->SetMaterial(MaterialIndex, MeshEntry.MaterialOverrides[MaterialIndex]);
					}
				}

				ISMComponent->SetupAttachment(GetRootComponent());
				ISMComponent->RegisterComponent();
				AddInstanceComponent(ISMComponent);

				const FTransform& MeshRelativeTransform = MeshEntry.RelativeTransform;
				for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
				{
					FTransform FinalTransform = MeshRelativeTransform * InstanceTransform;
					ISMComponent->AddInstance(FinalTransform, /*bWorldSpace=*/false);
				}

				Group.ISMComponents.Add(ISMComponent);
				Group.ComponentRelativeTransforms.Add(MeshRelativeTransform);
			}
		}

		// Try Niagara visualization trait
		const UArcNiagaraVisTrait* NiagaraTrait = Cast<UArcNiagaraVisTrait>(
			Entry.EntityConfig->GetConfig().FindTrait(UArcNiagaraVisTrait::StaticClass()));
		if (NiagaraTrait != nullptr && NiagaraTrait->IsValid())
		{
			const FArcNiagaraVisConfigFragment& NiagaraConfig = NiagaraTrait->GetNiagaraConfig();
			UNiagaraSystem* NiagaraSystem = NiagaraConfig.NiagaraSystem.LoadSynchronous();
			if (NiagaraSystem != nullptr)
			{
				const FTransform& ComponentTransform = NiagaraTrait->GetComponentTransform();
				Group.NiagaraComponentTransform = ComponentTransform;

				for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
				{
					UNiagaraComponent* NiagaraComp = NewObject<UNiagaraComponent>(this);
					NiagaraComp->bIsEditorOnly = true;
					NiagaraComp->SetAsset(NiagaraSystem);
					NiagaraComp->SetupAttachment(GetRootComponent());
					FTransform FinalTransform = ComponentTransform * InstanceTransform;
					NiagaraComp->SetRelativeTransform(FinalTransform);
					NiagaraComp->RegisterComponent();
					NiagaraComp->Activate(true);

					Group.NiagaraComponents.Add(NiagaraComp);
				}
			}
		}
	}
}

void AArcPlacedEntityPartitionActor::DestroyEditorPreviewISMCs()
{
	for (FArcPlacedEntityPreviewGroup& Group : EditorPreviewGroups)
	{
		for (UNiagaraComponent* NiagaraComp : Group.NiagaraComponents)
		{
			if (NiagaraComp != nullptr)
			{
				NiagaraComp->Deactivate();
				NiagaraComp->DestroyComponent();
			}
		}
		Group.NiagaraComponents.Empty();
	}

	TArray<UInstancedStaticMeshComponent*> ISMComponents;
	GetComponents(ISMComponents);
	for (UInstancedStaticMeshComponent* ISMComponent : ISMComponents)
	{
		ISMComponent->DestroyComponent();
	}

	EditorPreviewGroups.Empty();
}

bool AArcPlacedEntityPartitionActor::ResolveInstanceId(const FSMInstanceId& InstanceId, int32& OutEntryIndex, int32& OutTransformIndex) const
{
	UInstancedStaticMeshComponent* Component = InstanceId.ISMComponent;
	if (Component == nullptr)
	{
		return false;
	}

	for (int32 GroupIndex = 0; GroupIndex < EditorPreviewGroups.Num(); ++GroupIndex)
	{
		const FArcPlacedEntityPreviewGroup& Group = EditorPreviewGroups[GroupIndex];
		for (const TObjectPtr<UInstancedStaticMeshComponent>& ISMComp : Group.ISMComponents)
		{
			if (ISMComp == Component)
			{
				if (!Entries.IsValidIndex(GroupIndex))
				{
					return false;
				}

				int32 InstanceIndex = InstanceId.InstanceIndex;
				if (!Entries[GroupIndex].InstanceTransforms.IsValidIndex(InstanceIndex))
				{
					return false;
				}

				OutEntryIndex = GroupIndex;
				OutTransformIndex = InstanceIndex;
				return true;
			}
		}
	}

	return false;
}

#endif // WITH_EDITOR
