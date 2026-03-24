// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWMassISMPartitionActor.h"

#include "ArcIWActorPoolSubsystem.h"
#include "ArcIWSettings.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityFragments.h"
#include "MassObserverManager.h"
#include "Engine/ActorInstanceHandle.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"

// Mass MeshEngine fragments and utilities
#include "Mesh/MassEngineMeshFragments.h"
#include "Mesh/MassEngineMeshUtils.h"

// MassEngine Private headers -- required for render state helper creation.
// ArcInstancedWorld.Build.cs adds the Private include path for MassEngine.
#include "MassRenderStateHelper.h"
#include "MassISMRenderStateHelper.h"

// Deferred commands and execution context
#include "MassExecutionContext.h"
#include "MassCommands.h"

// Physics entity link, Chaos user data, and ArcMass physics body fragment
#include "MassEntityView.h"
#include "ArcMass/ArcMassPhysicsEntityLink.h"
#include "ArcMass/ArcMassPhysicsBody.h"
#include "ArcMass/ArcMassPhysicsLink.h"
#include "Chaos/ChaosUserEntity.h"
#include "Engine/World.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

// WorldPartition for GetGridCellSize
#include "MassSpawnerSubsystem.h"
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
	SpawnEntities();
}

void AArcIWMassISMPartitionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DespawnEntities();
	DestroyISMHolderEntities();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// ISM Holder Entity Management (Mass MeshEngine rendering)
// ---------------------------------------------------------------------------

void AArcIWMassISMPartitionActor::InitializeISMState()
{
	UWorld* World = GetWorld();
	if (!World) return;
	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem) return;
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	int32 TotalMeshSlots = 0;
	for (const FArcIWActorClassData& ClassData : ActorClassEntries)
	{
		TotalMeshSlots += ClassData.MeshDescriptors.Num();
	}
	ISMHolderEntities.SetNumZeroed(TotalMeshSlots);

	FMassElementBitSet Composition;
	Composition.Add<FTransformFragment>();
	Composition.Add<FMassRenderStateFragment>();
	Composition.Add<FMassRenderPrimitiveFragment>();
	Composition.Add<FMassRenderISMFragment>();
	ISMHolderArchetype = EntityManager.CreateArchetype(Composition, FMassArchetypeCreationParams{TEXT("ArcIWMassISMHolder")});

	FMassElementBitSet CompositionWithMats = Composition;
	CompositionWithMats.Add<FMassOverrideMaterialsFragment>();
	ISMHolderArchetypeWithMats = EntityManager.CreateArchetype(CompositionWithMats, FMassArchetypeCreationParams{TEXT("ArcIWMassISMHolderMats")});
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
	ISMHolderEntities.Empty();
	ISMHolderArchetype = FMassArchetypeHandle();
	ISMHolderArchetypeWithMats = FMassArchetypeHandle();
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

	// Count total entities
	int32 TotalEntities = 0;
	for (const FArcIWActorClassData& ClassData : ActorClassEntries)
	{
		TotalEntities += ClassData.InstanceTransforms.Num();
	}

	SpawnedEntities.Reserve(TotalEntities);

	// Per-class MeshSlotBase offsets.
	TArray<int32> ClassMeshSlotBases;
	ClassMeshSlotBases.SetNum(ActorClassEntries.Num());

	int32 SlotIdx = 0;
	for (int32 ClassIndex = 0; ClassIndex < ActorClassEntries.Num(); ++ClassIndex)
	{
		ClassMeshSlotBases[ClassIndex] = SlotIdx;
		SlotIdx += ActorClassEntries[ClassIndex].MeshDescriptors.Num();
	}

	// Batch-create entities per class
	for (int32 ClassIndex = 0; ClassIndex < ActorClassEntries.Num(); ++ClassIndex)
	{
		const FArcIWActorClassData& ClassData = ActorClassEntries[ClassIndex];
		const int32 InstanceCount = ClassData.InstanceTransforms.Num();
		if (InstanceCount == 0)
		{
			continue;
		}

		// Build entity template
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
		TemplateData.AddFragment<FArcMassPhysicsBodyFragment>();

		FArcIWVisConfigFragment ConfigFragment;
		ConfigFragment.ActorClass = ClassData.ActorClass;
		ConfigFragment.MeshDescriptors = ClassData.MeshDescriptors;

		FConstSharedStruct SharedConfigFragment = FConstSharedStruct::Make(ConfigFragment);
		TemplateData.AddConstSharedFragment(SharedConfigFragment);

		FGuid ClassGuid = FGuid::NewDeterministicGuid(ClassData.ActorClass->GetPathName());
		FMassEntityTemplateID TemplateID = FMassEntityTemplateIDFactory::Make(ClassGuid);

		UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
		check(SpawnerSubsystem);
		FMassEntityTemplateRegistry& TemplateRegistry = SpawnerSubsystem->GetMutableTemplateRegistryInstance();
		const TSharedRef<FMassEntityTemplate>& FinalTemplate = TemplateRegistry.FindOrAddTemplate(TemplateID, MoveTemp(TemplateData));

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
				BodyFrag->TerminateBodies();
			}
			ValidEntities.Add(EntityHandle);
		}
	}
	EntityManager.BatchDestroyEntities(MakeArrayView(ValidEntities));
	SpawnedEntities.Reset();
}

// ---------------------------------------------------------------------------
// ActivateMesh / DeactivateMesh / ActivatePhysics / DeactivatePhysics
// ---------------------------------------------------------------------------

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

	for (int32 MeshIdx = 0; MeshIdx < MeshCount; ++MeshIdx)
	{
		Instance.ISMInstanceIds[MeshIdx] = INDEX_NONE;
		const FArcIWMeshEntry& MeshEntry = Config.MeshDescriptors[MeshIdx];
		if (!MeshEntry.Mesh)
		{
			continue;
		}

		const int32 HolderSlot = Instance.MeshSlotBase + MeshIdx;
		if (!ISMHolderEntities.IsValidIndex(HolderSlot))
		{
			continue;
		}

		FMassEntityHandle HolderEntity = ISMHolderEntities[HolderSlot];

		if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
		{
			// Holder exists — add instance directly
			FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(HolderEntity);
			FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);
			FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

			if (ISMFrag && PrimFrag && RenderState)
			{
				FTransform FinalTransform = MeshEntry.RelativeTransform * WorldTransform;
				FInstancedStaticMeshInstanceData InstanceData;
				InstanceData.Transform = FinalTransform.ToMatrixWithScale();
				int32 SparseIdx = ISMFrag->PerInstanceSMData.Add(InstanceData);
				Instance.ISMInstanceIds[MeshIdx] = SparseIdx;

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
		}
		else
		{
			// Holder doesn't exist — queue deferred creation
			FTransform FinalTransform = MeshEntry.RelativeTransform * WorldTransform;
			FArcIWMeshEntry MeshEntryCopy = MeshEntry;
			FMassEntityHandle SourceEntity = Entity;
			int32 CapturedMeshIdx = MeshIdx;
			int32 CapturedSlot = HolderSlot;
			AArcIWMassISMPartitionActor* Self = this;

			bool bHasOverrideMats = false;
			for (const TObjectPtr<UMaterialInterface>& Mat : MeshEntryCopy.Materials)
			{
				if (Mat)
				{
					bHasOverrideMats = true;
					break;
				}
			}
			FMassArchetypeHandle CachedArchetype = bHasOverrideMats ? Self->ISMHolderArchetypeWithMats : Self->ISMHolderArchetype;

			Context.Defer().PushCommand<FMassDeferredAddCommand>(
				[Self, CapturedSlot, CapturedMeshIdx, SourceEntity, MeshEntryCopy, FinalTransform, bHasOverrideMats, CachedArchetype]
				(FMassEntityManager& DeferredEM)
			{
				if (!DeferredEM.IsEntityValid(SourceEntity))
				{
					return;
				}

				FMassEntityHandle ExistingHolder = Self->ISMHolderEntities[CapturedSlot];
				if (ExistingHolder.IsValid() && DeferredEM.IsEntityValid(ExistingHolder))
				{
					// Another deferred command already created this holder
					FMassRenderISMFragment* ISMFrag = DeferredEM.GetFragmentDataPtr<FMassRenderISMFragment>(ExistingHolder);
					FMassRenderPrimitiveFragment* PrimFrag = DeferredEM.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(ExistingHolder);
					FMassRenderStateFragment* RenderState = DeferredEM.GetFragmentDataPtr<FMassRenderStateFragment>(ExistingHolder);

					if (ISMFrag && PrimFrag && RenderState)
					{
						FInstancedStaticMeshInstanceData InstanceData;
						InstanceData.Transform = FinalTransform.ToMatrixWithScale();
						int32 SparseIdx = ISMFrag->PerInstanceSMData.Add(InstanceData);

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

						FArcIWInstanceFragment* SourceInstance = DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(SourceEntity);
						if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(CapturedMeshIdx))
						{
							SourceInstance->ISMInstanceIds[CapturedMeshIdx] = SparseIdx;
						}
					}
					return;
				}

				// Build shared fragments
				FMassStaticMeshFragment StaticMeshFrag(MeshEntryCopy.Mesh);
				FMassVisualizationMeshFragment VisMeshFrag;
				VisMeshFrag.CastShadow = MeshEntryCopy.bCastShadows;

				FMassArchetypeSharedFragmentValues SharedValues;
				SharedValues.Add(FConstSharedStruct::Make(StaticMeshFrag));
				SharedValues.Add(FConstSharedStruct::Make(VisMeshFrag));

				FMassOverrideMaterialsFragment OverrideMats;
				if (bHasOverrideMats)
				{
					OverrideMats.OverrideMaterials = MeshEntryCopy.Materials;
					SharedValues.Add(FConstSharedStruct::Make(OverrideMats));
				}

				SharedValues.Sort();

				TArray<FMassEntityHandle> CreatedEntities;
				TSharedRef<FMassObserverManager::FCreationContext> CreationContext = DeferredEM.BatchCreateEntities(
					CachedArchetype, SharedValues, 1, CreatedEntities);

				FMassEntityHandle HolderEntity = CreatedEntities[0];
				FMassEntityView EntityView(DeferredEM, HolderEntity);

				// Init transform
				FTransformFragment& TransformFrag = EntityView.GetFragmentData<FTransformFragment>();
				TransformFrag.SetTransform(FTransform::Identity);

				// Init ISM fragment + proxy desc
				FMassRenderISMFragment& ISMFrag = EntityView.GetFragmentData<FMassRenderISMFragment>();
				FMassRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FMassRenderPrimitiveFragment>();
				PrimFrag.bIsVisible = true;

				const FMassOverrideMaterialsFragment* OverrideMatsPtr = bHasOverrideMats ? &OverrideMats : nullptr;

				checkf(ISMFrag.InstancedStaticMeshSceneProxyDesc, TEXT("Default ctor should create desc"));
				UE::MassEngine::Mesh::InitializeInstanceStaticMeshSceneProxyDescFromFragment(
					StaticMeshFrag, VisMeshFrag, TransformFrag, *ISMFrag.InstancedStaticMeshSceneProxyDesc);

				// Add first instance
				FInstancedStaticMeshInstanceData InstanceData;
				InstanceData.Transform = FinalTransform.ToMatrixWithScale();
				int32 SparseIdx = ISMFrag.PerInstanceSMData.Add(InstanceData);

				// Calculate bounds so the primitive isn't frustum-culled as a zero-size point
				PrimFrag.LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
					TransformFrag, ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
				PrimFrag.WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
					TransformFrag, ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);

				// Create render state helper (PerInstanceSMData has 1 entry — safe)
				FMassRenderStateFragment& RenderState = EntityView.GetFragmentData<FMassRenderStateFragment>();
				RenderState.CreateRenderStateHelper<FMassISMRenderStateHelper>(
					HolderEntity, &DeferredEM, PrimFrag, OverrideMatsPtr, ISMFrag);

				// Store holder handle
				Self->ISMHolderEntities[CapturedSlot] = HolderEntity;

				// Write back sparse index to source entity
				FArcIWInstanceFragment* SourceInstance = DeferredEM.GetFragmentDataPtr<FArcIWInstanceFragment>(SourceEntity);
				if (SourceInstance && SourceInstance->ISMInstanceIds.IsValidIndex(CapturedMeshIdx))
				{
					SourceInstance->ISMInstanceIds[CapturedMeshIdx] = SparseIdx;
				}
			});
		}
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

void AArcIWMassISMPartitionActor::ActivatePhysics(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWVisConfigFragment& Config,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FPhysScene* PhysScene = World->GetPhysicsScene();
	if (!PhysScene)
	{
		return;
	}

	FArcMassPhysicsBodyFragment* BodyFrag = EntityManager.GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(Entity);
	if (!BodyFrag)
	{
		return;
	}

	const int32 MeshCount = Config.MeshDescriptors.Num();
	BodyFrag->Bodies.SetNum(MeshCount);

	for (int32 MeshIdx = 0; MeshIdx < MeshCount; ++MeshIdx)
	{
		const FArcIWMeshEntry& MeshEntry = Config.MeshDescriptors[MeshIdx];
		if (!MeshEntry.Mesh)
		{
			BodyFrag->Bodies[MeshIdx] = nullptr;
			continue;
		}

		UBodySetup* BodySetup = MeshEntry.Mesh->GetBodySetup();
		if (BodySetup)
		{
			FTransform BodyTransform = MeshEntry.RelativeTransform * WorldTransform;
			FBodyInstance* Body = new FBodyInstance();
			FInitBodySpawnParams SpawnParams(/*bStaticPhysics=*/true, /*bPhysicsTypeDeterminesSimulation=*/false);
			Body->InitBody(BodySetup, BodyTransform, nullptr, PhysScene, SpawnParams);
			BodyFrag->Bodies[MeshIdx] = Body;
			// NOTE: ArcMassPhysicsEntityLink::Attach() is NOT called here.
			// Physics attachment is deferred to UArcIWPhysicsAttachProcessor.
		}
		else
		{
			BodyFrag->Bodies[MeshIdx] = nullptr;
		}
	}
}

void AArcIWMassISMPartitionActor::DeactivatePhysics(
	FMassEntityHandle Entity,
	FMassEntityManager& EntityManager)
{
	FArcMassPhysicsBodyFragment* BodyFrag = EntityManager.GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(Entity);
	if (BodyFrag)
	{
		BodyFrag->TerminateBodies();
	}
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
	ActivatePhysics(Entity, Instance, Config, WorldTransform, EntityManager);
}

void AArcIWMassISMPartitionActor::DeactivateEntity(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWVisConfigFragment& Config,
	FMassEntityManager& EntityManager)
{
	DeactivateMesh(Entity, Instance, Config, EntityManager);
	DeactivatePhysics(Entity, EntityManager);
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

	// Remove ISM instances and terminate physics bodies independently.
	// The hydrated actor will manage its own physics from this point.
	DeactivateMesh(EntityHandle, *Instance, *Config, EntityManager);
	DeactivatePhysics(EntityHandle, EntityManager);

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

void AArcIWMassISMPartitionActor::RebuildEditorPreviewISMCs()
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

void AArcIWMassISMPartitionActor::DestroyEditorPreviewISMCs()
{
	TArray<UInstancedStaticMeshComponent*> ISMComponenets;
	GetComponents(ISMComponenets);
	for (UInstancedStaticMeshComponent* ISMComponent : ISMComponenets)
	{
		ISMComponent->DestroyComponent();
	}
	
	EditorPreviewISMCs.Empty();
}
#endif

// ---------------------------------------------------------------------------
// UE::ArcIW Utilities
// ---------------------------------------------------------------------------

void UE::ArcIW::DetachPhysicsLinkEntries(FArcMassPhysicsLinkFragment& LinkFragment)
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
