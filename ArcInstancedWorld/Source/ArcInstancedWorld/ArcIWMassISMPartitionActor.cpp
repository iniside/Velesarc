// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWMassISMPartitionActor.h"

#include "ArcIWActorPoolSubsystem.h"
#include "ArcIWPartitionActor.h"
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

// Physics entity link, Chaos user data, and ArcMass physics body fragment
#include "MassEntityView.h"
#include "ArcMass/ArcMassPhysicsEntityLink.h"
#include "ArcMass/ArcMassPhysicsBody.h"
#include "Chaos/ChaosUserEntity.h"
#include "Engine/World.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AArcIWMassISMPartitionActor::AArcIWMassISMPartitionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

uint32 AArcIWMassISMPartitionActor::GetGridCellSize(UWorld* InWorld, FName InGridName)
{
	return AArcIWPartitionActor::GetGridCellSize(InWorld, InGridName);
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

	CreateISMHolderEntities();
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

void AArcIWMassISMPartitionActor::CreateISMHolderEntities()
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

	// Count total mesh slots
	int32 TotalMeshSlots = 0;
	for (const FArcIWActorClassData& ClassData : ActorClassEntries)
	{
		TotalMeshSlots += ClassData.MeshDescriptors.Num();
	}

	ISMHolderEntities.SetNum(TotalMeshSlots);

	// Base element composition for ISM holder entities
	FMassElementBitSet BaseComposition;
	BaseComposition.Add<FTransformFragment>();
	BaseComposition.Add<FMassRenderStateFragment>();
	BaseComposition.Add<FMassRenderPrimitiveFragment>();
	BaseComposition.Add<FMassRenderISMFragment>();

	int32 SlotIdx = 0;
	for (const FArcIWActorClassData& ClassData : ActorClassEntries)
	{
		for (const FArcIWMeshEntry& MeshEntry : ClassData.MeshDescriptors)
		{
			if (!MeshEntry.Mesh)
			{
				ISMHolderEntities[SlotIdx] = FMassEntityHandle();
				++SlotIdx;
				continue;
			}

			// Build shared fragment values for this mesh
			FMassStaticMeshFragment StaticMeshFrag(MeshEntry.Mesh);
			FMassVisualizationMeshFragment VisMeshFrag;
			VisMeshFrag.CastShadow = MeshEntry.bCastShadows;

			FMassArchetypeSharedFragmentValues SharedValues;
			SharedValues.Add(FConstSharedStruct::Make(StaticMeshFrag));
			SharedValues.Add(FConstSharedStruct::Make(VisMeshFrag));

			// Add material overrides if present
			const FMassOverrideMaterialsFragment* OverrideMatsPtr = nullptr;
			FMassOverrideMaterialsFragment OverrideMats;
			if (MeshEntry.Materials.Num() > 0)
			{
				bool bHasMaterial = false;
				for (const TObjectPtr<UMaterialInterface>& Mat : MeshEntry.Materials)
				{
					if (Mat)
					{
						bHasMaterial = true;
						break;
					}
				}
				if (bHasMaterial)
				{
					OverrideMats.OverrideMaterials.Reserve(MeshEntry.Materials.Num());
					for (const TObjectPtr<UMaterialInterface>& Mat : MeshEntry.Materials)
					{
						OverrideMats.OverrideMaterials.Add(Mat);
					}
					SharedValues.Add(FConstSharedStruct::Make(OverrideMats));
					OverrideMatsPtr = &OverrideMats;
				}
			}

			// Build archetype -- add material override type if needed
			FMassElementBitSet Composition = BaseComposition;
			if (OverrideMatsPtr)
			{
				Composition.Add<FMassOverrideMaterialsFragment>();
			}

			FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(Composition, FMassArchetypeCreationParams{TEXT("ArcIWMassISMHolder")});

			SharedValues.Sort();

			TArray<FMassEntityHandle> CreatedEntities;
			TSharedRef<FMassObserverManager::FCreationContext> CreationContext = EntityManager.BatchCreateEntities(
				Archetype, SharedValues, 1, CreatedEntities);

			FMassEntityHandle HolderEntity = CreatedEntities[0];
			FMassEntityView EntityView(EntityManager, HolderEntity);

			// Set transform to identity (instances are in world space)
			FTransformFragment& TransformFrag = EntityView.GetFragmentData<FTransformFragment>();
			TransformFrag.SetTransform(FTransform::Identity);

			// ISM holders are created empty — instances are added per-entity by ActivateEntity().
			FMassRenderISMFragment& ISMFrag = EntityView.GetFragmentData<FMassRenderISMFragment>();

			// Initialize the scene proxy desc from fragments
			FMassRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FMassRenderPrimitiveFragment>();
			PrimFrag.bIsVisible = true;

			checkf(ISMFrag.InstancedStaticMeshSceneProxyDesc, TEXT("Default FMassRenderISMFragment ctor should create the desc"));
			UE::MassEngine::Mesh::InitializeInstanceStaticMeshSceneProxyDescFromFragment(
				StaticMeshFrag, VisMeshFrag, TransformFrag, *ISMFrag.InstancedStaticMeshSceneProxyDesc);

			// Calculate bounds
			PrimFrag.LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
				TransformFrag, ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
			PrimFrag.WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
				TransformFrag, ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);

			// Create the render state helper -- the existing UMassCreateRenderStateProcessor
			// will pick it up and call CreateRenderState() to create the scene proxy.
			FMassRenderStateFragment& RenderState = EntityView.GetFragmentData<FMassRenderStateFragment>();
			RenderState.CreateRenderStateHelper<FMassISMRenderStateHelper>(
				HolderEntity, &EntityManager, PrimFrag, OverrideMatsPtr, ISMFrag);

			ISMHolderEntities[SlotIdx] = HolderEntity;
			++SlotIdx;
		}
	}
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

	// Count totals
	int32 TotalMeshSlots = 0;
	int32 TotalEntities = 0;
	for (const FArcIWActorClassData& ClassData : ActorClassEntries)
	{
		TotalMeshSlots += ClassData.MeshDescriptors.Num();
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

	// Terminate physics bodies before destroying entities
	for (FMassEntityHandle EntityHandle : SpawnedEntities)
	{
		if (EntityManager.IsEntityValid(EntityHandle))
		{
			FArcMassPhysicsBodyFragment* BodyFrag = EntityManager.GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(EntityHandle);
			if (BodyFrag)
			{
				BodyFrag->TerminateBodies();
			}
		}
	}

	TArray<FMassEntityHandle> ValidEntities;
	ValidEntities.Reserve(SpawnedEntities.Num());
	for (FMassEntityHandle EntityHandle : SpawnedEntities)
	{
		if (EntityManager.IsEntityValid(EntityHandle))
		{
			ValidEntities.Add(EntityHandle);
		}
	}

	EntityManager.BatchDestroyEntities(MakeArrayView(ValidEntities));
	SpawnedEntities.Reset();
}

// ---------------------------------------------------------------------------
// ActivateEntity / DeactivateEntity
// ---------------------------------------------------------------------------

void AArcIWMassISMPartitionActor::ActivateEntity(
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

	const int32 MeshCount = Config.MeshDescriptors.Num();

	FArcMassPhysicsBodyFragment* BodyFrag = EntityManager.GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(Entity);
	if (BodyFrag)
	{
		BodyFrag->Bodies.SetNum(MeshCount);
	}

	Instance.ISMInstanceIds.SetNum(MeshCount);

	for (int32 MeshIdx = 0; MeshIdx < MeshCount; ++MeshIdx)
	{
		Instance.ISMInstanceIds[MeshIdx] = INDEX_NONE;

		const FArcIWMeshEntry& MeshEntry = Config.MeshDescriptors[MeshIdx];
		if (!MeshEntry.Mesh)
		{
			if (BodyFrag)
			{
				BodyFrag->Bodies[MeshIdx] = nullptr;
			}
			continue;
		}

		// --- ISM instance ---
		const int32 HolderSlot = Instance.MeshSlotBase + MeshIdx;
		if (ISMHolderEntities.IsValidIndex(HolderSlot))
		{
			FMassEntityHandle HolderEntity = ISMHolderEntities[HolderSlot];
			if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
			{
				FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(HolderEntity);
				FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

				if (ISMFrag && RenderState)
				{
					FTransform FinalTransform = MeshEntry.RelativeTransform * WorldTransform;
					FInstancedStaticMeshInstanceData InstanceData;
					InstanceData.Transform = FinalTransform.ToMatrixWithScale();
					const int32 SparseIdx = ISMFrag->PerInstanceSMData.Add(InstanceData);
					Instance.ISMInstanceIds[MeshIdx] = SparseIdx;
					RenderState->GetRenderStateHelper().MarkRenderStateDirty();
				}
			}
		}

		// --- Physics body ---
		if (BodyFrag && PhysScene)
		{
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
}

void AArcIWMassISMPartitionActor::DeactivateEntity(
	FMassEntityHandle Entity,
	FArcIWInstanceFragment& Instance,
	const FArcIWVisConfigFragment& Config,
	FMassEntityManager& EntityManager)
{
	const int32 MeshCount = Config.MeshDescriptors.Num();

	for (int32 MeshIdx = 0; MeshIdx < MeshCount; ++MeshIdx)
	{
		// --- Remove ISM instance ---
		const int32 HolderSlot = Instance.MeshSlotBase + MeshIdx;
		if (ISMHolderEntities.IsValidIndex(HolderSlot))
		{
			FMassEntityHandle HolderEntity = ISMHolderEntities[HolderSlot];
			if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
			{
				FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(HolderEntity);
				FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

				if (ISMFrag && RenderState)
				{
					const int32 SparseIdx = Instance.ISMInstanceIds.IsValidIndex(MeshIdx) ? Instance.ISMInstanceIds[MeshIdx] : INDEX_NONE;
					if (SparseIdx != INDEX_NONE && ISMFrag->PerInstanceSMData.IsValidIndex(SparseIdx))
					{
						ISMFrag->PerInstanceSMData.RemoveAt(SparseIdx);
						RenderState->GetRenderStateHelper().MarkRenderStateDirty();
					}
				}
			}
		}

		if (Instance.ISMInstanceIds.IsValidIndex(MeshIdx))
		{
			Instance.ISMInstanceIds[MeshIdx] = INDEX_NONE;
		}
	}

	// --- Destroy physics bodies ---
	FArcMassPhysicsBodyFragment* BodyFrag = EntityManager.GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(Entity);
	if (BodyFrag)
	{
		BodyFrag->TerminateBodies();
	}
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

	// Deactivate — remove ISM instances and terminate physics bodies.
	// The hydrated actor will manage its own physics from this point.
	DeactivateEntity(EntityHandle, *Instance, *Config, EntityManager);

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
