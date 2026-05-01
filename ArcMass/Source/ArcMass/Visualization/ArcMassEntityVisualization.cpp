// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityVisualization.h"

#include "ArcVisSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassSpawnerSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommands.h"
#include "MassExecutionContext.h"
#include "MassRenderStateHelper.h"
#include "MassISMRenderStateHelper.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "Mesh/MassEngineMeshUtils.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassInstancedSkinnedMeshRenderStateHelper.h"
#include "InstancedSkinnedMeshSceneProxyDesc.h"

TAutoConsoleVariable<bool> CVarArcDebugDrawVisualizationGrid(
	TEXT("arc.mass.DebugDrawVisualizationGrid"),
	false,
	TEXT("Toggles debug drawing for entity visualization grid active cells (0 = off, 1 = on)"));

// ---------------------------------------------------------------------------
// UArcEntityVisualizationSubsystem
// ---------------------------------------------------------------------------

bool UArcEntityVisualizationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldTyp) const
{
	return WorldTyp == EWorldType::PIE || WorldTyp == EWorldType::Game;
}

void UArcEntityVisualizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UArcVisSettings* Settings = GetDefault<UArcVisSettings>();

	// Mesh grid
	MeshGrid.CellSize = Settings->MeshCellSize;
	MeshActivationRadius = Settings->MeshActivationRadius;
	MeshDeactivationRadius = Settings->MeshDeactivationRadius;
	RecomputeMeshRadiusCells();
	LastMeshPlayerCell = FIntVector(TNumericLimits<int32>::Max());

	// Physics grid
	PhysicsGrid.CellSize = Settings->PhysicsCellSize;
	PhysicsActivationRadius = Settings->PhysicsActivationRadius;
	PhysicsDeactivationRadius = Settings->PhysicsDeactivationRadius;
	RecomputePhysicsRadiusCells();
	LastPhysicsPlayerCell = FIntVector(TNumericLimits<int32>::Max());
}

void UArcEntityVisualizationSubsystem::PreDeinitialize()
{
	// FMassEntityManager::Deinitialize() bulk-destroys entities without firing observers,
	// so UMassDestroyRenderStateProcessor never runs for ISM holder entities.
	// PreDeinitialize runs on ALL subsystems before any Deinitialize, so the entity manager
	// is guaranteed to still be alive here.
	if (UWorld* World = GetWorld())
	{
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
		for (TPair<FIntVector, TMap<FArcVisISMHolderKey, FMassEntityHandle>>& CellPair : CellISMHolders)
		{
			for (TPair<FArcVisISMHolderKey, FMassEntityHandle>& HolderPair : CellPair.Value)
			{
				FMassEntityHandle HolderEntity = HolderPair.Value;
				if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
				{
					FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);
					if (RenderState)
					{
						RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);
					}
				}
			}
		}

		for (TPair<FIntVector, TMap<FArcVisSkinnedISMHolderKey, FMassEntityHandle>>& CellPair : CellSkinnedISMHolders)
		{
			for (TPair<FArcVisSkinnedISMHolderKey, FMassEntityHandle>& HolderPair : CellPair.Value)
			{
				FMassEntityHandle HolderEntity = HolderPair.Value;
				if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
				{
					FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);
					if (RenderState)
					{
						RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);
						FArcMassInstancedSkinnedMeshRenderStateHelper* SkinnedHelper =
							static_cast<FArcMassInstancedSkinnedMeshRenderStateHelper*>(&RenderState->GetRenderStateHelper());
						SkinnedHelper->DestroyMeshObject();
					}
				}
			}
		}
	}

	Super::PreDeinitialize();
}

void UArcEntityVisualizationSubsystem::Deinitialize()
{
	CellISMHolders.Empty();
	CellSkinnedISMHolders.Empty();
	MeshGrid.Clear();
	PhysicsGrid.Clear();
	Super::Deinitialize();
}

// --- Mesh Grid ---

void UArcEntityVisualizationSubsystem::RecomputeMeshRadiusCells()
{
	MeshActivationRadiusCells = FMath::CeilToInt(MeshActivationRadius / MeshGrid.CellSize);
	MeshDeactivationRadiusCells = FMath::CeilToInt(MeshDeactivationRadius / MeshGrid.CellSize);
}

void UArcEntityVisualizationSubsystem::RegisterMeshEntity(FMassEntityHandle Entity, const FVector& Position)
{
	MeshGrid.AddEntity(Entity, Position);
}

void UArcEntityVisualizationSubsystem::UnregisterMeshEntity(FMassEntityHandle Entity, const FIntVector& Cell)
{
	MeshGrid.RemoveEntity(Entity, Cell);
}

void UArcEntityVisualizationSubsystem::UpdateMeshPlayerCell(const FIntVector& NewCell)
{
	LastMeshPlayerCell = NewCell;
}

bool UArcEntityVisualizationSubsystem::IsMeshActiveCellCoord(const FIntVector& Cell) const
{
	const int32 DX = FMath::Abs(Cell.X - LastMeshPlayerCell.X);
	const int32 DY = FMath::Abs(Cell.Y - LastMeshPlayerCell.Y);
	const int32 DZ = FMath::Abs(Cell.Z - LastMeshPlayerCell.Z);
	return DX <= MeshActivationRadiusCells && DY <= MeshActivationRadiusCells && DZ <= MeshActivationRadiusCells;
}

// --- Physics Grid ---

void UArcEntityVisualizationSubsystem::RecomputePhysicsRadiusCells()
{
	PhysicsActivationRadiusCells = FMath::CeilToInt(PhysicsActivationRadius / PhysicsGrid.CellSize);
	PhysicsDeactivationRadiusCells = FMath::CeilToInt(PhysicsDeactivationRadius / PhysicsGrid.CellSize);
}

void UArcEntityVisualizationSubsystem::RegisterPhysicsEntity(FMassEntityHandle Entity, const FVector& Position)
{
	PhysicsGrid.AddEntity(Entity, Position);
}

void UArcEntityVisualizationSubsystem::UnregisterPhysicsEntity(FMassEntityHandle Entity, const FIntVector& Cell)
{
	PhysicsGrid.RemoveEntity(Entity, Cell);
}

void UArcEntityVisualizationSubsystem::UpdatePhysicsPlayerCell(const FIntVector& NewCell)
{
	LastPhysicsPlayerCell = NewCell;
}

bool UArcEntityVisualizationSubsystem::IsPhysicsActiveCellCoord(const FIntVector& Cell) const
{
	const int32 DX = FMath::Abs(Cell.X - LastPhysicsPlayerCell.X);
	const int32 DY = FMath::Abs(Cell.Y - LastPhysicsPlayerCell.Y);
	const int32 DZ = FMath::Abs(Cell.Z - LastPhysicsPlayerCell.Z);
	return DX <= PhysicsActivationRadiusCells && DY <= PhysicsActivationRadiusCells && DZ <= PhysicsActivationRadiusCells;
}

// --- ISM Holders ---

void UArcEntityVisualizationSubsystem::InitializeISMHolderArchetypes(FMassEntityManager& EntityManager)
{
	if (bISMHolderArchetypesInitialized)
	{
		return;
	}

	FMassElementBitSet Composition;
	Composition.Add<FTransformFragment>();
	Composition.Add<FMassRenderStateFragment>();
	Composition.Add<FMassRenderPrimitiveFragment>();
	Composition.Add<FMassRenderISMFragment>();
	ISMHolderArchetype = EntityManager.CreateArchetype(Composition, FMassArchetypeCreationParams{TEXT("ArcVisISMHolder")});

	FMassElementBitSet CompositionWithMats = Composition;
	CompositionWithMats.Add<FMassOverrideMaterialsFragment>();
	ISMHolderArchetypeWithMats = EntityManager.CreateArchetype(CompositionWithMats, FMassArchetypeCreationParams{TEXT("ArcVisISMHolderMats")});

	bISMHolderArchetypesInitialized = true;
}

FMassEntityHandle UArcEntityVisualizationSubsystem::FindOrCreateISMHolder(
	const FIntVector& Cell,
	const FArcMassStaticMeshConfigFragment& StaticMeshConfigFrag,
	const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag,
	const FMassOverrideMaterialsFragment* OverrideMats,
	FMassEntityManager& EntityManager)
{
	InitializeISMHolderArchetypes(EntityManager);

	FArcVisISMHolderKey Key;
	Key.Mesh = StaticMeshConfigFrag.Mesh;
	Key.bCastShadow = VisMeshConfigFrag.CastShadow;
	Key.bHasOverrideMaterials = OverrideMats != nullptr;

	TMap<FArcVisISMHolderKey, FMassEntityHandle>& CellHolders = CellISMHolders.FindOrAdd(Cell);

	FMassEntityHandle* ExistingHolder = CellHolders.Find(Key);
	if (ExistingHolder && ExistingHolder->IsValid() && EntityManager.IsEntityValid(*ExistingHolder))
	{
		return *ExistingHolder;
	}

	// Convert config fragments to engine fragments for holder entity
	FMassStaticMeshFragment StaticMeshFrag = ArcMass::Visualization::ToEngineFragment(StaticMeshConfigFrag);
	FMassVisualizationMeshFragment VisMeshFrag = ArcMass::Visualization::ToEngineFragment(VisMeshConfigFrag);

	// Create new holder entity
	FMassArchetypeHandle Archetype = OverrideMats ? ISMHolderArchetypeWithMats : ISMHolderArchetype;

	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(FConstSharedStruct::Make(StaticMeshFrag));
	SharedValues.Add(FConstSharedStruct::Make(VisMeshFrag));
	if (OverrideMats)
	{
		SharedValues.Add(FConstSharedStruct::Make(*OverrideMats));
	}
	SharedValues.Sort();

	TArray<FMassEntityHandle> CreatedEntities;
	TSharedRef<FMassObserverManager::FCreationContext> CreationContext =
		EntityManager.BatchCreateEntities(Archetype, SharedValues, 1, CreatedEntities);

	if (CreatedEntities.IsEmpty())
	{
		return FMassEntityHandle();
	}

	FMassEntityHandle HolderEntity = CreatedEntities[0];
	FMassEntityView EntityView(EntityManager, HolderEntity);

	// Initialize transform (identity — instances carry own transforms)
	FTransformFragment& TransformFrag = EntityView.GetFragmentData<FTransformFragment>();
	TransformFrag.SetTransform(FTransform::Identity);

	// Initialize ISM scene proxy desc
	FMassRenderISMFragment& ISMFrag = EntityView.GetFragmentData<FMassRenderISMFragment>();
	FMassRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FMassRenderPrimitiveFragment>();
	PrimFrag.bIsVisible = true;

	checkf(ISMFrag.InstancedStaticMeshSceneProxyDesc, TEXT("Default ctor should create desc"));
	UE::MassEngine::Mesh::InitializeInstanceStaticMeshSceneProxyDescFromFragment(
		StaticMeshFrag, VisMeshFrag, TransformFrag, *ISMFrag.InstancedStaticMeshSceneProxyDesc);

	// Create render state helper
	FMassRenderStateFragment& RenderState = EntityView.GetFragmentData<FMassRenderStateFragment>();
	RenderState.CreateRenderStateHelper<FMassISMRenderStateHelper>(
		HolderEntity, &EntityManager, PrimFrag, OverrideMats, ISMFrag);

	CellHolders.Add(Key, HolderEntity);
	return HolderEntity;
}

int32 UArcEntityVisualizationSubsystem::AddISMInstance(
	FMassEntityHandle HolderEntity,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager)
{
	FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(HolderEntity);
	FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);
	FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

	if (!ISMFrag || !PrimFrag || !RenderState)
	{
		return INDEX_NONE;
	}

	FInstancedStaticMeshInstanceData InstanceData;
	InstanceData.Transform = WorldTransform.ToMatrixWithScale();
	int32 SparseIdx = ISMFrag->PerInstanceSMData.Add(InstanceData);

	// Recalculate bounds
	FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(HolderEntity);
	if (TransformFrag)
	{
		PrimFrag->LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
			*TransformFrag, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
		PrimFrag->WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
			*TransformFrag, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);
	}

	PrimFrag->bIsVisible = true;
	RenderState->GetRenderStateHelper().MarkRenderStateDirty();

	return SparseIdx;
}

void UArcEntityVisualizationSubsystem::RemoveISMInstance(
	FMassEntityHandle HolderEntity,
	int32 InstanceIndex,
	const FIntVector& Cell,
	FMassEntityManager& EntityManager)
{
	if (!HolderEntity.IsValid() || !EntityManager.IsEntityValid(HolderEntity))
	{
		return;
	}

	FMassRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FMassRenderISMFragment>(HolderEntity);
	FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);
	FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

	if (!ISMFrag || !PrimFrag || !RenderState)
	{
		return;
	}

	// Holder already emptied and queued for deferred destruction by a prior call in the same batch
	if (ISMFrag->PerInstanceSMData.Num() == 0)
	{
		return;
	}

	if (ISMFrag->PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		ISMFrag->PerInstanceSMData.RemoveAt(InstanceIndex);
	}

	if (ISMFrag->PerInstanceSMData.Num() == 0)
	{
		// Holder is empty — remove primitive from scene now (must happen before scene teardown),
		// but keep RenderStateHelper alive so UMassDestroyRenderStateProcessor can still run
		// when the deferred destroy fires (DestroyRenderState is a no-op with null proxy).
		RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);
		EntityManager.Defer().DestroyEntity(HolderEntity);

		// Remove from registry
		TMap<FArcVisISMHolderKey, FMassEntityHandle>* CellHolders = CellISMHolders.Find(Cell);
		if (CellHolders)
		{
			for (auto It = CellHolders->CreateIterator(); It; ++It)
			{
				if (It->Value == HolderEntity)
				{
					It.RemoveCurrent();
					break;
				}
			}
			if (CellHolders->IsEmpty())
			{
				CellISMHolders.Remove(Cell);
			}
		}
	}
	else
	{
		// Recalculate bounds
		FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(HolderEntity);
		if (TransformFrag)
		{
			PrimFrag->LocalBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
				*TransformFrag, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::LocalBounds);
			PrimFrag->WorldBounds = UE::MassEngine::Mesh::CalculateInstancedStaticMeshBounds(
				*TransformFrag, *ISMFrag, UE::MassEngine::Mesh::EBoundsType::WorldBounds);
		}

		RenderState->GetRenderStateHelper().MarkRenderStateDirty();
	}
}

void UArcEntityVisualizationSubsystem::DestroyCellHolders(const FIntVector& Cell, FMassEntityManager& EntityManager)
{
	TMap<FArcVisISMHolderKey, FMassEntityHandle>* CellHolders = CellISMHolders.Find(Cell);
	if (!CellHolders)
	{
		return;
	}

	for (auto& [Key, HolderEntity] : *CellHolders)
	{
		if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
		{
			FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);
			if (RenderState)
			{
				RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);
			}
			EntityManager.Defer().DestroyEntity(HolderEntity);
		}
	}

	CellISMHolders.Remove(Cell);
}

void UArcEntityVisualizationSubsystem::BatchActivateISMEntities(
	TArray<FArcPendingISMActivation>&& PendingActivations,
	FMassExecutionContext& Context)
{
	if (PendingActivations.IsEmpty())
	{
		return;
	}

	TWeakObjectPtr<UArcEntityVisualizationSubsystem> WeakThis(this);
	Context.Defer().PushCommand<FMassDeferredCreateCommand>(
		[WeakThis, Pending = MoveTemp(PendingActivations)]
		(FMassEntityManager& EntityManager) mutable
	{
		UArcEntityVisualizationSubsystem* Subsystem = WeakThis.Get();
		if (!Subsystem)
		{
			return;
		}

		for (FArcPendingISMActivation& Activation : Pending)
		{
			if (!EntityManager.IsEntityValid(Activation.SourceEntity))
			{
				continue;
			}

			const FMassOverrideMaterialsFragment* OverrideMatsPtr =
				Activation.bHasOverrideMats ? &Activation.OverrideMats : nullptr;

			FMassEntityHandle HolderEntity = Subsystem->FindOrCreateISMHolder(
				Activation.Cell,
				Activation.StaticMeshConfigFrag,
				Activation.VisMeshConfigFrag,
				OverrideMatsPtr,
				EntityManager);

			if (!HolderEntity.IsValid())
			{
				continue;
			}

			int32 InstanceIndex = Subsystem->AddISMInstance(
				HolderEntity, Activation.WorldTransform, EntityManager);

			EntityManager.AddFragmentToEntity(
				Activation.SourceEntity,
				FArcVisISMInstanceFragment::StaticStruct(),
				[HolderEntity, InstanceIndex](void* Fragment, const UScriptStruct&)
			{
				FArcVisISMInstanceFragment* ISMFrag = static_cast<FArcVisISMInstanceFragment*>(Fragment);
				ISMFrag->HolderEntity = HolderEntity;
				ISMFrag->InstanceIndex = InstanceIndex;
			});
		}
	});
}

// --- Skinned ISM Holders ---

void UArcEntityVisualizationSubsystem::InitializeSkinnedISMHolderArchetypes(FMassEntityManager& EntityManager)
{
	if (bSkinnedISMHolderArchetypesInitialized)
	{
		return;
	}

	FMassElementBitSet Composition;
	Composition.Add<FTransformFragment>();
	Composition.Add<FMassRenderStateFragment>();
	Composition.Add<FMassRenderPrimitiveFragment>();
	Composition.Add<FArcMassRenderISMSkinnedFragment>();
	SkinnedISMHolderArchetype = EntityManager.CreateArchetype(Composition, FMassArchetypeCreationParams{TEXT("ArcVisSkinnedISMHolder")});

	FMassElementBitSet CompositionWithMats = Composition;
	CompositionWithMats.Add<FMassOverrideMaterialsFragment>();
	SkinnedISMHolderArchetypeWithMats = EntityManager.CreateArchetype(CompositionWithMats, FMassArchetypeCreationParams{TEXT("ArcVisSkinnedISMHolderMats")});

	bSkinnedISMHolderArchetypesInitialized = true;
}

FMassEntityHandle UArcEntityVisualizationSubsystem::FindOrCreateSkinnedISMHolder(
	const FIntVector& Cell,
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag,
	const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag,
	const FMassOverrideMaterialsFragment* OverrideMats,
	FMassEntityManager& EntityManager)
{
	InitializeSkinnedISMHolderArchetypes(EntityManager);

	FArcVisSkinnedISMHolderKey Key;
	Key.SkinnedAsset = SkinnedMeshFrag.SkinnedAsset;
	Key.bCastShadow = VisMeshConfigFrag.CastShadow;
	Key.bHasOverrideMaterials = OverrideMats != nullptr;

	TMap<FArcVisSkinnedISMHolderKey, FMassEntityHandle>& CellHolders = CellSkinnedISMHolders.FindOrAdd(Cell);

	FMassEntityHandle* ExistingHolder = CellHolders.Find(Key);
	if (ExistingHolder && ExistingHolder->IsValid() && EntityManager.IsEntityValid(*ExistingHolder))
	{
		return *ExistingHolder;
	}

	FMassArchetypeHandle Archetype = OverrideMats ? SkinnedISMHolderArchetypeWithMats : SkinnedISMHolderArchetype;

	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(FConstSharedStruct::Make(SkinnedMeshFrag));
	FMassVisualizationMeshFragment VisMeshFrag = ArcMass::Visualization::ToEngineFragment(VisMeshConfigFrag);
	SharedValues.Add(FConstSharedStruct::Make(VisMeshFrag));
	if (OverrideMats)
	{
		SharedValues.Add(FConstSharedStruct::Make(*OverrideMats));
	}
	SharedValues.Sort();

	TArray<FMassEntityHandle> CreatedEntities;
	TSharedRef<FMassObserverManager::FCreationContext> CreationContext =
		EntityManager.BatchCreateEntities(Archetype, SharedValues, 1, CreatedEntities);

	if (CreatedEntities.IsEmpty())
	{
		return FMassEntityHandle();
	}

	FMassEntityHandle HolderEntity = CreatedEntities[0];
	FMassEntityView EntityView(EntityManager, HolderEntity);

	FTransformFragment& TransformFrag = EntityView.GetFragmentData<FTransformFragment>();
	TransformFrag.SetTransform(FTransform::Identity);

	FArcMassRenderISMSkinnedFragment& SkinnedISMFrag = EntityView.GetFragmentData<FArcMassRenderISMSkinnedFragment>();
	FMassRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FMassRenderPrimitiveFragment>();
	PrimFrag.bIsVisible = true;

	TSharedPtr<FInstancedSkinnedMeshSceneProxyDesc> SceneProxyDesc = MakeShared<FInstancedSkinnedMeshSceneProxyDesc>();
	SceneProxyDesc->SkinnedAsset = SkinnedMeshFrag.SkinnedAsset;
	SceneProxyDesc->TransformProvider = SkinnedMeshFrag.TransformProvider;
	SceneProxyDesc->CastShadow = VisMeshConfigFrag.CastShadow;
	SceneProxyDesc->bCastShadowAsTwoSided = VisMeshConfigFrag.CastShadowAsTwoSided;
	SceneProxyDesc->bCastHiddenShadow = VisMeshConfigFrag.CastHiddenShadow;
	SceneProxyDesc->bCastFarShadow = VisMeshConfigFrag.CastFarShadow;
	SceneProxyDesc->bCastInsetShadow = VisMeshConfigFrag.CastInsetShadow;
	SceneProxyDesc->bCastContactShadow = VisMeshConfigFrag.CastContactShadow;
	SceneProxyDesc->bReceivesDecals = VisMeshConfigFrag.ReceivesDecals;
	SceneProxyDesc->bRenderCustomDepth = VisMeshConfigFrag.RenderCustomDepth;
	SceneProxyDesc->CustomDepthStencilValue = static_cast<int32>(VisMeshConfigFrag.CustomDepthStencilValue);
	SceneProxyDesc->TranslucencySortPriority = VisMeshConfigFrag.TranslucencySortPriority;
	SceneProxyDesc->bAffectDynamicIndirectLighting = VisMeshConfigFrag.AffectDynamicIndirectLighting;
	SceneProxyDesc->bAffectIndirectLightingWhileHidden = VisMeshConfigFrag.AffectIndirectLightingWhileHidden;
	if (VisMeshConfigFrag.NeverDistanceCull)
	{
		SceneProxyDesc->CachedMaxDrawDistance = 0.0f;
	}
	SkinnedISMFrag.SceneProxyDesc = SceneProxyDesc;

	FMassRenderStateFragment& RenderState = EntityView.GetFragmentData<FMassRenderStateFragment>();
	RenderState.CreateRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>(
		HolderEntity, &EntityManager, PrimFrag, OverrideMats);

	CellHolders.Add(Key, HolderEntity);
	return HolderEntity;
}

int32 UArcEntityVisualizationSubsystem::AddSkinnedISMInstance(
	FMassEntityHandle HolderEntity,
	const FTransform& WorldTransform,
	uint32 AnimationIndex,
	FMassEntityManager& EntityManager)
{
	FArcMassRenderISMSkinnedFragment* SkinnedISMFrag = EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(HolderEntity);
	FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);
	FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

	if (!SkinnedISMFrag || !PrimFrag || !RenderState)
	{
		return INDEX_NONE;
	}

	FArcSkinnedMeshInstanceData InstanceData;
	InstanceData.Transform = FTransform3f(WorldTransform);
	InstanceData.AnimationIndex = AnimationIndex;
	int32 SparseIdx = SkinnedISMFrag->PerInstanceData.Add(InstanceData);

	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = EntityManager.GetConstSharedFragmentDataChecked<FArcMassSkinnedMeshFragment>(HolderEntity);
	USkinnedAsset* Asset = SkinnedMeshFrag.SkinnedAsset.Get();
	if (Asset)
	{
		FBoxSphereBounds AssetBounds = Asset->GetBounds();
		FBox AccumulatedBounds(ForceInit);
		for (const FArcSkinnedMeshInstanceData& Inst : SkinnedISMFrag->PerInstanceData)
		{
			FTransform InstTransform(Inst.Transform);
			FBox TransformedBox = AssetBounds.GetBox().TransformBy(InstTransform);
			AccumulatedBounds += TransformedBox;
		}
		PrimFrag->LocalBounds = FBoxSphereBounds(AccumulatedBounds);
		PrimFrag->WorldBounds = FBoxSphereBounds(AccumulatedBounds);
	}

	PrimFrag->bIsVisible = true;
	RenderState->GetRenderStateHelper().MarkRenderStateDirty();

	return SparseIdx;
}

void UArcEntityVisualizationSubsystem::RemoveSkinnedISMInstance(
	FMassEntityHandle HolderEntity,
	int32 InstanceIndex,
	const FIntVector& Cell,
	FMassEntityManager& EntityManager)
{
	if (!HolderEntity.IsValid() || !EntityManager.IsEntityValid(HolderEntity))
	{
		return;
	}

	FArcMassRenderISMSkinnedFragment* SkinnedISMFrag = EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(HolderEntity);
	FMassRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FMassRenderPrimitiveFragment>(HolderEntity);
	FMassRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FMassRenderStateFragment>(HolderEntity);

	if (!SkinnedISMFrag || !PrimFrag || !RenderState)
	{
		return;
	}

	if (SkinnedISMFrag->PerInstanceData.Num() == 0)
	{
		return;
	}

	if (SkinnedISMFrag->PerInstanceData.IsValidIndex(InstanceIndex))
	{
		SkinnedISMFrag->PerInstanceData.RemoveAt(InstanceIndex);
	}

	if (SkinnedISMFrag->PerInstanceData.Num() == 0)
	{
		RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);

		FArcMassInstancedSkinnedMeshRenderStateHelper* SkinnedHelper =
			static_cast<FArcMassInstancedSkinnedMeshRenderStateHelper*>(&RenderState->GetRenderStateHelper());
		SkinnedHelper->DestroyMeshObject();

		EntityManager.Defer().DestroyEntity(HolderEntity);

		TMap<FArcVisSkinnedISMHolderKey, FMassEntityHandle>* CellHolders = CellSkinnedISMHolders.Find(Cell);
		if (CellHolders)
		{
			for (auto It = CellHolders->CreateIterator(); It; ++It)
			{
				if (It->Value == HolderEntity)
				{
					It.RemoveCurrent();
					break;
				}
			}
			if (CellHolders->IsEmpty())
			{
				CellSkinnedISMHolders.Remove(Cell);
			}
		}
	}
	else
	{
		const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = EntityManager.GetConstSharedFragmentDataChecked<FArcMassSkinnedMeshFragment>(HolderEntity);
		USkinnedAsset* Asset = SkinnedMeshFrag.SkinnedAsset.Get();
		if (Asset)
		{
			FBoxSphereBounds AssetBounds = Asset->GetBounds();
			FBox AccumulatedBounds(ForceInit);
			for (const FArcSkinnedMeshInstanceData& Inst : SkinnedISMFrag->PerInstanceData)
			{
				FTransform InstTransform(Inst.Transform);
				FBox TransformedBox = AssetBounds.GetBox().TransformBy(InstTransform);
				AccumulatedBounds += TransformedBox;
			}
			PrimFrag->LocalBounds = FBoxSphereBounds(AccumulatedBounds);
			PrimFrag->WorldBounds = FBoxSphereBounds(AccumulatedBounds);
		}

		RenderState->GetRenderStateHelper().MarkRenderStateDirty();
	}
}

void UArcEntityVisualizationSubsystem::BatchActivateSkinnedISMEntities(
	TArray<FArcPendingSkinnedISMActivation>&& PendingActivations,
	FMassExecutionContext& Context)
{
	if (PendingActivations.IsEmpty())
	{
		return;
	}

	TWeakObjectPtr<UArcEntityVisualizationSubsystem> WeakThis(this);
	Context.Defer().PushCommand<FMassDeferredCreateCommand>(
		[WeakThis, Pending = MoveTemp(PendingActivations)]
		(FMassEntityManager& EntityManager) mutable
	{
		UArcEntityVisualizationSubsystem* Subsystem = WeakThis.Get();
		if (!Subsystem)
		{
			return;
		}

		for (FArcPendingSkinnedISMActivation& Activation : Pending)
		{
			if (!EntityManager.IsEntityValid(Activation.SourceEntity))
			{
				continue;
			}

			const FMassOverrideMaterialsFragment* OverrideMatsPtr =
				Activation.bHasOverrideMats ? &Activation.OverrideMats : nullptr;

			FMassEntityHandle HolderEntity = Subsystem->FindOrCreateSkinnedISMHolder(
				Activation.Cell,
				Activation.SkinnedMeshFrag,
				Activation.VisMeshConfigFrag,
				OverrideMatsPtr,
				EntityManager);

			if (!HolderEntity.IsValid())
			{
				continue;
			}

			int32 InstanceIndex = Subsystem->AddSkinnedISMInstance(
				HolderEntity, Activation.WorldTransform, 0, EntityManager);

			EntityManager.AddFragmentToEntity(
				Activation.SourceEntity,
				FArcVisISMInstanceFragment::StaticStruct(),
				[HolderEntity, InstanceIndex](void* Fragment, const UScriptStruct&)
			{
				FArcVisISMInstanceFragment* ISMFrag = static_cast<FArcVisISMInstanceFragment*>(Fragment);
				ISMFrag->HolderEntity = HolderEntity;
				ISMFrag->InstanceIndex = InstanceIndex;
			});
		}
	});
}

// ---------------------------------------------------------------------------
// UArcVisEntitySpawnLibrary
// ---------------------------------------------------------------------------

FMassEntityHandle UArcVisEntitySpawnLibrary::SpawnVisEntity(UObject* WorldContextObject, const UMassEntityConfigAsset* EntityConfig, FTransform Transform)
{
	if (!WorldContextObject || !EntityConfig)
	{
		return FMassEntityHandle();
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return FMassEntityHandle();
	}

	UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
	if (!SpawnerSubsystem)
	{
		return FMassEntityHandle();
	}

	const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);
	if (!EntityTemplate.IsValid())
	{
		return FMassEntityHandle();
	}

	TArray<FMassEntityHandle> SpawnedEntities;
	TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext = SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, SpawnedEntities);

	if (SpawnedEntities.IsEmpty())
	{
		return FMassEntityHandle();
	}

	// Set the transform on the spawned entity
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(SpawnedEntities[0]);
	if (TransformFragment)
	{
		TransformFragment->SetTransform(Transform);
	}

	return SpawnedEntities[0];
}
