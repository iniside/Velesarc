// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/ArcInstancedWorld.h"

#include "ArcInstancedWorld/ArcIWActorPoolSubsystem.h"
#include "ArcInstancedWorld/ArcIWRespawnSubsystem.h"
#include "PCGComponent.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityFragments.h"
#include "MassObserverManager.h"
#include "Engine/ActorInstanceHandle.h"
#include "Engine/StaticMesh.h"
#include "StructUtils/InstancedStruct.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"

// Mass MeshEngine fragments and utilities
#include "Mesh/MassEngineMeshFragments.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "Mesh/MassEngineMeshUtils.h"

// MassEngine Private headers -- required for render state helper creation.
// ArcInstancedWorld.Build.cs adds the Private include path for MassEngine.
#include "MassRenderStateHelper.h"
#include "MassISMRenderStateHelper.h"

// Skinned mesh fragments and render state helper
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassInstancedSkinnedMeshRenderStateHelper.h"
#include "InstancedSkinnedMeshSceneProxyDesc.h"
#include "Engine/SkinnedAsset.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"

// Deferred commands and execution context
#include "MassExecutionContext.h"
#include "MassCommands.h"
#include "Misc/ArchiveMD5.h"

// Physics entity link, Chaos user data, and ArcMass physics body fragment
#include "MassEntityView.h"
#include "ArcMass/Physics/ArcMassPhysicsEntityLink.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"
#include "Chaos/ChaosUserEntity.h"
#include "Engine/World.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

#include "ArcPersistence/ArcPersistenceSubsystem.h"
#include "ArcPersistence/Serialization/ArcJsonSaveArchive.h"
#include "ArcPersistence/Serialization/ArcJsonLoadArchive.h"
#include "ArcPersistence/Storage/ArcPersistenceBackend.h"

#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "MassEntityUtils.h"

// WorldPartition for GetGridCellSize
#include "ArcIWVisualizationSubsystem.h"
#include "MassSpawnerSubsystem.h"
#include "SceneInterface.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartitionLHGrid.h"

// ---------------------------------------------------------------------------
// Template hack to access private RuntimePartitions on UWorldPartitionRuntimeHashSet.
// UWorldPartitionRuntimeHashSet doesn't expose a public getter for its partition descriptors.
// Must live at file scope — the pattern requires the explicit template instantiation to be
// at the same namespace level as the struct definition.
// ---------------------------------------------------------------------------

template<typename Tag, typename Tag::type MemberPtr>
struct FArcIWMassISMPrivateAccessor
{
	friend typename Tag::type GetPrivateMember(Tag) { return MemberPtr; }
};

struct FArcIWMassISMRuntimePartitionsAccess
{
	using type = TArray<FRuntimePartitionDesc> UWorldPartitionRuntimeHashSet::*;
	friend type GetPrivateMember(FArcIWMassISMRuntimePartitionsAccess);
};

template struct FArcIWMassISMPrivateAccessor<FArcIWMassISMRuntimePartitionsAccess, &UWorldPartitionRuntimeHashSet::RuntimePartitions>;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AArcIWMassISMPartitionActor::AArcIWMassISMPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


uint32 AArcIWMassISMPartitionActor::GetGridCellSize(UWorld* InWorld, FName InGridName)
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
				const TArray<FRuntimePartitionDesc>& Partitions = HashSet->*GetPrivateMember(FArcIWMassISMRuntimePartitionsAccess{});
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

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void AArcIWMassISMPartitionActor::PostRegisterAllComponents()
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

void AArcIWMassISMPartitionActor::BeginPlay()
{
	Super::BeginPlay();

#if WITH_EDITOR
	DestroyEditorPreviewISMCs();
#endif

	InitializeISMState();
	LoadRemovals();
	PurgeExpiredRemovals();
	SpawnEntities();

	if (UArcIWRespawnSubsystem* RespawnSubsystem = GetWorld()->GetSubsystem<UArcIWRespawnSubsystem>())
	{
		RespawnSubsystem->RegisterPartitionActor(this);
	}
}

void AArcIWMassISMPartitionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UArcIWRespawnSubsystem* RespawnSubsystem = World->GetSubsystem<UArcIWRespawnSubsystem>())
		{
			RespawnSubsystem->UnregisterPartitionActor(this);
		}
	}

	SaveEntityData();
	SaveRemovals();
	DespawnEntities();
	DestroyISMHolderEntities();
	Super::EndPlay(EndPlayReason);
}

IArcPersistenceBackend* AArcIWMassISMPartitionActor::GetPersistenceBackend() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;
	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance) return nullptr;
	UArcPersistenceSubsystem* PersistenceSubsystem = GameInstance->GetSubsystem<UArcPersistenceSubsystem>();
	if (!PersistenceSubsystem) return nullptr;
	return PersistenceSubsystem->GetBackend();
}

// ---------------------------------------------------------------------------
// ISM Holder Entity Management (Mass MeshEngine rendering)
// ---------------------------------------------------------------------------

void AArcIWMassISMPartitionActor::InitializeISMState()
{
	int32 CompositeSlotCount = 0;
	int32 SimpleVisStaticSlotCount = 0;
	int32 SimpleVisSkinnedSlotCount = 0;

	for (const FArcIWActorClassData& ClassData : ActorClassEntries)
	{
		const bool bIsSingleMesh = (ClassData.MeshDescriptors.Num() == 1);
		if (bIsSingleMesh && !ClassData.MeshDescriptors[0].IsSkinned())
		{
			++SimpleVisStaticSlotCount;
		}
		else if (bIsSingleMesh && ClassData.MeshDescriptors[0].IsSkinned())
		{
			++SimpleVisSkinnedSlotCount;
		}
		else
		{
			CompositeSlotCount += ClassData.MeshDescriptors.Num();
		}
	}

	ISMHolderEntities.SetNumZeroed(CompositeSlotCount);
	SimpleVisISMHolders.SetNumZeroed(SimpleVisStaticSlotCount);
	SimpleVisSkinnedISMHolders.SetNumZeroed(SimpleVisSkinnedSlotCount);
}

void AArcIWMassISMPartitionActor::DestroyISMHolderEntities()
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

	for (FMassEntityHandle HolderEntity : ISMHolderEntities)
	{
		if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
		{
			// Render state destruction is handled by MassDestroyRenderStateProcessor observer
			// when the entity is destroyed.
			EntityManager.DestroyEntity(HolderEntity);
		}
	}
	for (FMassEntityHandle HolderEntity : SimpleVisISMHolders)
	{
		if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
		{
			FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);
			if (RenderState)
			{
				RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);
			}
			EntityManager.DestroyEntity(HolderEntity);
		}
	}
	SimpleVisISMHolders.Empty();
	for (FMassEntityHandle HolderEntity : SimpleVisSkinnedISMHolders)
	{
		if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
		{
			FArcMassRenderISMSkinnedFragment* ISMFrag =
				EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(HolderEntity);
			if (ISMFrag && ISMFrag->RenderStateHelper)
			{
				ISMFrag->RenderStateHelper->DestroyRenderState(nullptr);
				ISMFrag->RenderStateHelper->DestroyMeshObject();
				ISMFrag->RenderStateHelper.Reset();
			}
			EntityManager.DestroyEntity(HolderEntity);
		}
	}
	SimpleVisSkinnedISMHolders.Empty();
	ISMHolderEntities.Empty();
}

// ---------------------------------------------------------------------------
// Persistence -- save/load ClassRemovals
// ---------------------------------------------------------------------------

void AArcIWMassISMPartitionActor::SaveRemovals()
{
	bool bHasData = PendingRespawns.Num() > 0;
	if (!bHasData)
	{
		for (const FArcIWClassRemovals& Entry : ClassRemovals)
		{
			if (Entry.RemovedInstances.Num() > 0)
			{
				bHasData = true;
				break;
			}
		}
	}
	if (!bHasData)
	{
		return;
	}

	IArcPersistenceBackend* Backend = GetPersistenceBackend();
	if (!Backend) return;

	UWorld* World = GetWorld();

	FArcJsonSaveArchive Archive;

	// Serialize ClassRemovals
	Archive.BeginArray(FName(TEXT("ClassRemovals")), ClassRemovals.Num());
	for (int32 Idx = 0; Idx < ClassRemovals.Num(); ++Idx)
	{
		Archive.BeginArrayElement(Idx);

		const TSet<int32>& RemovedSet = ClassRemovals[Idx].RemovedInstances;
		Archive.BeginArray(FName(TEXT("RemovedInstances")), RemovedSet.Num());
		int32 ElemIdx = 0;
		for (int32 RemovedIdx : RemovedSet)
		{
			Archive.BeginArrayElement(ElemIdx);
			Archive.WriteProperty(FName(TEXT("Index")), RemovedIdx);
			Archive.EndArrayElement();
			++ElemIdx;
		}
		Archive.EndArray();

		Archive.EndArrayElement();
	}
	Archive.EndArray();

	// Serialize PendingRespawns
	Archive.BeginArray(FName(TEXT("PendingRespawns")), PendingRespawns.Num());
	for (int32 Idx = 0; Idx < PendingRespawns.Num(); ++Idx)
	{
		Archive.BeginArrayElement(Idx);
		Archive.WriteProperty(FName(TEXT("RespawnAtUtc")), PendingRespawns[Idx].RespawnAtUtc);
		Archive.WriteProperty(FName(TEXT("ClassIndex")), PendingRespawns[Idx].ClassIndex);
		Archive.WriteProperty(FName(TEXT("TransformIndex")), PendingRespawns[Idx].TransformIndex);
		Archive.EndArrayElement();
	}
	Archive.EndArray();

	TArray<uint8> Data = Archive.Finalize();

	FString StorageKey = FString::Printf(TEXT("worlds/%s/partitions/%s/removals"),
		*World->GetName(), *GetName());
	Backend->SaveEntry(StorageKey, MoveTemp(Data));
}

void AArcIWMassISMPartitionActor::LoadRemovals()
{
	IArcPersistenceBackend* Backend = GetPersistenceBackend();
	if (!Backend) return;

	UWorld* World = GetWorld();

	FString StorageKey = FString::Printf(TEXT("worlds/%s/partitions/%s/removals"),
		*World->GetName(), *GetName());

	TFuture<FArcPersistenceLoadResult> Future = Backend->LoadEntry(StorageKey);
	FArcPersistenceLoadResult Result = Future.Get();

	if (!Result.bSuccess || Result.Data.Num() == 0)
	{
		return;
	}

	FArcJsonLoadArchive Archive;
	if (!Archive.InitializeFromData(Result.Data))
	{
		return;
	}

	ClassRemovals.SetNum(ActorClassEntries.Num());

	int32 ClassCount = 0;
	if (!Archive.BeginArray(FName(TEXT("ClassRemovals")), ClassCount))
	{
		return;
	}

	const int32 NumToRead = FMath::Min(ClassCount, ClassRemovals.Num());
	for (int32 Idx = 0; Idx < NumToRead; ++Idx)
	{
		Archive.BeginArrayElement(Idx);

		int32 RemovedCount = 0;
		if (Archive.BeginArray(FName(TEXT("RemovedInstances")), RemovedCount))
		{
			for (int32 R = 0; R < RemovedCount; ++R)
			{
				Archive.BeginArrayElement(R);
				int32 RemovedIdx = INDEX_NONE;
				Archive.ReadProperty(FName(TEXT("Index")), RemovedIdx);
				if (RemovedIdx != INDEX_NONE)
				{
					ClassRemovals[Idx].RemovedInstances.Add(RemovedIdx);
				}
				Archive.EndArrayElement();
			}
			Archive.EndArray();
		}

		Archive.EndArrayElement();
	}
	Archive.EndArray();

	// Load PendingRespawns
	int32 PendingCount = 0;
	if (Archive.BeginArray(FName(TEXT("PendingRespawns")), PendingCount))
	{
		PendingRespawns.Reserve(PendingCount);
		for (int32 Idx = 0; Idx < PendingCount; ++Idx)
		{
			Archive.BeginArrayElement(Idx);
			FArcIWPendingRespawn Entry;
			Archive.ReadProperty(FName(TEXT("RespawnAtUtc")), Entry.RespawnAtUtc);
			Archive.ReadProperty(FName(TEXT("ClassIndex")), Entry.ClassIndex);
			Archive.ReadProperty(FName(TEXT("TransformIndex")), Entry.TransformIndex);
			PendingRespawns.Add(Entry);
			Archive.EndArrayElement();
		}
		Archive.EndArray();
	}
}

// ---------------------------------------------------------------------------
// Per-entity persistence
// ---------------------------------------------------------------------------

FGuid AArcIWMassISMPartitionActor::MakeDeterministicGuid(const FGuid& PartitionGuid, int32 ClassIndex, int32 TransformIndex)
{
	uint32 Hash[4];
	Hash[0] = PartitionGuid.A ^ static_cast<uint32>(ClassIndex);
	Hash[1] = PartitionGuid.B ^ static_cast<uint32>(TransformIndex);
	Hash[2] = PartitionGuid.C ^ HashCombine(static_cast<uint32>(ClassIndex), static_cast<uint32>(TransformIndex));
	Hash[3] = PartitionGuid.D;
	return FGuid(Hash[0], Hash[1], Hash[2], Hash[3]);
}

void AArcIWMassISMPartitionActor::SaveEntityData()
{
	IArcPersistenceBackend* Backend = GetPersistenceBackend();
	if (!Backend) return;

	UWorld* World = GetWorld();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	int32 PersistableCount = 0;
	for (const TPair<FIntPoint, FMassEntityHandle>& Pair : EntityLookupByIndex)
	{
		if (!EntityManager.IsEntityValid(Pair.Value))
		{
			continue;
		}
		if (!EntityManager.GetFragmentDataPtr<FArcMassPersistenceFragment>(Pair.Value))
		{
			continue;
		}
		const int32 ClassIndex = Pair.Key.X;
		if (ClassRemovals.IsValidIndex(ClassIndex) && ClassRemovals[ClassIndex].RemovedInstances.Contains(Pair.Key.Y))
		{
			continue;
		}
		++PersistableCount;
	}

	if (PersistableCount == 0)
	{
		return;
	}

	FArcJsonSaveArchive Archive;
	Archive.BeginArray(FName(TEXT("entities")), PersistableCount);

	int32 EntryIdx = 0;
	for (const TPair<FIntPoint, FMassEntityHandle>& Pair : EntityLookupByIndex)
	{
		const FIntPoint& Key = Pair.Key;
		const FMassEntityHandle& EntityHandle = Pair.Value;

		if (!EntityManager.IsEntityValid(EntityHandle))
		{
			continue;
		}

		const FArcMassPersistenceFragment* PersistFrag = EntityManager.GetFragmentDataPtr<FArcMassPersistenceFragment>(EntityHandle);
		if (!PersistFrag)
		{
			continue;
		}

		const FArcMassPersistenceConfigFragment* PersistConfig =
			EntityManager.GetConstSharedFragmentDataPtr<FArcMassPersistenceConfigFragment>(EntityHandle);
		if (!PersistConfig)
		{
			continue;
		}

		const int32 ClassIndex = Key.X;
		if (ClassRemovals.IsValidIndex(ClassIndex) && ClassRemovals[ClassIndex].RemovedInstances.Contains(Key.Y))
		{
			continue;
		}

		Archive.BeginArrayElement(EntryIdx);
		Archive.WriteProperty(FName(TEXT("ClassIndex")), Key.X);
		Archive.WriteProperty(FName(TEXT("TransformIndex")), Key.Y);
		FArcMassFragmentSerializer::SaveEntityFragments(EntityManager, EntityHandle, *PersistConfig, Archive, World);
		Archive.EndArrayElement();
		++EntryIdx;
	}

	Archive.EndArray();

	TArray<uint8> Data = Archive.Finalize();

	FString StorageKey = FString::Printf(TEXT("worlds/%s/partitions/%s/entitydata"),
		*World->GetName(), *GetName());
	Backend->SaveEntry(StorageKey, MoveTemp(Data));
}

int32 AArcIWMassISMPartitionActor::ComputeMeshSlotBase(int32 ClassIndex) const
{
	int32 CompositeSlotIdx = 0;
	int32 SimpleVisStaticSlotIdx = 0;
	int32 SimpleVisSkinnedSlotIdx = 0;

	for (int32 Idx = 0; Idx <= ClassIndex; ++Idx)
	{
		const FArcIWActorClassData& ClassData = ActorClassEntries[Idx];
		const bool bIsSingleMesh = (ClassData.MeshDescriptors.Num() == 1);
		if (bIsSingleMesh && !ClassData.MeshDescriptors[0].IsSkinned())
		{
			if (Idx == ClassIndex) return SimpleVisStaticSlotIdx;
			++SimpleVisStaticSlotIdx;
		}
		else if (bIsSingleMesh && ClassData.MeshDescriptors[0].IsSkinned())
		{
			if (Idx == ClassIndex) return SimpleVisSkinnedSlotIdx;
			++SimpleVisSkinnedSlotIdx;
		}
		else
		{
			if (Idx == ClassIndex) return CompositeSlotIdx;
			CompositeSlotIdx += ClassData.MeshDescriptors.Num();
		}
	}
	return 0;
}

// ---------------------------------------------------------------------------
// Entity Spawning (Phase B -- mirrors ISM version)
// ---------------------------------------------------------------------------

void AArcIWMassISMPartitionActor::SpawnEntities()
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

	// Ensure ClassRemovals is parallel to ActorClassEntries.
	// May already be populated from persistence load; resize if needed (new classes get empty sets).
	ClassRemovals.SetNum(ActorClassEntries.Num());

	// Count total entities (accounting for removals)
	int32 TotalEntities = 0;
	for (int32 Idx = 0; Idx < ActorClassEntries.Num(); ++Idx)
	{
		TotalEntities += ActorClassEntries[Idx].InstanceTransforms.Num() - ClassRemovals[Idx].RemovedInstances.Num();
	}

	SpawnedEntities.Reserve(TotalEntities);

	// Per-class MeshSlotBase offsets.
	TArray<int32> ClassMeshSlotBases;
	ClassMeshSlotBases.SetNum(ActorClassEntries.Num());
	for (int32 ClassIndex = 0; ClassIndex < ActorClassEntries.Num(); ++ClassIndex)
	{
		ClassMeshSlotBases[ClassIndex] = ComputeMeshSlotBase(ClassIndex);
	}

	// Acquire creation context before entity creation — observers will be deferred
	// until the context is released after persisted data is loaded.
	{
		TSharedRef<FMassObserverManager::FCreationContext> CreationContext =
			EntityManager.GetObserverManager().GetOrMakeCreationContext();

	for (int32 ClassIndex = 0; ClassIndex < ActorClassEntries.Num(); ++ClassIndex)
	{
		const FArcIWActorClassData& ClassData = ActorClassEntries[ClassIndex];
		const TSet<int32>& RemovedSet = ClassRemovals[ClassIndex].RemovedInstances;

		// Build list of transform indices to spawn (skip removed).
		TArray<int32> TransformIndicesToSpawn;
		TransformIndicesToSpawn.Reserve(ClassData.InstanceTransforms.Num());
		for (int32 i = 0; i < ClassData.InstanceTransforms.Num(); ++i)
		{
			if (!RemovedSet.Contains(i))
			{
				TransformIndicesToSpawn.Add(i);
			}
		}

		const int32 InstanceCount = TransformIndicesToSpawn.Num();
		if (InstanceCount == 0)
		{
			continue;
		}

		// Get or build entity template via visualization subsystem cache.
		// On dedicated servers the subsystem is null, so we build a minimal template inline.
		FMassArchetypeHandle Archetype;
		const FMassArchetypeSharedFragmentValues* SharedValuesPtr = nullptr;

		UArcIWVisualizationSubsystem* VisSubsystem = World->GetSubsystem<UArcIWVisualizationSubsystem>();
		if (VisSubsystem)
		{
			const UArcIWVisualizationSubsystem::FCachedEntityTemplate& Cached =
				VisSubsystem->EnsureEntityTemplate(ClassData, *World);
			Archetype = Cached.EntityTemplate->GetArchetype();
			SharedValuesPtr = &Cached.EntityTemplate->GetSharedFragmentValues();
		}

		// Dedicated server fallback — no vis subsystem, build gameplay-only template
		if (!VisSubsystem)
		{
			FMassEntityTemplateData TemplateData;

			if (ClassData.AdditionalEntityConfig != nullptr)
			{
				FMassEntityTemplateBuildContext BuildContext(TemplateData);
				const FMassEntityConfig& Config = ClassData.AdditionalEntityConfig->GetConfig();
				BuildContext.BuildFromTraits(Config.GetTraits(), *World);
			}

			const bool bIsSingleMesh = (ClassData.MeshDescriptors.Num() == 1);
			if (bIsSingleMesh && ClassData.MeshDescriptors[0].IsSkinned())
			{
				TemplateData.AddTag<FArcIWSimpleVisSkinnedTag>();
			}
			else if (bIsSingleMesh)
			{
				TemplateData.AddTag<FArcIWSimpleVisEntityTag>();
			}
			else
			{
				TemplateData.AddTag<FArcIWEntityTag>();
			}

			TemplateData.AddFragment<FArcIWInstanceFragment>();
			TemplateData.AddFragment<FTransformFragment>();
			TemplateData.AddFragment<FArcMassPhysicsBodyFragment>();

			FArcIWVisConfigFragment ConfigFragment;
			ConfigFragment.ActorClass = ClassData.ActorClass;
			ConfigFragment.MeshDescriptors = ClassData.MeshDescriptors;
			TemplateData.AddConstSharedFragment(FConstSharedStruct::Make(ConfigFragment));

			if (ClassData.CollisionBodySetup)
			{
				FArcMassPhysicsBodyConfigFragment PhysicsConfig;
				PhysicsConfig.BodySetup = ClassData.CollisionBodySetup;
				PhysicsConfig.BodyTemplate = ClassData.BodyTemplate;
				PhysicsConfig.BodyType = ClassData.PhysicsBodyType;
				TemplateData.AddConstSharedFragment(FConstSharedStruct::Make(PhysicsConfig));
			}

			FGuid ClassGuid = FGuid::NewDeterministicGuid(ClassData.ActorClass->GetPathName());
			FMassEntityTemplateID TemplateID = FMassEntityTemplateIDFactory::Make(ClassGuid);

			UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
			check(SpawnerSubsystem);
			FMassEntityTemplateRegistry& TemplateRegistry = SpawnerSubsystem->GetMutableTemplateRegistryInstance();
			const TSharedRef<FMassEntityTemplate>& RegisteredTemplate = TemplateRegistry.FindOrAddTemplate(TemplateID, MoveTemp(TemplateData));
			Archetype = RegisteredTemplate->GetArchetype();
			SharedValuesPtr = &RegisteredTemplate->GetSharedFragmentValues();
		}

		const FMassArchetypeSharedFragmentValues& SharedValues = *SharedValuesPtr;
		TArray<FMassEntityHandle> ClassEntities;
		EntityManager.BatchCreateEntities(Archetype, SharedValues, InstanceCount, ClassEntities);

		const int32 BaseIndex = SpawnedEntities.Num();
		const int32 ClassMeshSlotBase = ClassMeshSlotBases[ClassIndex];

		for (int32 i = 0; i < InstanceCount; ++i)
		{
			const int32 OriginalTransformIndex = TransformIndicesToSpawn[i];

			FTransformFragment& TransformFragment = EntityManager.GetFragmentDataChecked<FTransformFragment>(ClassEntities[i]);
			TransformFragment.SetTransform(ClassData.InstanceTransforms[OriginalTransformIndex]);

			FArcIWInstanceFragment& InstanceFragment = EntityManager.GetFragmentDataChecked<FArcIWInstanceFragment>(ClassEntities[i]);
			InstanceFragment.InstanceIndex = BaseIndex + i;
			InstanceFragment.MeshSlotBase = ClassMeshSlotBase;
			InstanceFragment.PartitionActor = this;
			InstanceFragment.ClassIndex = ClassIndex;
			InstanceFragment.TransformIndex = OriginalTransformIndex;

			if (ActorClassEntries[ClassIndex].MeshDescriptors.Num() == 1)
			{
				InstanceFragment.ISMInstanceIds.SetNum(1);
				InstanceFragment.ISMInstanceIds[0] = INDEX_NONE;
			}

			const FIntPoint EntityKey(ClassIndex, OriginalTransformIndex);
			EntityLookupByIndex.Add(EntityKey, ClassEntities[i]);

			// Assign deterministic GUID if entity has persistence fragment
			FArcMassPersistenceFragment* PersistFrag = EntityManager.GetFragmentDataPtr<FArcMassPersistenceFragment>(ClassEntities[i]);
			if (PersistFrag)
			{
				PersistFrag->PersistenceGuid = MakeDeterministicGuid(GetActorGuid(), ClassIndex, OriginalTransformIndex);
			}
		}

		SpawnedEntities.Append(ClassEntities);
	}

	// -----------------------------------------------------------------------
	// Apply persisted entity data before releasing CreationContext.
	// Observers have not fired yet — transforms and fragments will be correct
	// when init observers run.
	// -----------------------------------------------------------------------
	IArcPersistenceBackend* Backend = GetPersistenceBackend();
	if (Backend)
	{
		FString StorageKey = FString::Printf(TEXT("worlds/%s/partitions/%s/entitydata"),
			*World->GetName(), *GetName());

		TFuture<FArcPersistenceLoadResult> Future = Backend->LoadEntry(StorageKey);
		FArcPersistenceLoadResult Result = Future.Get();

		if (Result.bSuccess && Result.Data.Num() > 0)
		{
			FArcJsonLoadArchive Archive;
			if (Archive.InitializeFromData(Result.Data))
			{
				int32 EntityCount = 0;
				if (Archive.BeginArray(FName(TEXT("entities")), EntityCount))
				{
					for (int32 Idx = 0; Idx < EntityCount; ++Idx)
					{
						if (!Archive.BeginArrayElement(Idx))
						{
							continue;
						}

						int32 ClassIndex = INDEX_NONE;
						int32 TransformIndex = INDEX_NONE;
						Archive.ReadProperty(FName(TEXT("ClassIndex")), ClassIndex);
						Archive.ReadProperty(FName(TEXT("TransformIndex")), TransformIndex);

						bool bSkipEntity = false;
						if (ClassRemovals.IsValidIndex(ClassIndex) && ClassRemovals[ClassIndex].RemovedInstances.Contains(TransformIndex))
						{
							bSkipEntity = true;
						}

						const FIntPoint EntityKey(ClassIndex, TransformIndex);
						const FMassEntityHandle* EntityHandle = bSkipEntity ? nullptr : EntityLookupByIndex.Find(EntityKey);
						if (!EntityHandle || !EntityManager.IsEntityValid(*EntityHandle))
						{
							bSkipEntity = true;
						}

						if (bSkipEntity)
						{
							int32 FragCount = 0;
							if (Archive.BeginArray(FName(TEXT("fragments")), FragCount))
							{
								for (int32 F = 0; F < FragCount; ++F)
								{
									Archive.BeginArrayElement(F);
									Archive.EndArrayElement();
								}
								Archive.EndArray();
							}
							Archive.EndArrayElement();
							continue;
						}

						FArcMassFragmentSerializer::LoadEntityFragments(EntityManager, *EntityHandle, Archive, World);

						FArcLifecycleFragment* LifecycleFrag = EntityManager.GetFragmentDataPtr<FArcLifecycleFragment>(*EntityHandle);
						const FArcLifecycleConfigFragment* Config =
							EntityManager.GetConstSharedFragmentDataPtr<FArcLifecycleConfigFragment>(*EntityHandle);
						if (LifecycleFrag && Config && !Config->IsValidPhase(LifecycleFrag->CurrentPhase))
						{
							UE_LOG(LogMass, Warning, TEXT("LoadEntityData: Phase %d out of range for entity (%d,%d), clamping to InitialPhase %d"),
								LifecycleFrag->CurrentPhase, ClassIndex, TransformIndex, Config->InitialPhase);
							LifecycleFrag->CurrentPhase = Config->InitialPhase;
							LifecycleFrag->PreviousPhase = Config->InitialPhase;
							LifecycleFrag->PhaseTimeElapsed = 0.f;
						}

						Archive.EndArrayElement();
					}

					Archive.EndArray();
				}
			}
		}
	}

	// CreationContext drops here — observers fire with correct persisted data
	}
}

void AArcIWMassISMPartitionActor::DespawnEntities()
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

	TArray<FMassEntityHandle> ValidEntities;
	ValidEntities.Reserve(SpawnedEntities.Num());
	for (FMassEntityHandle EntityHandle : SpawnedEntities)
	{
		if (EntityManager.IsEntityValid(EntityHandle))
		{
			FArcMassPhysicsBodyFragment* BodyFrag = EntityManager.GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(EntityHandle);
			if (BodyFrag)
			{
				BodyFrag->TerminateBody();
			}
			ValidEntities.Add(EntityHandle);
		}
	}
	EntityManager.BatchDestroyEntities(MakeArrayView(ValidEntities));
	SpawnedEntities.Reset();
	EntityLookupByIndex.Reset();
}

void AArcIWMassISMPartitionActor::MarkInstanceRemoved(int32 ClassIndex, int32 TransformIndex)
{
	if (!ClassRemovals.IsValidIndex(ClassIndex))
	{
		UE_LOG(LogArcIW, Warning, TEXT("MarkInstanceRemoved: invalid ClassIndex %d (max %d)"),
			ClassIndex, ClassRemovals.Num() - 1);
		return;
	}

	const FArcIWActorClassData& ClassData = ActorClassEntries[ClassIndex];
	if (TransformIndex < 0 || TransformIndex >= ClassData.InstanceTransforms.Num())
	{
		UE_LOG(LogArcIW, Warning, TEXT("MarkInstanceRemoved: invalid TransformIndex %d for class %d (max %d)"),
			TransformIndex, ClassIndex, ClassData.InstanceTransforms.Num() - 1);
		return;
	}

	ClassRemovals[ClassIndex].RemovedInstances.Add(TransformIndex);

	// Queue respawn if class has a respawn time
	int32 RespawnTime = ActorClassEntries[ClassIndex].RespawnTimeSeconds;
	if (RespawnTime > 0)
	{
		FArcIWPendingRespawn PendingEntry;
		PendingEntry.RespawnAtUtc = FDateTime::UtcNow().ToUnixTimestamp() + RespawnTime;
		PendingEntry.ClassIndex = ClassIndex;
		PendingEntry.TransformIndex = TransformIndex;
		PendingRespawns.Add(PendingEntry);
	}

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

	const FIntPoint EntityKey(ClassIndex, TransformIndex);
	FMassEntityHandle* FoundEntity = EntityLookupByIndex.Find(EntityKey);
	if (FoundEntity && EntityManager.IsEntityValid(*FoundEntity))
	{
		EntityManager.DestroyEntity(*FoundEntity);
	}
	EntityLookupByIndex.Remove(EntityKey);
}

// ---------------------------------------------------------------------------
// Respawn
// ---------------------------------------------------------------------------

TArray<FArcIWPendingRespawn> AArcIWMassISMPartitionActor::CollectAndClearExpiredRespawns(int64 NowUtc)
{
	TArray<FArcIWPendingRespawn> Expired;
	int32 ExpiredCount = 0;
	for (int32 Idx = 0; Idx < PendingRespawns.Num(); ++Idx)
	{
		if (PendingRespawns[Idx].RespawnAtUtc > NowUtc) break;
		Expired.Add(PendingRespawns[Idx]);
		++ExpiredCount;
	}
	if (ExpiredCount > 0)
	{
		PendingRespawns.RemoveAt(0, ExpiredCount);
	}
	for (const FArcIWPendingRespawn& Entry : Expired)
	{
		if (ClassRemovals.IsValidIndex(Entry.ClassIndex))
		{
			ClassRemovals[Entry.ClassIndex].RemovedInstances.Remove(Entry.TransformIndex);
		}
	}
	return Expired;
}

void AArcIWMassISMPartitionActor::PurgeExpiredRemovals()
{
	int64 NowUtc = FDateTime::UtcNow().ToUnixTimestamp();
	CollectAndClearExpiredRespawns(NowUtc);
}

void AArcIWMassISMPartitionActor::RestoreExpiredRemovals(int64 NowUtc)
{
	TArray<FArcIWPendingRespawn> Expired = CollectAndClearExpiredRespawns(NowUtc);
	for (const FArcIWPendingRespawn& Entry : Expired)
	{
		RestoreInstance(Entry.ClassIndex, Entry.TransformIndex);
	}
}

void AArcIWMassISMPartitionActor::RestoreInstance(int32 ClassIndex, int32 TransformIndex)
{
	if (!ActorClassEntries.IsValidIndex(ClassIndex))
	{
		UE_LOG(LogArcIW, Warning, TEXT("RestoreInstance: invalid ClassIndex %d"), ClassIndex);
		return;
	}

	const FArcIWActorClassData& ClassData = ActorClassEntries[ClassIndex];
	if (TransformIndex < 0 || TransformIndex >= ClassData.InstanceTransforms.Num())
	{
		UE_LOG(LogArcIW, Warning, TEXT("RestoreInstance: invalid TransformIndex %d for class %d"), TransformIndex, ClassIndex);
		return;
	}

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

	const int32 MeshSlotBase = ComputeMeshSlotBase(ClassIndex);

	// Look up the cached template (already registered by SpawnEntities)
	UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
	if (!SpawnerSubsystem)
	{
		return;
	}

	FGuid ClassGuid = FGuid::NewDeterministicGuid(ClassData.ActorClass->GetPathName());
	FMassEntityTemplateID TemplateID = FMassEntityTemplateIDFactory::Make(ClassGuid);
	FMassEntityTemplateRegistry& TemplateRegistry = SpawnerSubsystem->GetMutableTemplateRegistryInstance();
	const TSharedRef<FMassEntityTemplate>* TemplatePtr = TemplateRegistry.FindTemplateFromTemplateID(TemplateID);
	if (!TemplatePtr)
	{
		UE_LOG(LogArcIW, Warning, TEXT("RestoreInstance: no cached template for class %s"), *ClassData.ActorClass->GetName());
		return;
	}

	const FMassEntityTemplate& Template = TemplatePtr->Get();
	const FMassArchetypeHandle& Archetype = Template.GetArchetype();
	const FMassArchetypeSharedFragmentValues& SharedValues = Template.GetSharedFragmentValues();

	TArray<FMassEntityHandle> NewEntities;
	TSharedRef<FMassObserverManager::FCreationContext> CreationContext = EntityManager.BatchCreateEntities(
		Archetype, SharedValues, 1, NewEntities);

	FMassEntityHandle NewEntity = NewEntities[0];
	int32 InstanceIndex = SpawnedEntities.Num();

	FTransformFragment& TransformFragment = EntityManager.GetFragmentDataChecked<FTransformFragment>(NewEntity);
	TransformFragment.SetTransform(ClassData.InstanceTransforms[TransformIndex]);

	FArcIWInstanceFragment& InstanceFragment = EntityManager.GetFragmentDataChecked<FArcIWInstanceFragment>(NewEntity);
	InstanceFragment.InstanceIndex = InstanceIndex;
	InstanceFragment.MeshSlotBase = MeshSlotBase;
	InstanceFragment.PartitionActor = this;
	InstanceFragment.ClassIndex = ClassIndex;
	InstanceFragment.TransformIndex = TransformIndex;

	if (ClassData.MeshDescriptors.Num() == 1)
	{
		InstanceFragment.ISMInstanceIds.SetNum(1);
		InstanceFragment.ISMInstanceIds[0] = INDEX_NONE;
	}

	const FIntPoint EntityKey(ClassIndex, TransformIndex);
	EntityLookupByIndex.Add(EntityKey, NewEntity);

	// Assign deterministic GUID if entity has persistence fragment
	FArcMassPersistenceFragment* PersistFrag = EntityManager.GetFragmentDataPtr<FArcMassPersistenceFragment>(NewEntity);
	if (PersistFrag)
	{
		PersistFrag->PersistenceGuid = MakeDeterministicGuid(GetActorGuid(), ClassIndex, TransformIndex);
	}

	SpawnedEntities.Add(NewEntity);
}

// ---------------------------------------------------------------------------
// ActivateMesh / DeactivateMesh
// ---------------------------------------------------------------------------

void AArcIWMassISMPartitionActor::ActivateMesh(
	TArray<FArcIWMeshActivationRequest>& Requests,
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TArray<TArray<FArcIWMeshActivationRequest>> PendingBySlot;
	PendingBySlot.SetNum(ISMHolderEntities.Num());

	for (FArcIWMeshActivationRequest& Req : Requests)
	{
		FMassEntityHandle HolderEntity = ISMHolderEntities[Req.HolderSlot];
		if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
		{
			FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(HolderEntity);
			FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);
			FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

			if (ISMFrag && PrimFrag && RenderState)
			{
				FInstancedStaticMeshInstanceData InstanceData;
				InstanceData.Transform = Req.FinalTransform.ToMatrixWithScale();
				int32 SparseIdx = ISMFrag->PerInstanceSMData.Add(InstanceData);

				FTransformFragment* HolderTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(HolderEntity);
				if (HolderTransform)
				{
					PrimFrag->LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
						*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
					PrimFrag->WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
						*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);
				}

				PrimFrag->bIsVisible = true;
				RenderState->GetRenderStateHelper().MarkRenderStateDirty();

				FArcIWInstanceFragment& SourceInstance = EntityManager.GetFragmentDataChecked<FArcIWInstanceFragment>(Req.SourceEntity);
				if (SourceInstance.ISMInstanceIds.IsValidIndex(Req.MeshIdx))
				{
					SourceInstance.ISMInstanceIds[Req.MeshIdx] = SparseIdx;
				}
			}
		}
		else
		{
			PendingBySlot[Req.HolderSlot].Add(MoveTemp(Req));
		}
	}

	bool bHasPending = false;
	for (const TArray<FArcIWMeshActivationRequest>& SlotRequests : PendingBySlot)
	{
		if (SlotRequests.Num() > 0) { bHasPending = true; break; }
	}
	if (!bHasPending) return;

	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	FMassArchetypeHandle StaticHolderArchetype;
	FMassArchetypeHandle StaticHolderArchetypeWithMats;
	if (EntitySubsystem)
	{
		FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();
		FMassElementBitSet Composition;
		Composition.Add<FTransformFragment>();
		Composition.Add<FMassRenderStateFragment>();
		Composition.Add<FMassRenderPrimitiveFragment>();
		Composition.Add<FMassRenderISMFragment>();
		StaticHolderArchetype = EM.CreateArchetype(Composition,
			FMassArchetypeCreationParams{TEXT("ArcIWMassISMHolder")});
		FMassElementBitSet CompositionWithMats = Composition;
		CompositionWithMats.Add<FMassOverrideMaterialsFragment>();
		StaticHolderArchetypeWithMats = EM.CreateArchetype(CompositionWithMats,
			FMassArchetypeCreationParams{TEXT("ArcIWMassISMHolderMats")});
	}

	TWeakObjectPtr<AArcIWMassISMPartitionActor> WeakSelf = this;
	Context.Defer().PushCommand<FMassDeferredAddCommand>(
	[WeakSelf, PendingBySlot = MoveTemp(PendingBySlot), StaticHolderArchetype, StaticHolderArchetypeWithMats](FMassEntityManager& DeferredEM)
	{
		AArcIWMassISMPartitionActor* Self = WeakSelf.Get();
		if (!Self) return;
		for (int32 Slot = 0; Slot < PendingBySlot.Num(); ++Slot)
		{
			const TArray<FArcIWMeshActivationRequest>& SlotRequests = PendingBySlot[Slot];
			if (SlotRequests.IsEmpty()) continue;

			FMassEntityHandle ExistingHolder = Self->ISMHolderEntities[Slot];
			if (ExistingHolder.IsValid() && DeferredEM.IsEntityValid(ExistingHolder))
			{
				FMassRenderISMFragment* ISMFrag = DeferredEM.GetFragmentDataPtr<FMassRenderISMFragment>(ExistingHolder);
				FMassRenderPrimitiveFragment* PrimFrag = DeferredEM.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(ExistingHolder);
				FMassRenderStateFragment* RenderState = DeferredEM.GetFragmentDataPtr<FMassRenderStateFragment>(ExistingHolder);

				if (ISMFrag && PrimFrag && RenderState)
				{
					for (const FArcIWMeshActivationRequest& Req : SlotRequests)
					{
						if (!DeferredEM.IsEntityValid(Req.SourceEntity)) continue;

						FInstancedStaticMeshInstanceData InstanceData;
						InstanceData.Transform = Req.FinalTransform.ToMatrixWithScale();
						int32 SparseIdx = ISMFrag->PerInstanceSMData.Add(InstanceData);

						FArcIWInstanceFragment* SourceInstance =
							DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(Req.SourceEntity);
						if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(Req.MeshIdx))
						{
							SourceInstance->ISMInstanceIds[Req.MeshIdx] = SparseIdx;
						}
					}

					FTransformFragment* HolderTransform = DeferredEM.GetFragmentDataPtr<FTransformFragment>(ExistingHolder);
					if (HolderTransform)
					{
						PrimFrag->LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
							*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
						PrimFrag->WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
							*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);
					}

					PrimFrag->bIsVisible = true;
					RenderState->GetRenderStateHelper().MarkRenderStateDirty();
				}
				continue;
			}

			const FArcIWMeshActivationRequest& First = SlotRequests[0];
			FMassArchetypeHandle Archetype = First.bHasOverrideMats
				? StaticHolderArchetypeWithMats : StaticHolderArchetype;

			FMassStaticMeshFragment StaticMeshFrag(First.MeshEntry.Mesh);
			FMassVisualizationMeshFragment VisMeshFrag;
			VisMeshFrag.CastShadow = First.MeshEntry.bCastShadows;

			FMassArchetypeSharedFragmentValues SharedValues;
			SharedValues.Add(FConstSharedStruct::Make(StaticMeshFrag));
			SharedValues.Add(FConstSharedStruct::Make(VisMeshFrag));

			FMassOverrideMaterialsFragment OverrideMats;
			if (First.bHasOverrideMats)
			{
				OverrideMats.OverrideMaterials = First.MeshEntry.Materials;
				SharedValues.Add(FConstSharedStruct::Make(OverrideMats));
			}
			SharedValues.Sort();

			TArray<FMassEntityHandle> CreatedEntities;
			TSharedRef<FMassObserverManager::FCreationContext> CreationContext =
				DeferredEM.BatchCreateEntities(Archetype, SharedValues, 1, CreatedEntities);

			if (CreatedEntities.IsEmpty()) continue;

			FMassEntityHandle HolderEntity = CreatedEntities[0];
			FMassEntityView EntityView(DeferredEM, HolderEntity);

			FTransformFragment& TransformFrag = EntityView.GetFragmentData<FTransformFragment>();
			TransformFrag.SetTransform(FTransform::Identity);

			FMassRenderISMFragment& ISMFrag = EntityView.GetFragmentData<FMassRenderISMFragment>();
			FMassRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FMassRenderPrimitiveFragment>();
			PrimFrag.bIsVisible = true;

			const FMassOverrideMaterialsFragment* OverrideMatsPtr =
				First.bHasOverrideMats ? &OverrideMats : nullptr;

			checkf(ISMFrag.InstancedStaticMeshSceneProxyDesc, TEXT("Default ctor should create desc"));
			UE::MassEngine::Mesh::InitializeInstanceStaticMeshSceneProxyDescFromFragment(
				StaticMeshFrag, VisMeshFrag, TransformFrag, *ISMFrag.InstancedStaticMeshSceneProxyDesc);

			for (const FArcIWMeshActivationRequest& Req : SlotRequests)
			{
				if (!DeferredEM.IsEntityValid(Req.SourceEntity)) continue;

				FInstancedStaticMeshInstanceData InstanceData;
				InstanceData.Transform = Req.FinalTransform.ToMatrixWithScale();
				int32 SparseIdx = ISMFrag.PerInstanceSMData.Add(InstanceData);

				FArcIWInstanceFragment* SourceInstance =
					DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(Req.SourceEntity);
				if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(Req.MeshIdx))
				{
					SourceInstance->ISMInstanceIds[Req.MeshIdx] = SparseIdx;
				}
			}

			PrimFrag.LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
				TransformFrag, ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
			PrimFrag.WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
				TransformFrag, ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);

			FMassRenderStateFragment& RenderState = EntityView.GetFragmentData<FMassRenderStateFragment>();
			RenderState.CreateRenderStateHelper<FMassISMRenderStateHelper>(
				HolderEntity, &DeferredEM, PrimFrag, OverrideMatsPtr, ISMFrag);

			Self->ISMHolderEntities[Slot] = HolderEntity;
		}
	});
}

void AArcIWMassISMPartitionActor::ActivateMesh(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWVisConfigFragment& Config,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	const int32 MeshCount = Config.MeshDescriptors.Num();
	Instance.ISMInstanceIds.SetNum(MeshCount);

	TArray<FArcIWMeshActivationRequest> Requests;
	for (int32 MeshIdx = 0; MeshIdx < MeshCount; ++MeshIdx)
	{
		Instance.ISMInstanceIds[MeshIdx] = INDEX_NONE;
		const FArcIWMeshEntry& MeshEntry = Config.MeshDescriptors[MeshIdx];
		if (!MeshEntry.Mesh)
		{
			continue;
		}

		int32 HolderSlot = Instance.MeshSlotBase + MeshIdx;
		if (!IsValidHolderSlot(HolderSlot))
		{
			continue;
		}

		bool bHasOverrideMats = false;
		for (const TObjectPtr<UMaterialInterface>& Mat : MeshEntry.Materials)
		{
			if (Mat)
			{
				bHasOverrideMats = true;
				break;
			}
		}

		FArcIWMeshActivationRequest Req;
		Req.SourceEntity = Entity;
		Req.FinalTransform = MeshEntry.RelativeTransform * WorldTransform;
		Req.MeshEntry = MeshEntry;
		Req.MeshIdx = MeshIdx;
		Req.HolderSlot = HolderSlot;
		Req.bHasOverrideMats = bHasOverrideMats;
		Requests.Add(MoveTemp(Req));
	}

	if (Requests.Num() > 0)
	{
		ActivateMesh(Requests, EntityManager, Context);
	}
}

void AArcIWMassISMPartitionActor::DeactivateMesh(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWVisConfigFragment& Config,
	FMassEntityManager& EntityManager)
{
	const int32 MeshCount = Config.MeshDescriptors.Num();

	for (int32 MeshIdx = 0; MeshIdx < MeshCount; ++MeshIdx)
	{
		const int32 HolderSlot = Instance.MeshSlotBase + MeshIdx;
		if (!ISMHolderEntities.IsValidIndex(HolderSlot))
		{
			if (Instance.ISMInstanceIds.IsValidIndex(MeshIdx))
			{
				Instance.ISMInstanceIds[MeshIdx] = INDEX_NONE;
			}
			continue;
		}

		FMassEntityHandle HolderEntity = ISMHolderEntities[HolderSlot];
		if (!HolderEntity.IsValid() || !EntityManager.IsEntityValid(HolderEntity))
		{
			if (Instance.ISMInstanceIds.IsValidIndex(MeshIdx))
			{
				Instance.ISMInstanceIds[MeshIdx] = INDEX_NONE;
			}
			continue;
		}

		FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(HolderEntity);
		FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);
		FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

		if (ISMFrag && PrimFrag && RenderState)
		{
			const int32 SparseIdx = Instance.ISMInstanceIds.IsValidIndex(MeshIdx) ? Instance.ISMInstanceIds[MeshIdx] : INDEX_NONE;
			if (SparseIdx != INDEX_NONE && ISMFrag->PerInstanceSMData.IsValidIndex(SparseIdx))
			{
				ISMFrag->PerInstanceSMData.RemoveAt(SparseIdx);
				if (ISMFrag->PerInstanceSMData.Num() == 0)
				{
					PrimFrag->bIsVisible = false;
				}
				RenderState->GetRenderStateHelper().MarkRenderStateDirty();
			}
		}

		if (Instance.ISMInstanceIds.IsValidIndex(MeshIdx))
		{
			Instance.ISMInstanceIds[MeshIdx] = INDEX_NONE;
		}
	}
}

void AArcIWMassISMPartitionActor::ActivateSimpleVisMesh(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWVisConfigFragment& Config,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	const int32 HolderSlot = Instance.MeshSlotBase;
	if (!SimpleVisISMHolders.IsValidIndex(HolderSlot))
	{
		return;
	}

	// Already has an ISM instance
	if (Instance.ISMInstanceIds.IsValidIndex(0) && Instance.ISMInstanceIds[0] != INDEX_NONE)
	{
		return;
	}

	FMassEntityHandle HolderEntity = SimpleVisISMHolders[HolderSlot];

	// Holder exists — add instance immediately
	if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
	{
		FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(HolderEntity);
		FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);
		FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

		if (ISMFrag && PrimFrag && RenderState)
		{
			FInstancedStaticMeshInstanceData InstanceData;
			InstanceData.Transform = WorldTransform.ToMatrixWithScale();
			int32 SparseIdx = ISMFrag->PerInstanceSMData.Add(InstanceData);

			if (Instance.ISMInstanceIds.IsValidIndex(0))
			{
				Instance.ISMInstanceIds[0] = SparseIdx;
			}

			FTransformFragment* HolderTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(HolderEntity);
			if (HolderTransform)
			{
				PrimFrag->LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
					*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
				PrimFrag->WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
					*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);
			}

			PrimFrag->bIsVisible = true;
			RenderState->GetRenderStateHelper().MarkRenderStateDirty();
		}
		return;
	}

	// Holder doesn't exist — defer creation
	if (Config.MeshDescriptors.Num() == 0 || !Config.MeshDescriptors[0].Mesh)
	{
		return;
	}

	const FArcIWMeshEntry& MeshEntry = Config.MeshDescriptors[0];

	bool bHasOverrideMats = false;
	for (const TObjectPtr<UMaterialInterface>& Mat : MeshEntry.Materials)
	{
		if (Mat) { bHasOverrideMats = true; break; }
	}

	UArcIWVisualizationSubsystem* VisSubsystem = GetWorld()->GetSubsystem<UArcIWVisualizationSubsystem>();
	FMassArchetypeHandle HolderArchetype;
	if (VisSubsystem)
	{
		const UArcIWVisualizationSubsystem::FCachedEntityTemplate& Cached =
			VisSubsystem->EnsureEntityTemplate(ActorClassEntries[Instance.ClassIndex], *GetWorld());
		HolderArchetype = Cached.HolderArchetype;
	}

	TWeakObjectPtr<AArcIWMassISMPartitionActor> WeakSelf = this;
	Context.Defer().PushCommand<FMassDeferredAddCommand>(
	[WeakSelf, Entity, HolderSlot, WorldTransform, MeshEntry, bHasOverrideMats, HolderArchetype](FMassEntityManager& DeferredEM)
	{
		AArcIWMassISMPartitionActor* Self = WeakSelf.Get();
		if (!Self) return;
		if (!DeferredEM.IsEntityValid(Entity)) return;

		// Check if holder was created by a previous deferred command
		FMassEntityHandle ExistingHolder = Self->SimpleVisISMHolders[HolderSlot];
		if (ExistingHolder.IsValid() && DeferredEM.IsEntityValid(ExistingHolder))
		{
			FMassRenderISMFragment* ISMFrag = DeferredEM.GetFragmentDataPtr<FMassRenderISMFragment>(ExistingHolder);
			FMassRenderPrimitiveFragment* PrimFrag = DeferredEM.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(ExistingHolder);
			FMassRenderStateFragment* RenderState = DeferredEM.GetFragmentDataPtr<FMassRenderStateFragment>(ExistingHolder);

			if (ISMFrag && PrimFrag && RenderState)
			{
				FInstancedStaticMeshInstanceData InstanceData;
				InstanceData.Transform = WorldTransform.ToMatrixWithScale();
				int32 SparseIdx = ISMFrag->PerInstanceSMData.Add(InstanceData);

				FArcIWInstanceFragment* SourceInstance =
					DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(Entity);
				if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(0))
				{
					SourceInstance->ISMInstanceIds[0] = SparseIdx;
				}

				FTransformFragment* HolderTransform = DeferredEM.GetFragmentDataPtr<FTransformFragment>(ExistingHolder);
				if (HolderTransform)
				{
					PrimFrag->LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
						*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
					PrimFrag->WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
						*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);
				}

				PrimFrag->bIsVisible = true;
				RenderState->GetRenderStateHelper().MarkRenderStateDirty();
			}
			return;
		}

		// Create holder entity
		FMassArchetypeHandle Archetype = HolderArchetype;

		FMassStaticMeshFragment StaticMeshFrag(MeshEntry.Mesh);
		FMassVisualizationMeshFragment VisMeshFrag;
		VisMeshFrag.CastShadow = MeshEntry.bCastShadows;

		FMassArchetypeSharedFragmentValues SharedValues;
		SharedValues.Add(FConstSharedStruct::Make(StaticMeshFrag));
		SharedValues.Add(FConstSharedStruct::Make(VisMeshFrag));

		FMassOverrideMaterialsFragment OverrideMats;
		if (bHasOverrideMats)
		{
			OverrideMats.OverrideMaterials = MeshEntry.Materials;
			SharedValues.Add(FConstSharedStruct::Make(OverrideMats));
		}
		SharedValues.Sort();

		TArray<FMassEntityHandle> CreatedEntities;
		TSharedRef<FMassObserverManager::FCreationContext> CreationContext =
			DeferredEM.BatchCreateEntities(Archetype, SharedValues, 1, CreatedEntities);

		if (CreatedEntities.IsEmpty()) return;

		FMassEntityHandle HolderEntity = CreatedEntities[0];
		FMassEntityView EntityView(DeferredEM, HolderEntity);

		FTransformFragment& TransformFrag = EntityView.GetFragmentData<FTransformFragment>();
		TransformFrag.SetTransform(FTransform::Identity);

		FMassRenderISMFragment& ISMFrag = EntityView.GetFragmentData<FMassRenderISMFragment>();
		FMassRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FMassRenderPrimitiveFragment>();
		PrimFrag.bIsVisible = true;

		const FMassOverrideMaterialsFragment* OverrideMatsPtr =
			bHasOverrideMats ? &OverrideMats : nullptr;

		checkf(ISMFrag.InstancedStaticMeshSceneProxyDesc, TEXT("Default ctor should create desc"));
		UE::MassEngine::Mesh::InitializeInstanceStaticMeshSceneProxyDescFromFragment(
			StaticMeshFrag, VisMeshFrag, TransformFrag, *ISMFrag.InstancedStaticMeshSceneProxyDesc);

		// Add instance
		FInstancedStaticMeshInstanceData InstanceData;
		InstanceData.Transform = WorldTransform.ToMatrixWithScale();
		int32 SparseIdx = ISMFrag.PerInstanceSMData.Add(InstanceData);

		FArcIWInstanceFragment* SourceInstance =
			DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(Entity);
		if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(0))
		{
			SourceInstance->ISMInstanceIds[0] = SparseIdx;
		}

		PrimFrag.LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
			TransformFrag, ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
		PrimFrag.WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
			TransformFrag, ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);

		FMassRenderStateFragment& RenderState = EntityView.GetFragmentData<FMassRenderStateFragment>();
		RenderState.CreateRenderStateHelper<FMassISMRenderStateHelper>(
			HolderEntity, &DeferredEM, PrimFrag, OverrideMatsPtr, ISMFrag);

		Self->SimpleVisISMHolders[HolderSlot] = HolderEntity;
	});
}

void AArcIWMassISMPartitionActor::DeactivateSimpleVisMesh(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	FMassEntityManager& EntityManager)
{
	const int32 HolderSlot = Instance.MeshSlotBase;
	if (!SimpleVisISMHolders.IsValidIndex(HolderSlot))
	{
		return;
	}

	const int32 SparseIdx = Instance.ISMInstanceIds.IsValidIndex(0) ? Instance.ISMInstanceIds[0] : INDEX_NONE;
	if (SparseIdx == INDEX_NONE)
	{
		return;
	}

	FMassEntityHandle HolderEntity = SimpleVisISMHolders[HolderSlot];
	if (!HolderEntity.IsValid() || !EntityManager.IsEntityValid(HolderEntity))
	{
		Instance.ISMInstanceIds[0] = INDEX_NONE;
		return;
	}

	FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(HolderEntity);
	FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);
	FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

	if (ISMFrag && PrimFrag && RenderState)
	{
		if (ISMFrag->PerInstanceSMData.IsValidIndex(SparseIdx))
		{
			ISMFrag->PerInstanceSMData.RemoveAt(SparseIdx);
			if (ISMFrag->PerInstanceSMData.Num() == 0)
			{
				PrimFrag->bIsVisible = false;
			}
			RenderState->GetRenderStateHelper().MarkRenderStateDirty();
		}
	}

	Instance.ISMInstanceIds[0] = INDEX_NONE;
}

FMassEntityHandle AArcIWMassISMPartitionActor::ActivateSimpleVisSkinnedMesh(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWVisConfigFragment& Config,
	const FArcMassSkinnedMeshFragment& SkinnedMeshFragment,
	const FArcMassVisualizationMeshConfigFragment& MeshConfigFragment,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	const int32 HolderSlot = Instance.MeshSlotBase;
	if (!SimpleVisSkinnedISMHolders.IsValidIndex(HolderSlot))
	{
		return FMassEntityHandle();
	}

	if (Instance.ISMInstanceIds.IsValidIndex(0) && Instance.ISMInstanceIds[0] != INDEX_NONE)
	{
		return FMassEntityHandle();
	}

	const int32 DefaultAnimIndex = Config.MeshDescriptors.Num() > 0
		? Config.MeshDescriptors[0].DefaultAnimationIndex : 0;

	FMassEntityHandle HolderEntity = SimpleVisSkinnedISMHolders[HolderSlot];

	// Holder exists — add instance immediately
	if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
	{
		FArcMassRenderISMSkinnedFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(HolderEntity);
		FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);

		if (ISMFrag)
		{
			FArcSkinnedMeshInstanceData InstanceData;
			InstanceData.Transform = FTransform3f(WorldTransform);
			InstanceData.AnimationIndex = DefaultAnimIndex;
			int32 SparseIdx = ISMFrag->PerInstanceData.Add(InstanceData);

			if (Instance.ISMInstanceIds.IsValidIndex(0))
			{
				Instance.ISMInstanceIds[0] = SparseIdx;
			}

			if (PrimFrag)
			{
				PrimFrag->bIsVisible = true;
			}
			ISMFrag->bRenderStateDirty = true;
		}

		return HolderEntity;
	}

	// Holder doesn't exist — defer creation
	USkinnedAsset* Asset = SkinnedMeshFragment.SkinnedAsset.Get();
	if (!Asset)
	{
		return FMassEntityHandle();
	}

	bool bHasOverrideMats = false;
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
	if (Config.MeshDescriptors.Num() > 0)
	{
		for (const TObjectPtr<UMaterialInterface>& Mat : Config.MeshDescriptors[0].Materials)
		{
			if (Mat) { bHasOverrideMats = true; break; }
		}
		if (bHasOverrideMats)
		{
			OverrideMaterials = Config.MeshDescriptors[0].Materials;
		}
	}

	UArcIWVisualizationSubsystem* VisSubsystem = GetWorld()->GetSubsystem<UArcIWVisualizationSubsystem>();
	FMassArchetypeHandle HolderArchetype;
	if (VisSubsystem)
	{
		const UArcIWVisualizationSubsystem::FCachedEntityTemplate& Cached =
			VisSubsystem->EnsureEntityTemplate(ActorClassEntries[Instance.ClassIndex], *GetWorld());
		HolderArchetype = Cached.HolderArchetype;
	}

	TWeakObjectPtr<AArcIWMassISMPartitionActor> WeakSelf = this;
	FArcMassSkinnedMeshFragment CapturedSkinnedFrag = SkinnedMeshFragment;
	FMassVisualizationMeshFragment CapturedMeshFrag = ArcMass::Visualization::ToEngineFragment(MeshConfigFragment);

	Context.Defer().PushCommand<FMassDeferredAddCommand>(
	[WeakSelf, Entity, HolderSlot, WorldTransform, DefaultAnimIndex, bHasOverrideMats,
	 OverrideMaterials = MoveTemp(OverrideMaterials),
	 CapturedSkinnedFrag = MoveTemp(CapturedSkinnedFrag),
	 CapturedMeshFrag = MoveTemp(CapturedMeshFrag),
	 HolderArchetype](FMassEntityManager& DeferredEM)
	{
		AArcIWMassISMPartitionActor* Self = WeakSelf.Get();
		if (!Self) return;
		if (!DeferredEM.IsEntityValid(Entity)) return;

		// Check if holder was created by a previous deferred command
		FMassEntityHandle ExistingHolder = Self->SimpleVisSkinnedISMHolders[HolderSlot];
		if (ExistingHolder.IsValid() && DeferredEM.IsEntityValid(ExistingHolder))
		{
			FArcMassRenderISMSkinnedFragment* ISMFrag =
				DeferredEM.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(ExistingHolder);
			FMassRenderPrimitiveFragment* PrimFrag =
				DeferredEM.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(ExistingHolder);

			if (ISMFrag)
			{
				FArcSkinnedMeshInstanceData InstanceData;
				InstanceData.Transform = FTransform3f(WorldTransform);
				InstanceData.AnimationIndex = DefaultAnimIndex;
				int32 SparseIdx = ISMFrag->PerInstanceData.Add(InstanceData);

				FArcIWInstanceFragment* SourceInstance =
					DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(Entity);
				if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(0))
				{
					SourceInstance->ISMInstanceIds[0] = SparseIdx;
				}

				if (PrimFrag)
				{
					PrimFrag->bIsVisible = true;
				}

				// Flush render state inline since FlushDirtyHolders already ran
				if (ISMFrag->RenderStateHelper)
				{
					ISMFrag->RenderStateHelper->DestroyRenderState(nullptr);
					if (ISMFrag->PerInstanceData.Num() > 0)
					{
						ISMFrag->RenderStateHelper->CreateRenderState(nullptr);
					}
					ISMFrag->bRenderStateDirty = false;
				}
			}
			return;
		}

		// Create holder entity
		FMassArchetypeHandle Archetype = HolderArchetype;

		FMassArchetypeSharedFragmentValues SharedValues;
		SharedValues.Add(FConstSharedStruct::Make(CapturedSkinnedFrag));
		SharedValues.Add(FConstSharedStruct::Make(CapturedMeshFrag));

		FMassOverrideMaterialsFragment OverrideMats;
		if (bHasOverrideMats)
		{
			OverrideMats.OverrideMaterials = OverrideMaterials;
			SharedValues.Add(FConstSharedStruct::Make(OverrideMats));
		}
		SharedValues.Sort();

		TArray<FMassEntityHandle> CreatedEntities;
		TSharedRef<FMassObserverManager::FCreationContext> CreationContext =
			DeferredEM.BatchCreateEntities(Archetype, SharedValues, 1, CreatedEntities);

		if (CreatedEntities.IsEmpty()) return;

		FMassEntityHandle HolderEntity = CreatedEntities[0];
		FMassEntityView EntityView(DeferredEM, HolderEntity);

		FTransformFragment& TransformFrag = EntityView.GetFragmentData<FTransformFragment>();
		TransformFrag.SetTransform(FTransform::Identity);

		USkinnedAsset* Asset = CapturedSkinnedFrag.SkinnedAsset.Get();
		if (!Asset) return;

		FArcMassRenderISMSkinnedFragment& ISMFrag = EntityView.GetFragmentData<FArcMassRenderISMSkinnedFragment>();
		ISMFrag.SceneProxyDesc = MakeShared<FInstancedSkinnedMeshSceneProxyDesc>();

		FInstancedSkinnedMeshSceneProxyDesc& Desc = *ISMFrag.SceneProxyDesc;
		Desc.SkinnedAsset = Asset;
		Desc.TransformProvider = CapturedSkinnedFrag.TransformProvider;
		Desc.CastShadow = CapturedMeshFrag.CastShadow;

		FSkeletalMeshRenderData* RenderData = Asset->GetResourceForRendering();
		UWorld* World = Self->GetWorld();
		if (RenderData && World && World->Scene)
		{
			Desc.World = World;
			Desc.Scene = World->Scene;
			ISMFrag.MeshObject = FInstancedSkinnedMeshSceneProxyDesc::CreateMeshObject(
				Desc, RenderData, World->Scene->GetFeatureLevel());
		}

		FMassRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FMassRenderPrimitiveFragment>();
		PrimFrag.bIsVisible = true;
		PrimFrag.LocalBounds = Asset->GetBounds();

		// Add instance
		FArcSkinnedMeshInstanceData InstanceData;
		InstanceData.Transform = FTransform3f(WorldTransform);
		InstanceData.AnimationIndex = DefaultAnimIndex;
		int32 SparseIdx = ISMFrag.PerInstanceData.Add(InstanceData);

		FArcIWInstanceFragment* SourceInstance =
			DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(Entity);
		if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(0))
		{
			SourceInstance->ISMInstanceIds[0] = SparseIdx;
		}

		const FMassOverrideMaterialsFragment* OverrideMatsPtr =
			bHasOverrideMats ? &OverrideMats : nullptr;

		ISMFrag.RenderStateHelper = MakeShared<FArcMassInstancedSkinnedMeshRenderStateHelper>(
			HolderEntity, &DeferredEM, PrimFrag, OverrideMatsPtr);

		// Create render state immediately — FlushDirtyHolders already ran before deferred
		ISMFrag.RenderStateHelper->CreateRenderState(nullptr);
		ISMFrag.bRenderStateDirty = false;

		Self->SimpleVisSkinnedISMHolders[HolderSlot] = HolderEntity;
	});

	// Holder will be created in deferred — caller cannot track dirty state
	return FMassEntityHandle();
}

FMassEntityHandle AArcIWMassISMPartitionActor::DeactivateSimpleVisSkinnedMesh(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	FMassEntityManager& EntityManager)
{
	const int32 HolderSlot = Instance.MeshSlotBase;
	if (!SimpleVisSkinnedISMHolders.IsValidIndex(HolderSlot))
	{
		return FMassEntityHandle();
	}

	const int32 SparseIdx = Instance.ISMInstanceIds.IsValidIndex(0) ? Instance.ISMInstanceIds[0] : INDEX_NONE;
	if (SparseIdx == INDEX_NONE)
	{
		return FMassEntityHandle();
	}

	FMassEntityHandle HolderEntity = SimpleVisSkinnedISMHolders[HolderSlot];
	if (!HolderEntity.IsValid() || !EntityManager.IsEntityValid(HolderEntity))
	{
		Instance.ISMInstanceIds[0] = INDEX_NONE;
		return FMassEntityHandle();
	}

	FArcMassRenderISMSkinnedFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(HolderEntity);
	FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);

	if (ISMFrag)
	{
		if (ISMFrag->PerInstanceData.IsValidIndex(SparseIdx))
		{
			ISMFrag->PerInstanceData.RemoveAt(SparseIdx);
			if (ISMFrag->PerInstanceData.Num() == 0 && PrimFrag)
			{
				PrimFrag->bIsVisible = false;
			}
			ISMFrag->bRenderStateDirty = true;
		}
	}

	Instance.ISMInstanceIds[0] = INDEX_NONE;
	return HolderEntity;
}

void AArcIWMassISMPartitionActor::ActivateSimpleVisMesh(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWMeshEntry& ExplicitMeshEntry,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	const int32 HolderSlot = Instance.MeshSlotBase;
	if (!SimpleVisISMHolders.IsValidIndex(HolderSlot))
	{
		return;
	}

	// Already has an ISM instance
	if (Instance.ISMInstanceIds.IsValidIndex(0) && Instance.ISMInstanceIds[0] != INDEX_NONE)
	{
		return;
	}

	FMassEntityHandle HolderEntity = SimpleVisISMHolders[HolderSlot];

	// Holder exists — add instance immediately
	if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
	{
		FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(HolderEntity);
		FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);
		FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

		if (ISMFrag && PrimFrag && RenderState)
		{
			FInstancedStaticMeshInstanceData InstanceData;
			InstanceData.Transform = WorldTransform.ToMatrixWithScale();
			int32 SparseIdx = ISMFrag->PerInstanceSMData.Add(InstanceData);

			if (Instance.ISMInstanceIds.IsValidIndex(0))
			{
				Instance.ISMInstanceIds[0] = SparseIdx;
			}

			FTransformFragment* HolderTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(HolderEntity);
			if (HolderTransform)
			{
				PrimFrag->LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
					*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
				PrimFrag->WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
					*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);
			}

			PrimFrag->bIsVisible = true;
			RenderState->GetRenderStateHelper().MarkRenderStateDirty();
		}
		return;
	}

	// Holder doesn't exist — defer creation
	if (!ExplicitMeshEntry.Mesh)
	{
		return;
	}

	bool bHasOverrideMats = false;
	for (const TObjectPtr<UMaterialInterface>& Mat : ExplicitMeshEntry.Materials)
	{
		if (Mat) { bHasOverrideMats = true; break; }
	}

	FMassArchetypeHandle HolderArchetype;
	UMassEntitySubsystem* EntitySubsystem = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (EntitySubsystem)
	{
		FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();
		FMassElementBitSet Composition;
		Composition.Add<FTransformFragment>();
		Composition.Add<FMassRenderStateFragment>();
		Composition.Add<FMassRenderPrimitiveFragment>();
		Composition.Add<FMassRenderISMFragment>();
		if (bHasOverrideMats)
		{
			Composition.Add<FMassOverrideMaterialsFragment>();
		}
		HolderArchetype = EM.CreateArchetype(Composition,
			FMassArchetypeCreationParams{bHasOverrideMats
				? TEXT("ArcIWMassISMHolderMats") : TEXT("ArcIWMassISMHolder")});
	}

	TWeakObjectPtr<AArcIWMassISMPartitionActor> WeakSelf = this;
	FArcIWMeshEntry CapturedMeshEntry = ExplicitMeshEntry;
	Context.Defer().PushCommand<FMassDeferredAddCommand>(
	[WeakSelf, Entity, HolderSlot, WorldTransform, CapturedMeshEntry, bHasOverrideMats, HolderArchetype](FMassEntityManager& DeferredEM)
	{
		AArcIWMassISMPartitionActor* Self = WeakSelf.Get();
		if (!Self) return;
		if (!DeferredEM.IsEntityValid(Entity)) return;

		// Check if holder was created by a previous deferred command
		FMassEntityHandle ExistingHolder = Self->SimpleVisISMHolders[HolderSlot];
		if (ExistingHolder.IsValid() && DeferredEM.IsEntityValid(ExistingHolder))
		{
			FMassRenderISMFragment* ISMFrag = DeferredEM.GetFragmentDataPtr<FMassRenderISMFragment>(ExistingHolder);
			FMassRenderPrimitiveFragment* PrimFrag = DeferredEM.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(ExistingHolder);
			FMassRenderStateFragment* RenderState = DeferredEM.GetFragmentDataPtr<FMassRenderStateFragment>(ExistingHolder);

			if (ISMFrag && PrimFrag && RenderState)
			{
				FInstancedStaticMeshInstanceData InstanceData;
				InstanceData.Transform = WorldTransform.ToMatrixWithScale();
				int32 SparseIdx = ISMFrag->PerInstanceSMData.Add(InstanceData);

				FArcIWInstanceFragment* SourceInstance =
					DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(Entity);
				if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(0))
				{
					SourceInstance->ISMInstanceIds[0] = SparseIdx;
				}

				FTransformFragment* HolderTransform = DeferredEM.GetFragmentDataPtr<FTransformFragment>(ExistingHolder);
				if (HolderTransform)
				{
					PrimFrag->LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
						*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
					PrimFrag->WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
						*HolderTransform, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);
				}

				PrimFrag->bIsVisible = true;
				RenderState->GetRenderStateHelper().MarkRenderStateDirty();
			}
			return;
		}

		// Create holder entity
		FMassArchetypeHandle Archetype = HolderArchetype;

		FMassStaticMeshFragment StaticMeshFrag(CapturedMeshEntry.Mesh);
		FMassVisualizationMeshFragment VisMeshFrag;
		VisMeshFrag.CastShadow = CapturedMeshEntry.bCastShadows;

		FMassArchetypeSharedFragmentValues SharedValues;
		SharedValues.Add(FConstSharedStruct::Make(StaticMeshFrag));
		SharedValues.Add(FConstSharedStruct::Make(VisMeshFrag));

		FMassOverrideMaterialsFragment OverrideMats;
		if (bHasOverrideMats)
		{
			OverrideMats.OverrideMaterials = CapturedMeshEntry.Materials;
			SharedValues.Add(FConstSharedStruct::Make(OverrideMats));
		}
		SharedValues.Sort();

		TArray<FMassEntityHandle> CreatedEntities;
		TSharedRef<FMassObserverManager::FCreationContext> CreationContext =
			DeferredEM.BatchCreateEntities(Archetype, SharedValues, 1, CreatedEntities);

		if (CreatedEntities.IsEmpty()) return;

		FMassEntityHandle HolderEntity = CreatedEntities[0];
		FMassEntityView EntityView(DeferredEM, HolderEntity);

		FTransformFragment& TransformFrag = EntityView.GetFragmentData<FTransformFragment>();
		TransformFrag.SetTransform(FTransform::Identity);

		FMassRenderISMFragment& ISMFrag = EntityView.GetFragmentData<FMassRenderISMFragment>();
		FMassRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FMassRenderPrimitiveFragment>();
		PrimFrag.bIsVisible = true;

		const FMassOverrideMaterialsFragment* OverrideMatsPtr =
			bHasOverrideMats ? &OverrideMats : nullptr;

		checkf(ISMFrag.InstancedStaticMeshSceneProxyDesc, TEXT("Default ctor should create desc"));
		UE::MassEngine::Mesh::InitializeInstanceStaticMeshSceneProxyDescFromFragment(
			StaticMeshFrag, VisMeshFrag, TransformFrag, *ISMFrag.InstancedStaticMeshSceneProxyDesc);

		// Add instance
		FInstancedStaticMeshInstanceData InstanceData;
		InstanceData.Transform = WorldTransform.ToMatrixWithScale();
		int32 SparseIdx = ISMFrag.PerInstanceSMData.Add(InstanceData);

		FArcIWInstanceFragment* SourceInstance =
			DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(Entity);
		if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(0))
		{
			SourceInstance->ISMInstanceIds[0] = SparseIdx;
		}

		PrimFrag.LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
			TransformFrag, ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
		PrimFrag.WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
			TransformFrag, ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);

		FMassRenderStateFragment& RenderState = EntityView.GetFragmentData<FMassRenderStateFragment>();
		RenderState.CreateRenderStateHelper<FMassISMRenderStateHelper>(
			HolderEntity, &DeferredEM, PrimFrag, OverrideMatsPtr, ISMFrag);

		Self->SimpleVisISMHolders[HolderSlot] = HolderEntity;
	});
}

FMassEntityHandle AArcIWMassISMPartitionActor::ActivateSimpleVisSkinnedMesh(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWMeshEntry& ExplicitMeshEntry,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	const int32 HolderSlot = Instance.MeshSlotBase;
	if (!SimpleVisSkinnedISMHolders.IsValidIndex(HolderSlot))
	{
		return FMassEntityHandle();
	}

	if (Instance.ISMInstanceIds.IsValidIndex(0) && Instance.ISMInstanceIds[0] != INDEX_NONE)
	{
		return FMassEntityHandle();
	}

	const int32 DefaultAnimIndex = ExplicitMeshEntry.DefaultAnimationIndex;

	FMassEntityHandle HolderEntity = SimpleVisSkinnedISMHolders[HolderSlot];

	// Holder exists — add instance immediately
	if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
	{
		FArcMassRenderISMSkinnedFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(HolderEntity);
		FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);

		if (ISMFrag)
		{
			FArcSkinnedMeshInstanceData InstanceData;
			InstanceData.Transform = FTransform3f(WorldTransform);
			InstanceData.AnimationIndex = DefaultAnimIndex;
			int32 SparseIdx = ISMFrag->PerInstanceData.Add(InstanceData);

			if (Instance.ISMInstanceIds.IsValidIndex(0))
			{
				Instance.ISMInstanceIds[0] = SparseIdx;
			}

			if (PrimFrag)
			{
				PrimFrag->bIsVisible = true;
			}
			ISMFrag->bRenderStateDirty = true;
		}

		return HolderEntity;
	}

	// Holder doesn't exist — defer creation
	USkinnedAsset* Asset = ExplicitMeshEntry.SkinnedAsset.Get();
	if (!Asset)
	{
		return FMassEntityHandle();
	}

	bool bHasOverrideMats = false;
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
	for (const TObjectPtr<UMaterialInterface>& Mat : ExplicitMeshEntry.Materials)
	{
		if (Mat) { bHasOverrideMats = true; break; }
	}
	if (bHasOverrideMats)
	{
		OverrideMaterials = ExplicitMeshEntry.Materials;
	}

	FArcMassSkinnedMeshFragment CapturedSkinnedFrag;
	CapturedSkinnedFrag.SkinnedAsset = ExplicitMeshEntry.SkinnedAsset;
	CapturedSkinnedFrag.TransformProvider = ExplicitMeshEntry.TransformProvider;

	FMassVisualizationMeshFragment CapturedMeshFrag;
	CapturedMeshFrag.CastShadow = ExplicitMeshEntry.bCastShadows;

	FMassArchetypeHandle HolderArchetype;
	UMassEntitySubsystem* EntitySubsystem = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (EntitySubsystem)
	{
		FMassEntityManager& EM = EntitySubsystem->GetMutableEntityManager();
		FMassElementBitSet Composition;
		Composition.Add<FTransformFragment>();
		Composition.Add<FArcMassRenderISMSkinnedFragment>();
		Composition.Add<FMassRenderPrimitiveFragment>();
		if (bHasOverrideMats)
		{
			Composition.Add<FMassOverrideMaterialsFragment>();
		}
		HolderArchetype = EM.CreateArchetype(Composition,
			FMassArchetypeCreationParams{bHasOverrideMats
				? TEXT("ArcIWSkinnedISMHolderMats") : TEXT("ArcIWSkinnedISMHolder")});
	}

	TWeakObjectPtr<AArcIWMassISMPartitionActor> WeakSelf = this;
	Context.Defer().PushCommand<FMassDeferredAddCommand>(
	[WeakSelf, Entity, HolderSlot, WorldTransform, DefaultAnimIndex, bHasOverrideMats,
	 OverrideMaterials = MoveTemp(OverrideMaterials),
	 CapturedSkinnedFrag = MoveTemp(CapturedSkinnedFrag),
	 CapturedMeshFrag = MoveTemp(CapturedMeshFrag),
	 HolderArchetype](FMassEntityManager& DeferredEM)
	{
		AArcIWMassISMPartitionActor* Self = WeakSelf.Get();
		if (!Self) return;
		if (!DeferredEM.IsEntityValid(Entity)) return;

		// Check if holder was created by a previous deferred command
		FMassEntityHandle ExistingHolder = Self->SimpleVisSkinnedISMHolders[HolderSlot];
		if (ExistingHolder.IsValid() && DeferredEM.IsEntityValid(ExistingHolder))
		{
			FArcMassRenderISMSkinnedFragment* ISMFrag =
				DeferredEM.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(ExistingHolder);
			FMassRenderPrimitiveFragment* PrimFrag =
				DeferredEM.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(ExistingHolder);

			if (ISMFrag)
			{
				FArcSkinnedMeshInstanceData InstanceData;
				InstanceData.Transform = FTransform3f(WorldTransform);
				InstanceData.AnimationIndex = DefaultAnimIndex;
				int32 SparseIdx = ISMFrag->PerInstanceData.Add(InstanceData);

				FArcIWInstanceFragment* SourceInstance =
					DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(Entity);
				if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(0))
				{
					SourceInstance->ISMInstanceIds[0] = SparseIdx;
				}

				if (PrimFrag)
				{
					PrimFrag->bIsVisible = true;
				}

				// Flush render state inline since FlushDirtyHolders already ran
				if (ISMFrag->RenderStateHelper)
				{
					ISMFrag->RenderStateHelper->DestroyRenderState(nullptr);
					if (ISMFrag->PerInstanceData.Num() > 0)
					{
						ISMFrag->RenderStateHelper->CreateRenderState(nullptr);
					}
					ISMFrag->bRenderStateDirty = false;
				}
			}
			return;
		}

		// Create holder entity
		FMassArchetypeHandle Archetype = HolderArchetype;

		FMassArchetypeSharedFragmentValues SharedValues;
		SharedValues.Add(FConstSharedStruct::Make(CapturedSkinnedFrag));
		SharedValues.Add(FConstSharedStruct::Make(CapturedMeshFrag));

		FMassOverrideMaterialsFragment OverrideMats;
		if (bHasOverrideMats)
		{
			OverrideMats.OverrideMaterials = OverrideMaterials;
			SharedValues.Add(FConstSharedStruct::Make(OverrideMats));
		}
		SharedValues.Sort();

		TArray<FMassEntityHandle> CreatedEntities;
		TSharedRef<FMassObserverManager::FCreationContext> CreationContext =
			DeferredEM.BatchCreateEntities(Archetype, SharedValues, 1, CreatedEntities);

		if (CreatedEntities.IsEmpty()) return;

		FMassEntityHandle HolderEntity = CreatedEntities[0];
		FMassEntityView EntityView(DeferredEM, HolderEntity);

		FTransformFragment& TransformFrag = EntityView.GetFragmentData<FTransformFragment>();
		TransformFrag.SetTransform(FTransform::Identity);

		USkinnedAsset* Asset = CapturedSkinnedFrag.SkinnedAsset.Get();
		if (!Asset) return;

		FArcMassRenderISMSkinnedFragment& ISMFrag = EntityView.GetFragmentData<FArcMassRenderISMSkinnedFragment>();
		ISMFrag.SceneProxyDesc = MakeShared<FInstancedSkinnedMeshSceneProxyDesc>();

		FInstancedSkinnedMeshSceneProxyDesc& Desc = *ISMFrag.SceneProxyDesc;
		Desc.SkinnedAsset = Asset;
		Desc.TransformProvider = CapturedSkinnedFrag.TransformProvider;
		Desc.CastShadow = CapturedMeshFrag.CastShadow;

		FSkeletalMeshRenderData* RenderData = Asset->GetResourceForRendering();
		UWorld* World = Self->GetWorld();
		if (RenderData && World && World->Scene)
		{
			Desc.World = World;
			Desc.Scene = World->Scene;
			ISMFrag.MeshObject = FInstancedSkinnedMeshSceneProxyDesc::CreateMeshObject(
				Desc, RenderData, World->Scene->GetFeatureLevel());
		}

		FMassRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FMassRenderPrimitiveFragment>();
		PrimFrag.bIsVisible = true;
		PrimFrag.LocalBounds = Asset->GetBounds();

		// Add instance
		FArcSkinnedMeshInstanceData InstanceData;
		InstanceData.Transform = FTransform3f(WorldTransform);
		InstanceData.AnimationIndex = DefaultAnimIndex;
		int32 SparseIdx = ISMFrag.PerInstanceData.Add(InstanceData);

		FArcIWInstanceFragment* SourceInstance =
			DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(Entity);
		if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(0))
		{
			SourceInstance->ISMInstanceIds[0] = SparseIdx;
		}

		const FMassOverrideMaterialsFragment* OverrideMatsPtr =
			bHasOverrideMats ? &OverrideMats : nullptr;

		ISMFrag.RenderStateHelper = MakeShared<FArcMassInstancedSkinnedMeshRenderStateHelper>(
			HolderEntity, &DeferredEM, PrimFrag, OverrideMatsPtr);

		// Create render state immediately — FlushDirtyHolders already ran before deferred
		ISMFrag.RenderStateHelper->CreateRenderState(nullptr);
		ISMFrag.bRenderStateDirty = false;

		Self->SimpleVisSkinnedISMHolders[HolderSlot] = HolderEntity;
	});

	// Holder will be created in deferred — caller cannot track dirty state
	return FMassEntityHandle();
}

void AArcIWMassISMPartitionActor::SwapSimpleVisMesh(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWMeshEntry& NewMeshEntry,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	DeactivateSimpleVisMesh(Entity, Instance, EntityManager);
	ActivateSimpleVisMesh(Entity, Instance, NewMeshEntry, WorldTransform, EntityManager, Context);
}

FMassEntityHandle AArcIWMassISMPartitionActor::SwapSimpleVisSkinnedMesh(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWMeshEntry& NewMeshEntry,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	FMassEntityHandle OldHolder = DeactivateSimpleVisSkinnedMesh(Entity, Instance, EntityManager);
	FMassEntityHandle NewHolder = ActivateSimpleVisSkinnedMesh(Entity, Instance, NewMeshEntry, WorldTransform, EntityManager, Context);
	return NewHolder.IsValid() ? NewHolder : OldHolder;
}

// ---------------------------------------------------------------------------
// ActivateEntity / DeactivateEntity  (convenience wrappers)
// ---------------------------------------------------------------------------

void AArcIWMassISMPartitionActor::ActivateEntity(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWVisConfigFragment& Config,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	ActivateMesh(Entity, Instance, Config, WorldTransform, EntityManager, Context);
}

void AArcIWMassISMPartitionActor::DeactivateEntity(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWVisConfigFragment& Config,
	FMassEntityManager& EntityManager)
{
	DeactivateMesh(Entity, Instance, Config, EntityManager);
}

// ---------------------------------------------------------------------------
// Hydration
// ---------------------------------------------------------------------------

void AArcIWMassISMPartitionActor::HydrateEntity(FMassEntityHandle EntityHandle)
{
	const UArcIWSettings* Settings = UArcIWSettings::Get();
	if (Settings->bDisableActorHydration)
	{
		return;
	}

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

	// Check if this is a simple vis entity (has render fragments, no ISM holders)
	FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(EntityHandle);
	FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(EntityHandle);

	if (PrimFrag && RenderState)
	{
		// Simple vis entity: hide the directly-rendered mesh
		PrimFrag->bIsVisible = false;
		RenderState->GetRenderStateHelper().MarkRenderStateDirty();
	}
	else
	{
		// Composite entity: remove ISM instances
		DeactivateMesh(EntityHandle, *Instance, *Config, EntityManager);
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

int32 AArcIWMassISMPartitionActor::ConvertCollisionIndexToInstanceIndex(
	int32 InIndex, const UPrimitiveComponent* RelevantComponent) const
{
	// No component-based resolution -- standalone bodies use ArcMassPhysicsEntityLink instead.
	return INDEX_NONE;
}

AActor* AArcIWMassISMPartitionActor::FindActor(const FActorInstanceHandle& Handle)
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

AActor* AArcIWMassISMPartitionActor::FindOrCreateActor(const FActorInstanceHandle& Handle)
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

UClass* AArcIWMassISMPartitionActor::GetRepresentedClass(int32 InstanceIndex) const
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

ULevel* AArcIWMassISMPartitionActor::GetLevelForInstance(int32 InstanceIndex) const
{
	return GetLevel();
}

FTransform AArcIWMassISMPartitionActor::GetTransform(const FActorInstanceHandle& Handle) const
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

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

#if WITH_EDITOR
void AArcIWMassISMPartitionActor::AddActorInstance(
	TSubclassOf<AActor> ActorClass,
	const FTransform& WorldTransform,
	const TArray<FArcIWMeshEntry>& MeshDescriptors,
	UMassEntityConfigAsset* InAdditionalConfig,
	UBodySetup* InCollisionBodySetup,
	const FCollisionProfileName& InCollisionProfile,
	const FBodyInstance& InBodyTemplate,
	int32 InRespawnTimeSeconds,
	EArcMassPhysicsBodyType InPhysicsBodyType,
	UPCGComponent* InSourcePCGComponent)
{
	// Find existing entry for this (ActorClass, SourcePCGComponent) pair.
	FArcIWActorClassData* FoundEntry = nullptr;
	for (FArcIWActorClassData& Entry : ActorClassEntries)
	{
		if (Entry.ActorClass == ActorClass && Entry.SourcePCGComponent.Get() == InSourcePCGComponent)
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
		NewEntry.CollisionBodySetup = InCollisionBodySetup && InCollisionBodySetup->GetOuter() != this
			? DuplicateObject(InCollisionBodySetup, this)
			: InCollisionBodySetup;
		NewEntry.CollisionProfile = InCollisionProfile;
		NewEntry.BodyTemplate = InBodyTemplate;
		NewEntry.RespawnTimeSeconds = InRespawnTimeSeconds;
		NewEntry.PhysicsBodyType = InPhysicsBodyType;
		NewEntry.bFromPCG = (InSourcePCGComponent != nullptr);
		NewEntry.SourcePCGComponent = InSourcePCGComponent;
		FoundEntry = &NewEntry;
	}

	FoundEntry->InstanceTransforms.Add(WorldTransform);

	Modify();
}

int32 AArcIWMassISMPartitionActor::RemoveActorInstances(
	TSubclassOf<AActor> ActorClass,
	const TArray<int32>& SortedTransformIndices)
{
	int32 ClassIdx = INDEX_NONE;
	for (int32 Idx = 0; Idx < ActorClassEntries.Num(); ++Idx)
	{
		if (ActorClassEntries[Idx].ActorClass == ActorClass)
		{
			ClassIdx = Idx;
			break;
		}
	}

	if (ClassIdx == INDEX_NONE)
	{
		return 0;
	}

	FArcIWActorClassData& ClassData = ActorClassEntries[ClassIdx];
	int32 RemoveCount = 0;

	// Remove in reverse order so earlier indices stay valid.
	for (int32 i = SortedTransformIndices.Num() - 1; i >= 0; --i)
	{
		int32 TransformIdx = SortedTransformIndices[i];
		if (ClassData.InstanceTransforms.IsValidIndex(TransformIdx))
		{
			ClassData.InstanceTransforms.RemoveAtSwap(TransformIdx);
			++RemoveCount;
		}
	}

	// If no transforms remain, remove the entire class entry.
	if (ClassData.InstanceTransforms.IsEmpty())
	{
		ActorClassEntries.RemoveAtSwap(ClassIdx);
	}

	if (RemoveCount > 0)
	{
		Modify();
	}

	return RemoveCount;
}

int32 AArcIWMassISMPartitionActor::RemoveEntriesBySource(UPCGComponent* SourcePCGComponent)
{
	if (!SourcePCGComponent)
	{
		return 0;
	}

	int32 RemoveCount = 0;
	for (int32 Idx = ActorClassEntries.Num() - 1; Idx >= 0; --Idx)
	{
		if (ActorClassEntries[Idx].SourcePCGComponent.Get() == SourcePCGComponent)
		{
			ActorClassEntries.RemoveAtSwap(Idx);
			++RemoveCount;
		}
	}

	if (RemoveCount > 0)
	{
		Modify();
	}

	return RemoveCount;
}

bool AArcIWMassISMPartitionActor::RemoveStaleEntries()
{
	int32 RemoveCount = 0;
	for (int32 Idx = ActorClassEntries.Num() - 1; Idx >= 0; --Idx)
	{
		FArcIWActorClassData& Entry = ActorClassEntries[Idx];
		// Remove PCG entries whose source component no longer exists.
		if (Entry.bFromPCG && !Entry.SourcePCGComponent.IsValid())
		{
			ActorClassEntries.RemoveAtSwap(Idx);
			++RemoveCount;
		}
	}

	if (RemoveCount > 0)
	{
		Modify();
	}

	return RemoveCount > 0;
}

void AArcIWMassISMPartitionActor::RebuildEditorPreviewISMCs()
{
	// Purge entries from destroyed PCG components before rebuilding previews.
	RemoveStaleEntries();

	if (ActorClassEntries.IsEmpty())
	{
		DestroyEditorPreviewISMCs();
		return;
	}

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

		// Build a map of skinned asset -> skinned ISM component
		TMap<FObjectKey, UInstancedSkinnedMeshComponent*> SkinnedAssetToISMC;

		for (const FArcIWMeshEntry& MeshEntry : ClassData.MeshDescriptors)
		{
			if (!MeshEntry.IsSkinned())
			{
				continue;
			}

			const FObjectKey AssetKey(MeshEntry.SkinnedAsset);
			if (!SkinnedAssetToISMC.Contains(AssetKey))
			{
				UInstancedSkinnedMeshComponent* NewSkinned = NewObject<UInstancedSkinnedMeshComponent>(this);
				NewSkinned->SetSkinnedAssetAndUpdate(MeshEntry.SkinnedAsset);
				NewSkinned->SetTransformProvider(MeshEntry.TransformProvider);
				NewSkinned->SetCastShadow(MeshEntry.bCastShadows);
				NewSkinned->SetMobility(EComponentMobility::Static);
				NewSkinned->bIsEditorOnly = true;
				NewSkinned->SetAbsolute(true, true, true);

				for (int32 MatIdx = 0; MatIdx < MeshEntry.Materials.Num(); ++MatIdx)
				{
					if (MeshEntry.Materials[MatIdx])
					{
						NewSkinned->SetMaterial(MatIdx, MeshEntry.Materials[MatIdx]);
					}
				}

				NewSkinned->RegisterComponent();
				NewSkinned->SetWorldTransform(FTransform::Identity);
				AddInstanceComponent(NewSkinned);

				SkinnedAssetToISMC.Add(AssetKey, NewSkinned);
				EditorPreviewSkinnedISMCs.Add(NewSkinned);
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

			for (const FArcIWMeshEntry& MeshEntry : ClassData.MeshDescriptors)
			{
				if (!MeshEntry.IsSkinned())
				{
					continue;
				}

				const FObjectKey AssetKey(MeshEntry.SkinnedAsset);
				UInstancedSkinnedMeshComponent** FoundSkinned = SkinnedAssetToISMC.Find(AssetKey);
				if (FoundSkinned && *FoundSkinned)
				{
					const FTransform FinalTransform = MeshEntry.RelativeTransform * InstanceTransform;
					(*FoundSkinned)->AddInstance(FinalTransform, MeshEntry.DefaultAnimationIndex, /*bWorldSpace=*/true);
				}
			}
		}
	}
}

void AArcIWMassISMPartitionActor::DestroyEditorPreviewISMCs()
{
	TArray<UInstancedStaticMeshComponent*> ISMComponenets;
	GetComponents(ISMComponenets);
	for (UInstancedStaticMeshComponent* ISMComponent : ISMComponenets)
	{
		ISMComponent->DestroyComponent();
	}

	TArray<UInstancedSkinnedMeshComponent*> SkinnedComponents;
	GetComponents(SkinnedComponents);
	for (UInstancedSkinnedMeshComponent* SkinnedComponent : SkinnedComponents)
	{
		SkinnedComponent->DestroyComponent();
	}

	EditorPreviewISMCs.Empty();
	EditorPreviewSkinnedISMCs.Empty();
}
#endif

