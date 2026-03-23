// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWPartitionActor.h"

#include "MassClientBubbleHandler.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityFragments.h"
#include "ArcIWISMComponent.h"
#include "MassObserverManager.h"
#include "ArcIWActorPoolSubsystem.h"
#include "ArcIWSettings.h"
#include "Engine/ActorInstanceHandle.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionLHGrid.h"
#include "ArcMass/ArcMassPhysicsLink.h"
#include "ArcMass/ArcMassPhysicsEntityLink.h"
#include "Chaos/ChaosUserEntity.h"

// Template hack to access private RuntimePartitions on UWorldPartitionRuntimeHashSet.
// UWorldPartitionRuntimeHashSet doesn't expose a public getter for its partition descriptors.
template<typename Tag, typename Tag::type MemberPtr>
struct FPrivateAccessor
{
	friend typename Tag::type GetPrivateMember(Tag) { return MemberPtr; }
};

struct FRuntimePartitionsAccess
{
	using type = TArray<FRuntimePartitionDesc> UWorldPartitionRuntimeHashSet::*;
	friend type GetPrivateMember(FRuntimePartitionsAccess);
};

template struct FPrivateAccessor<FRuntimePartitionsAccess, &UWorldPartitionRuntimeHashSet::RuntimePartitions>;

AArcIWPartitionActor::AArcIWPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

uint32 AArcIWPartitionActor::GetGridCellSize(UWorld* InWorld, FName InGridName)
{
	if (InWorld)
	{
		UWorldPartition* WorldPartition = InWorld->GetWorldPartition();
		if (WorldPartition)
		{
			// Path 1: UWorldPartitionRuntimeSpatialHash (legacy/simple WP setup)
			UWorldPartitionRuntimeSpatialHash* SpatialHash = Cast<UWorldPartitionRuntimeSpatialHash>(WorldPartition->RuntimeHash);
			if (SpatialHash)
			{
				const FSpatialHashStreamingGrid* StreamingGrid = SpatialHash->GetStreamingGridByName(InGridName);
				if (StreamingGrid)
				{
					return static_cast<uint32>(StreamingGrid->CellSize);
				}
			}

			// Path 2: UWorldPartitionRuntimeHashSet (new WP setup with runtime partitions)
			UWorldPartitionRuntimeHashSet* HashSet = Cast<UWorldPartitionRuntimeHashSet>(WorldPartition->RuntimeHash);
			if (HashSet)
			{
				const TArray<FRuntimePartitionDesc>& Partitions = HashSet->*GetPrivateMember(FRuntimePartitionsAccess{});
				for (const FRuntimePartitionDesc& Desc : Partitions)
				{
					if (Desc.Name == InGridName && Desc.MainLayer)
					{
						URuntimePartitionLHGrid* LHGrid = Cast<URuntimePartitionLHGrid>(Desc.MainLayer);
						if (LHGrid)
						{
							return LHGrid->GetCellSize();
						}
					}
				}
			}
		}
	}

	// Fallback to project settings
	return UArcIWSettings::Get()->DefaultGridCellSize;
}

void AArcIWPartitionActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (World && !World->IsGameWorld() && EditorPreviewISMCs.IsEmpty())
	{
		RebuildEditorPreviewISMCs();
	}
#endif
}

void AArcIWPartitionActor::BeginPlay()
{
	Super::BeginPlay();

#if WITH_EDITOR
	// Strip editor preview ISMCs in game worlds — runtime uses Mass processors
	DestroyEditorPreviewISMCs();
#endif

	SpawnEntities();
}

void AArcIWPartitionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DespawnEntities();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void AArcIWPartitionActor::AddActorInstance(
	TSubclassOf<AActor> ActorClass,
	const FTransform& WorldTransform,
	const TArray<FArcIWMeshEntry>& MeshDescriptors,
	UMassEntityConfigAsset* InAdditionalConfig)
{
	// Find existing entry for this actor class.
	FArcIWActorClassData* FoundEntry = nullptr;
	for (FArcIWActorClassData& Entry : ActorClassEntries)
	{
		if (Entry.ActorClass == ActorClass)
		{
			FoundEntry = &Entry;
			break;
		}
	}

	if (FoundEntry == nullptr)
	{
		FArcIWActorClassData& NewEntry = ActorClassEntries.AddDefaulted_GetRef();
		NewEntry.ActorClass = ActorClass;
		NewEntry.MeshDescriptors = MeshDescriptors;
		NewEntry.AdditionalEntityConfig = InAdditionalConfig;
		FoundEntry = &NewEntry;
	}

	FoundEntry->InstanceTransforms.Add(WorldTransform);

	Modify();
}

void AArcIWPartitionActor::RebuildEditorPreviewISMCs()
{
	DestroyEditorPreviewISMCs();

	for (const FArcIWActorClassData& ClassData : ActorClassEntries)
	{
		// Build a map of mesh -> ISM component
		TMap<FObjectKey, UInstancedStaticMeshComponent*> MeshToISMC;

		for (const FArcIWMeshEntry& MeshEntry : ClassData.MeshDescriptors)
		{
			if (!MeshEntry.Mesh)
			{
				continue;
			}

			const FObjectKey MeshKey(MeshEntry.Mesh);
			if (!MeshToISMC.Contains(MeshKey))
			{
				UInstancedStaticMeshComponent* NewISMC = NewObject<UInstancedStaticMeshComponent>(this);
				NewISMC->SetStaticMesh(MeshEntry.Mesh);
				NewISMC->SetCastShadow(MeshEntry.bCastShadows);
				NewISMC->SetMobility(EComponentMobility::Static);
				NewISMC->bIsEditorOnly = true;
				NewISMC->SetAbsolute(true, true, true);

				for (int32 MatIdx = 0; MatIdx < MeshEntry.Materials.Num(); ++MatIdx)
				{
					if (MeshEntry.Materials[MatIdx])
					{
						NewISMC->SetMaterial(MatIdx, MeshEntry.Materials[MatIdx]);
					}
				}

				NewISMC->RegisterComponent();
				NewISMC->SetWorldTransform(FTransform::Identity);
				AddInstanceComponent(NewISMC);

				MeshToISMC.Add(MeshKey, NewISMC);
				EditorPreviewISMCs.Add(NewISMC);
			}
		}

		// Add instances for each transform
		for (const FTransform& InstanceTransform : ClassData.InstanceTransforms)
		{
			for (const FArcIWMeshEntry& MeshEntry : ClassData.MeshDescriptors)
			{
				if (!MeshEntry.Mesh)
				{
					continue;
				}

				const FObjectKey MeshKey(MeshEntry.Mesh);
				UInstancedStaticMeshComponent** FoundISMC = MeshToISMC.Find(MeshKey);
				if (FoundISMC && *FoundISMC)
				{
					const FTransform FinalTransform = MeshEntry.RelativeTransform * InstanceTransform;
					(*FoundISMC)->AddInstance(FinalTransform, /*bWorldSpace=*/true);
				}
			}
		}
	}
}

void AArcIWPartitionActor::DestroyEditorPreviewISMCs()
{
	for (UInstancedStaticMeshComponent* ISMC : EditorPreviewISMCs)
	{
		if (IsValid(ISMC))
		{
			RemoveInstanceComponent(ISMC);
			ISMC->DestroyComponent();
		}
	}
	EditorPreviewISMCs.Empty();
}
#endif

void AArcIWPartitionActor::SpawnEntities()
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

	// -----------------------------------------------------------------------
	// Phase A: Pre-build MeshSlots for ALL classes (even those with 0 transforms).
	// This must visit every class to keep MeshSlotBase offsets correct.
	// -----------------------------------------------------------------------

	int32 TotalMeshSlots = 0;
	int32 TotalEntities = 0;
	for (const FArcIWActorClassData& ClassData : ActorClassEntries)
	{
		TotalMeshSlots += ClassData.MeshDescriptors.Num();
		TotalEntities += ClassData.InstanceTransforms.Num();
	}

	MeshSlots.SetNum(TotalMeshSlots);
	SpawnedEntities.Reserve(TotalEntities);

	// Per-class MeshSlotBase offsets.
	TArray<int32> ClassMeshSlotBases;
	ClassMeshSlotBases.SetNum(ActorClassEntries.Num());

	int32 SlotIdx = 0;
	for (int32 ClassIndex = 0; ClassIndex < ActorClassEntries.Num(); ++ClassIndex)
	{
		const FArcIWActorClassData& ClassData = ActorClassEntries[ClassIndex];
		ClassMeshSlotBases[ClassIndex] = SlotIdx;

		for (const FArcIWMeshEntry& MeshEntry : ClassData.MeshDescriptors)
		{
			if (MeshEntry.Mesh)
			{
				UArcIWISMComponent* NewISMC = NewObject<UArcIWISMComponent>(this);
				NewISMC->SetStaticMesh(MeshEntry.Mesh);
				NewISMC->SetCastShadow(MeshEntry.bCastShadows);
				NewISMC->SetMobility(EComponentMobility::Stationary);
				NewISMC->SetAbsolute(true, true, true);

				for (int32 MatIdx = 0; MatIdx < MeshEntry.Materials.Num(); ++MatIdx)
				{
					if (MeshEntry.Materials[MatIdx])
					{
						NewISMC->SetMaterial(MatIdx, MeshEntry.Materials[MatIdx]);
					}
				}

				NewISMC->MeshSlotIndex = SlotIdx;
				NewISMC->SpawnedEntities = &SpawnedEntities;
				NewISMC->Owners.Reserve(ClassData.InstanceTransforms.Num());
				NewISMC->RegisterComponent();
				NewISMC->SetWorldTransform(FTransform::Identity);
				AddInstanceComponent(NewISMC);

				MeshSlots[SlotIdx] = NewISMC;
			}

			++SlotIdx;
		}
	}

	// -----------------------------------------------------------------------
	// Phase B: Batch-create entities (skip classes with 0 transforms).
	// -----------------------------------------------------------------------

	for (int32 ClassIndex = 0; ClassIndex < ActorClassEntries.Num(); ++ClassIndex)
	{
		const FArcIWActorClassData& ClassData = ActorClassEntries[ClassIndex];
		const int32 InstanceCount = ClassData.InstanceTransforms.Num();
		if (InstanceCount == 0)
		{
			continue;
		}

		// Build entity template.
		FMassEntityTemplateData TemplateData;

		if (ClassData.AdditionalEntityConfig != nullptr)
		{
			FMassEntityTemplateBuildContext BuildContext(TemplateData);
			const FMassEntityConfig& Config = ClassData.AdditionalEntityConfig->GetConfig();
			BuildContext.BuildFromTraits(Config.GetTraits(), *World);
		}

		TemplateData.AddTag<FArcIWEntityTag>();
		TemplateData.AddFragment<FArcIWInstanceFragment>();
		TemplateData.AddFragment<FTransformFragment>();
		TemplateData.AddFragment<FArcMassPhysicsLinkFragment>();
		TemplateData.AddTag<FArcMassPhysicsLinkTag>();

		FArcIWVisConfigFragment ConfigFragment;
		ConfigFragment.ActorClass = ClassData.ActorClass;
		ConfigFragment.MeshDescriptors = ClassData.MeshDescriptors;

		FConstSharedStruct SharedConfigFragment = FConstSharedStruct::Make(ConfigFragment);
		TemplateData.AddConstSharedFragment(SharedConfigFragment);

		FGuid ClassGuid = FGuid::NewDeterministicGuid(ClassData.ActorClass->GetPathName());
		FMassEntityTemplateID TemplateID = FMassEntityTemplateIDFactory::Make(ClassGuid);

		TSharedRef<FMassEntityTemplate> FinalTemplate = FMassEntityTemplate::MakeFinalTemplate(
			EntityManager, MoveTemp(TemplateData), TemplateID);

		const FMassArchetypeHandle& Archetype = FinalTemplate->GetArchetype();
		const FMassArchetypeSharedFragmentValues& SharedValues = FinalTemplate->GetSharedFragmentValues();
		TArray<FMassEntityHandle> ClassEntities;
		TSharedRef<FMassObserverManager::FCreationContext> CreationContext = EntityManager.BatchCreateEntities(
			Archetype, SharedValues, InstanceCount, ClassEntities);

		const int32 BaseIndex = SpawnedEntities.Num();
		const int32 ClassMeshSlotBase = ClassMeshSlotBases[ClassIndex];

		for (int32 i = 0; i < InstanceCount; ++i)
		{
			FTransformFragment& TransformFragment = EntityManager.GetFragmentDataChecked<FTransformFragment>(ClassEntities[i]);
			TransformFragment.SetTransform(ClassData.InstanceTransforms[i]);

			FArcIWInstanceFragment& InstanceFragment = EntityManager.GetFragmentDataChecked<FArcIWInstanceFragment>(ClassEntities[i]);
			InstanceFragment.InstanceIndex = BaseIndex + i;
			InstanceFragment.MeshSlotBase = ClassMeshSlotBase;
			InstanceFragment.PartitionActor = this;
		}

		SpawnedEntities.Append(ClassEntities);

		// CreationContext going out of scope will notify observers.
	}
}

void AArcIWPartitionActor::DespawnEntities()
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
	for (FMassEntityHandle EntityHandle : SpawnedEntities)
	{
		if (EntityManager.IsEntityValid(EntityHandle))
		{
			ValidEntities.Add(EntityHandle);
		}
	}
	// Destroy ISM components BEFORE destroying entities.
	// Mass deinit observers are deferred — if MeshSlots is cleared after BatchDestroyEntities,
	// the observer would access an empty array. By clearing first, the deinit observer
	// sees null partition actor ISM slots and safely skips ISM cleanup.
	for (UArcIWISMComponent* ISMC : MeshSlots)
	{
		if (ISMC)
		{
			ISMC->DestroyComponent();
		}
	}
	MeshSlots.Reset();

	EntityManager.BatchDestroyEntities(MakeArrayView(ValidEntities));
	SpawnedEntities.Reset();
}

// ---------------------------------------------------------------------------
// ISM Management
// ---------------------------------------------------------------------------

void AArcIWPartitionActor::AddCompositeISMInstances(
	int32 MeshSlotBase,
	const FTransform& WorldTransform,
	const TArray<FArcIWMeshEntry>& MeshDescriptors,
	TArray<int32>& InOutISMInstanceIds,
	int32 InInstanceIndex)
{
	InOutISMInstanceIds.SetNum(MeshDescriptors.Num());

	for (int32 MeshIdx = 0; MeshIdx < MeshDescriptors.Num(); ++MeshIdx)
	{
		if (!MeshDescriptors[MeshIdx].Mesh)
		{
			InOutISMInstanceIds[MeshIdx] = INDEX_NONE;
			continue;
		}

		const int32 SlotIdx = MeshSlotBase + MeshIdx;
		if (!MeshSlots.IsValidIndex(SlotIdx) || !MeshSlots[SlotIdx])
		{
			InOutISMInstanceIds[MeshIdx] = INDEX_NONE;
			continue;
		}

		const FTransform InstanceTransform = MeshDescriptors[MeshIdx].RelativeTransform * WorldTransform;
		InOutISMInstanceIds[MeshIdx] = MeshSlots[SlotIdx]->AddTrackedInstance(InstanceTransform, MeshIdx, InInstanceIndex);
	}
}

void AArcIWPartitionActor::RemoveCompositeISMInstances(
	int32 MeshSlotBase,
	const TArray<FArcIWMeshEntry>& MeshDescriptors,
	const TArray<int32>& ISMInstanceIds,
	FMassEntityManager& EntityManager)
{
	for (int32 MeshIdx = 0; MeshIdx < MeshDescriptors.Num(); ++MeshIdx)
	{
		if (MeshIdx >= ISMInstanceIds.Num())
		{
			break;
		}

		const int32 InstanceId = ISMInstanceIds[MeshIdx];
		if (InstanceId == INDEX_NONE)
		{
			continue;
		}

		const int32 SlotIdx = MeshSlotBase + MeshIdx;
		if (!MeshSlots.IsValidIndex(SlotIdx) || !MeshSlots[SlotIdx])
		{
			continue;
		}

		MeshSlots[SlotIdx]->RemoveTrackedInstance(InstanceId, EntityManager);
	}
}

void AArcIWPartitionActor::PopulatePhysicsLinkEntries(
	FArcMassPhysicsLinkFragment& LinkFragment,
	int32 MeshSlotBase,
	const TArray<int32>& ISMInstanceIds)
{
	for (int32 MeshIdx = 0; MeshIdx < ISMInstanceIds.Num(); ++MeshIdx)
	{
		if (ISMInstanceIds[MeshIdx] == INDEX_NONE)
		{
			continue;
		}

		const int32 SlotIdx = MeshSlotBase + MeshIdx;
		if (!MeshSlots.IsValidIndex(SlotIdx) || !MeshSlots[SlotIdx])
		{
			continue;
		}

		FArcMassPhysicsLinkEntry Entry;
		Entry.Component = MeshSlots[SlotIdx];
		Entry.BodyIndex = ISMInstanceIds[MeshIdx];
		LinkFragment.Entries.Add(Entry);
	}
}

void AArcIWPartitionActor::DetachPhysicsLinkEntries(FArcMassPhysicsLinkFragment& LinkFragment)
{
	for (FArcMassPhysicsLinkEntry& Entry : LinkFragment.Entries)
	{
		if (!Entry.Append)
		{
			continue;
		}

		UPrimitiveComponent* Comp = Entry.Component.Get();
		FBodyInstance* Body = nullptr;

		if (Comp)
		{
			if (Entry.BodyIndex != INDEX_NONE)
			{
				Body = Comp->GetBodyInstance(NAME_None, false, Entry.BodyIndex);
			}
			else
			{
				Body = Comp->GetBodyInstance();
			}
		}

		if (Body)
		{
			ArcMassPhysicsEntityLink::Detach(*Body, Entry.Append);
		}
		else
		{
			delete Entry.Append->UserDefinedEntity;
			Entry.Append->UserDefinedEntity = nullptr;
			delete Entry.Append;
		}
		Entry.Append = nullptr;
	}

	LinkFragment.Entries.Empty();
}

FMassEntityHandle AArcIWPartitionActor::ResolveISMInstanceToEntity(
	const UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex) const
{
	const UArcIWISMComponent* ISMC = Cast<UArcIWISMComponent>(ISMComponent);
	if (!ISMC)
	{
		return FMassEntityHandle();
	}

	const UArcIWISMComponent::FInstanceOwnerEntry* OwnerEntry = ISMC->ResolveOwner(InstanceIndex);
	if (!OwnerEntry)
	{
		return FMassEntityHandle();
	}

	return SpawnedEntities.IsValidIndex(OwnerEntry->InstanceIndex) ? SpawnedEntities[OwnerEntry->InstanceIndex] : FMassEntityHandle();
}

bool AArcIWPartitionActor::ResolveISMInstanceToOwner(
	const UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex,
	FMassEntityHandle& OutEntity, int32& OutInstanceIndex) const
{
	const UArcIWISMComponent* ISMC = Cast<UArcIWISMComponent>(ISMComponent);
	if (!ISMC)
	{
		return false;
	}

	const UArcIWISMComponent::FInstanceOwnerEntry* OwnerEntry = ISMC->ResolveOwner(InstanceIndex);
	if (!OwnerEntry)
	{
		return false;
	}

	OutInstanceIndex = OwnerEntry->InstanceIndex;
	OutEntity = SpawnedEntities.IsValidIndex(OutInstanceIndex) ? SpawnedEntities[OutInstanceIndex] : FMassEntityHandle();
	return OutEntity.IsValid();
}

void AArcIWPartitionActor::HydrateEntity(FMassEntityHandle EntityHandle)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(EntityHandle))
	{
		return;
	}

	FArcIWInstanceFragment* Instance = EntityManager.GetFragmentDataPtr<FArcIWInstanceFragment>(EntityHandle);
	if (!Instance || Instance->bIsActorRepresentation)
	{
		return;
	}

	const FArcIWVisConfigFragment* Config = EntityManager.GetConstSharedFragmentDataPtr<FArcIWVisConfigFragment>(EntityHandle);
	if (!Config || !Config->ActorClass)
	{
		return;
	}

	const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(EntityHandle);
	if (!TransformFrag)
	{
		return;
	}

	// Remove ISM instances
	bool bHasISM = false;
	for (int32 Id : Instance->ISMInstanceIds)
	{
		if (Id != INDEX_NONE)
		{
			bHasISM = true;
			break;
		}
	}
	if (bHasISM)
	{
		RemoveCompositeISMInstances(Instance->MeshSlotBase, Config->MeshDescriptors, Instance->ISMInstanceIds, EntityManager);
		for (int32& Id : Instance->ISMInstanceIds)
		{
			Id = INDEX_NONE;
		}
	}

	// Acquire actor from pool
	UArcIWActorPoolSubsystem* Pool = World->GetSubsystem<UArcIWActorPoolSubsystem>();
	if (Pool)
	{
		AActor* NewActor = Pool->AcquireActor(Config->ActorClass, TransformFrag->GetTransform(), EntityHandle);
		Instance->HydratedActor = NewActor;
	}
	Instance->bIsActorRepresentation = true;
	Instance->bKeepHydrated = true;
}


// ---------------------------------------------------------------------------
// IActorInstanceManagerInterface
// ---------------------------------------------------------------------------

int32 AArcIWPartitionActor::ConvertCollisionIndexToInstanceIndex(int32 InIndex, const UPrimitiveComponent* RelevantComponent) const
{
	if (!UArcIWSettings::Get()->bHydrateOnHit || InIndex == INDEX_NONE || !RelevantComponent)
	{
		return INDEX_NONE;
	}

	const UInstancedStaticMeshComponent* ISMComp = Cast<UInstancedStaticMeshComponent>(RelevantComponent);
	if (!ISMComp)
	{
		return INDEX_NONE;
	}

	FMassEntityHandle Entity;
	int32 InstanceIdx = INDEX_NONE;

	if (!ResolveISMInstanceToOwner(ISMComp, InIndex, Entity, InstanceIdx))
	{
		return INDEX_NONE;
	}

	return InstanceIdx;
}

AActor* AArcIWPartitionActor::FindActor(const FActorInstanceHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return nullptr;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	const int32 Idx = Handle.GetInstanceIndex();
	if (!SpawnedEntities.IsValidIndex(Idx))
	{
		return nullptr;
	}

	FMassEntityHandle EntityHandle = SpawnedEntities[Idx];
	if (!EntityHandle.IsValid() || !EntityManager.IsEntityValid(EntityHandle))
	{
		return nullptr;
	}

	FArcIWInstanceFragment* Instance = EntityManager.GetFragmentDataPtr<FArcIWInstanceFragment>(EntityHandle);
	if (!Instance || !Instance->bIsActorRepresentation)
	{
		return nullptr;
	}

	return Instance->HydratedActor.Get();
}

AActor* AArcIWPartitionActor::FindOrCreateActor(const FActorInstanceHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return nullptr;
	}

	const int32 Idx = Handle.GetInstanceIndex();
	if (!SpawnedEntities.IsValidIndex(Idx))
	{
		return nullptr;
	}

	FMassEntityHandle EntityHandle = SpawnedEntities[Idx];
	if (!EntityHandle.IsValid())
	{
		return nullptr;
	}

	// Check if already hydrated
	AActor* ExistingActor = FindActor(Handle);
	if (ExistingActor)
	{
		return ExistingActor;
	}

	// Hydrate and return the newly created actor
	HydrateEntity(EntityHandle);
	return FindActor(Handle);
}

UClass* AArcIWPartitionActor::GetRepresentedClass(int32 InstanceIndex) const
{
	if (!UArcIWSettings::Get()->bHydrateOnHit || InstanceIndex == INDEX_NONE)
	{
		return nullptr;
	}

	if (!SpawnedEntities.IsValidIndex(InstanceIndex))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return nullptr;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassEntityHandle EntityHandle = SpawnedEntities[InstanceIndex];
	if (!EntityManager.IsEntityValid(EntityHandle))
	{
		return nullptr;
	}

	const FArcIWVisConfigFragment* Config = EntityManager.GetConstSharedFragmentDataPtr<FArcIWVisConfigFragment>(EntityHandle);
	if (Config)
	{
		return Config->ActorClass;
	}

	return nullptr;
}

ULevel* AArcIWPartitionActor::GetLevelForInstance(int32 InstanceIndex) const
{
	return GetLevel();
}

FTransform AArcIWPartitionActor::GetTransform(const FActorInstanceHandle& Handle) const
{
	if (!Handle.IsValid())
	{
		return FTransform::Identity;
	}

	const int32 Idx = Handle.GetInstanceIndex();
	if (!SpawnedEntities.IsValidIndex(Idx))
	{
		return FTransform::Identity;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return FTransform::Identity;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return FTransform::Identity;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassEntityHandle EntityHandle = SpawnedEntities[Idx];
	if (!EntityManager.IsEntityValid(EntityHandle))
	{
		return FTransform::Identity;
	}

	const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(EntityHandle);
	if (TransformFrag)
	{
		return TransformFrag->GetTransform();
	}

	return FTransform::Identity;
}
