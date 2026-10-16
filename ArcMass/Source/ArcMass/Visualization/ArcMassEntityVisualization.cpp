// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityVisualization.h"

#include "ArcVisSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif // WITH_EDITOR
#include "GameFramework/Actor.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "MassSpawnerSubsystem.h"
#include "MassCommonFragments.h"
#include "Mass/Commands.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "ArcMassRenderLogs.h"
#include "Mesh/ArcRenderStateHelper.h"
#include "Mesh/ArcPrimitiveRenderStateHelper.h"
#include "Mesh/ArcISMRenderStateHelper.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "Mesh/ArcRenderMeshUtils.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassInstancedSkinnedMeshRenderStateHelper.h"
#include "InstancedSkinnedMeshSceneProxyDesc.h"
#include "MassWorldUtils.h"

TAutoConsoleVariable<bool> CVarArcDebugDrawVisualizationGrid(
	TEXT("arc.mass.DebugDrawVisualizationGrid"),
	false,
	TEXT("Toggles debug drawing for entity visualization grid active cells (0 = off, 1 = on)"));

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace ArcMassEntityVisualization_Private
{
	/**
	 * Compute a by-value content hash of every proxy-baked property so that two
	 * configs produce the same hash ONLY when they would render identically in the
	 * same ISM.  Used to build FArcVisISMHolderKey::ConfigHash.
	 *
	 * Fields folded in:
	 *   • All shadow/lighting/decal/stencil/sort/distance-cull flags from
	 *     FArcMassVisualizationMeshConfigFragment (including HiddenInGame — cheap,
	 *     harmless to include even though it is not written to the proxy desc today).
	 *   • LightingChannels (bChannel0/1/2 combined individually).
	 *   • VisibleInReflections, VisibleInRayTracing, VisibleInReflectionCaptures.
	 *   • MinDrawDistance, LDMaxDrawDistance (float, hashed via GetTypeHash).
	 *   • CustomDepthStencilWriteMask (enum, cast to uint8).
	 *   • CustomDepthStencilValue (int64), TranslucencySortPriority (int32).
	 *   • Each UMaterialInterface* slot in OverrideMats in order (pointer identity
	 *     is stable for loaded assets; null slot hashes as 0).  If OverrideMats is
	 *     null the loop is skipped and a sentinel constant (0) is folded in to keep
	 *     "no overrides" distinct from "one null-slot override".
	 *
	 * NOTE: if per-instance custom data (PerInstanceSMCustomData stride) is ever
	 * introduced, fold that stride in here too — holders cannot be shared across
	 * differing strides.
	 */
	static uint32 ComputeISMHolderConfigHash(
		const FArcMassVisualizationMeshConfigFragment& VisCfg,
		const FArcOverrideMaterialsFragment* OverrideMats)
	{
		uint32 Hash = 0;

		// Integer / enum scalars
		Hash = HashCombine(Hash, GetTypeHash(static_cast<int64>(VisCfg.CustomDepthStencilValue)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<int32>(VisCfg.TranslucencySortPriority)));

		// Booleans — bitfield members, cast to bool for a stable 0/1 value
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.HiddenInGame)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.CastShadow)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.CastShadowAsTwoSided)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.CastHiddenShadow)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.AffectDynamicIndirectLighting)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.AffectIndirectLightingWhileHidden)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.ReceivesDecals)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.NeverDistanceCull)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.CastFarShadow)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.CastInsetShadow)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.CastContactShadow)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.RenderCustomDepth)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.VisibleInReflections)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.VisibleInRayTracing)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.VisibleInReflectionCaptures)));

		// LightingChannels — hash each channel bool individually
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.LightingChannels.bChannel0)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.LightingChannels.bChannel1)));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<bool>(VisCfg.LightingChannels.bChannel2)));

		// Floats
		Hash = HashCombine(Hash, GetTypeHash(VisCfg.MinDrawDistance));
		Hash = HashCombine(Hash, GetTypeHash(VisCfg.LDMaxDrawDistance));

		// Enum (stencil write mask)
		Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(VisCfg.CustomDepthStencilWriteMask)));

		// Material overrides — hash each slot pointer in order so that different
		// material sets produce different hashes (not just a bool "has overrides").
		if (OverrideMats != nullptr)
		{
			for (const TObjectPtr<UMaterialInterface>& MatPtr : OverrideMats->OverrideMaterials)
			{
				Hash = HashCombine(Hash, GetTypeHash(MatPtr.Get()));
			}
		}
		else
		{
			// Sentinel: distinguish "no override fragment" from an empty override array.
			Hash = HashCombine(Hash, 0u);
		}

		return Hash;
	}

	/**
	 * Bootstrap-create the GPU scene proxy for a holder whose helper has no live proxy yet.
	 *
	 * MarkRenderStateDirty() is guarded on IsRenderStateCreated()/IsRenderStateDelayed(), so it
	 * silently no-ops before the first proxy exists. The initial create used to be done by the
	 * per-frame UArcCreateRenderStateProcessor poll (deleted in the render-state event-driven
	 * rollout); for live ISM/skinned holders nothing replaced it — the create observer is
	 * partitioned to FArcRenderStaticMeshFragment (holders don't carry it) and the ISM
	 * create-at-tail lives in the dormant candidate-tag processor. Create directly from the
	 * now-populated instance buffer, mirroring the deleted poll's batching.
	 */
	void CreateRenderStateIfNeeded(FArcRenderStateHelper& Helper)
	{
		if (Helper.IsRenderStateCreated() || !Helper.ShouldCreateRenderState())
		{
			return;
		}

		FRegisterComponentContext BatchingContext(nullptr);
		BatchingContext.Initialize(Helper.GetWorld());
		Helper.CreateRenderState(&BatchingContext);
		BatchingContext.Process();
	}
} // namespace ArcMassEntityVisualization_Private

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

#if WITH_EDITOR
	MaterialCompilationFinishedHandle = UMaterial::OnMaterialCompilationFinished().AddWeakLambda(
		this, [this](UMaterialInterface* Material){ OnMaterialCompilationFinished(Material); });
#endif // WITH_EDITOR
}

void UArcEntityVisualizationSubsystem::PreDeinitialize()
{
	// FMassEntityManager::Deinitialize() bulk-destroys entities without firing observers,
	// so UMassDestroyRenderStateProcessor never runs for ISM holder entities.
	// PreDeinitialize runs on ALL subsystems before any Deinitialize, so the entity manager
	// is guaranteed to still be alive here.
	if (UWorld* World = GetWorld())
	{
		int32 SweptStaticHolders = 0;
		int32 SweptSkinnedHolders = 0;
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
		for (TPair<FIntVector, TMap<FArcISMHolderKey, FMassEntityHandle>>& CellPair : CellISMHolders)
		{
			for (TPair<FArcISMHolderKey, FMassEntityHandle>& HolderPair : CellPair.Value)
			{
				FMassEntityHandle HolderEntity = HolderPair.Value;
				if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
				{
					FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(HolderEntity);
					if (RenderState)
					{
						RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);
						++SweptStaticHolders;
					}
				}

				// Erase authoritative dense-index maps from the registry — entity manager is still alive here.
				ISMHolderRegistry.HolderInstanceKeys.Remove(HolderEntity);
				ISMHolderRegistry.HolderEntityToDenseIndex.Remove(HolderEntity);
			}
		}

		for (TPair<FIntVector, TMap<FArcVisSkinnedISMHolderKey, FMassEntityHandle>>& CellPair : CellSkinnedISMHolders)
		{
			for (TPair<FArcVisSkinnedISMHolderKey, FMassEntityHandle>& HolderPair : CellPair.Value)
			{
				FMassEntityHandle HolderEntity = HolderPair.Value;
				if (HolderEntity.IsValid() && EntityManager.IsEntityValid(HolderEntity))
				{
					FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(HolderEntity);
					if (RenderState)
					{
						RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);
						FArcMassInstancedSkinnedMeshRenderStateHelper* SkinnedHelper =
							static_cast<FArcMassInstancedSkinnedMeshRenderStateHelper*>(&RenderState->GetRenderStateHelper());
						SkinnedHelper->DestroyMeshObject();
						++SweptSkinnedHolders;
					}
				}
			}
		}

		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] VisSubsystem PreDeinitialize: swept %d static + %d skinned holders (world teardown safety net)",
			SweptStaticHolders, SweptSkinnedHolders);
	}

	Super::PreDeinitialize();
}

void UArcEntityVisualizationSubsystem::Deinitialize()
{
#if WITH_EDITOR
	UMaterial::OnMaterialCompilationFinished().Remove(MaterialCompilationFinishedHandle);
	MaterialCompilationFinishedHandle.Reset();
	if (PostCompileRebuildTickHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PostCompileRebuildTickHandle);
		PostCompileRebuildTickHandle.Reset();
	}
#endif // WITH_EDITOR

	CellISMHolders.Empty();
	CellSkinnedISMHolders.Empty();
	MeshGrid.Clear();
	PhysicsGrid.Clear();
	Super::Deinitialize();
}

#if WITH_EDITOR

void UArcEntityVisualizationSubsystem::OnMaterialCompilationFinished(UMaterialInterface* /*Material*/)
{
	// Arm a debounced ticker to rebuild existing holders once shader compilation settles.
	bNeedsPostCompileHolderRebuild = true;
	PostCompileRebuildPollTicks = 0;

	if (!PostCompileRebuildTickHandle.IsValid())
	{
		PostCompileRebuildTickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float /*DeltaTime*/) -> bool
			{
				++PostCompileRebuildPollTicks;

				if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
				{
					if (PostCompileRebuildPollTicks <= 1200)
					{
						return true; // still compiling, keep ticking
					}

					// Timeout — give up waiting, log a warning.
					UE_LOGF(LogMass, Warning,"UArcEntityVisualizationSubsystem: shader compilation did not settle after 1200 ticks; "
						     "aborting post-compile holder rebuild.");
					PostCompileRebuildTickHandle.Reset();
					bNeedsPostCompileHolderRebuild = false;
					PostCompileRebuildPollTicks = 0;
					return false;
				}

				// Shaders settled — rebuild if still needed.
				if (bNeedsPostCompileHolderRebuild)
				{
					bNeedsPostCompileHolderRebuild = false;
					RebuildAllHoldersRenderState();
				}

				PostCompileRebuildTickHandle.Reset();
				PostCompileRebuildPollTicks = 0;
				return false;
			}),
			0.25f);
	}
}

void UArcEntityVisualizationSubsystem::RebuildAllHoldersRenderState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	// Rebuild static ISM holders.
	for (TPair<FIntVector, TMap<FArcISMHolderKey, FMassEntityHandle>>& CellPair : CellISMHolders)
	{
		for (TPair<FArcISMHolderKey, FMassEntityHandle>& HolderPair : CellPair.Value)
		{
			FMassEntityHandle HolderEntity = HolderPair.Value;
			if (!HolderEntity.IsValid() || !EntityManager.IsEntityValid(HolderEntity))
			{
				continue;
			}
			FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(HolderEntity);
			if (!RenderState)
			{
				continue;
			}
			FArcPrimitiveRenderStateHelper& Helper = RenderState->GetRenderStateHelper<FArcPrimitiveRenderStateHelper>();
			if (Helper.GetSceneProxy())
			{
				// Live proxy: refresh Nanite material bins in place instead of freeing + rebuilding the proxy.
				// On an editor material shader recompile the material UB is reallocated; destroy+recreate frees
				// the old refcounted Nanite shading entry (and its transient-buffer SRVs) while an in-flight
				// base-pass command may still reference it -> use-after-free. Keeping the proxy alive and
				// re-caching in FIFO render-command order avoids the free-during-churn.
				// (Create-branch below is safe: this method is WITH_EDITOR-only and CreateRenderState's
				//  check(ProxyState==None) can only trip via the !WITH_EDITOR Delayed PSO-precache state.)
				Helper.MarkRenderStateDirty();
			}
			else if (Helper.ShouldCreateRenderState())
			{
				FRegisterComponentContext Ctx(nullptr);
				Ctx.Initialize(Helper.GetWorld());
				Helper.CreateRenderState(&Ctx);
				Ctx.Process();
			}
		}
	}

	// Rebuild skinned ISM holders.
	for (TPair<FIntVector, TMap<FArcVisSkinnedISMHolderKey, FMassEntityHandle>>& CellPair : CellSkinnedISMHolders)
	{
		for (TPair<FArcVisSkinnedISMHolderKey, FMassEntityHandle>& HolderPair : CellPair.Value)
		{
			FMassEntityHandle HolderEntity = HolderPair.Value;
			if (!HolderEntity.IsValid() || !EntityManager.IsEntityValid(HolderEntity))
			{
				continue;
			}
			FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(HolderEntity);
			if (!RenderState)
			{
				continue;
			}
			FArcRenderStateHelper& Helper = RenderState->GetRenderStateHelper();
			Helper.DestroyRenderState(nullptr);
			if (Helper.ShouldCreateRenderState())
			{
				FRegisterComponentContext Ctx(nullptr);
				Ctx.Initialize(Helper.GetWorld());
				Helper.CreateRenderState(&Ctx);
				Ctx.Process();
			}
		}
	}
}

#endif // WITH_EDITOR

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

FMassEntityHandle UArcEntityVisualizationSubsystem::FindOrCreateISMHolder(
	const FIntVector& Cell,
	const FArcMassStaticMeshConfigFragment& StaticMeshConfigFrag,
	const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag,
	const FArcOverrideMaterialsFragment* OverrideMats,
	FMassEntityManager& EntityManager)
{
	FArcISMHolderKey Key;
	Key.Mesh = StaticMeshConfigFrag.Mesh;
	Key.ConfigHash = ArcMassEntityVisualization_Private::ComputeISMHolderConfigHash(VisMeshConfigFrag, OverrideMats);

	// Convert config fragments to engine fragments — registry never sees ArcMass config types.
	FArcStaticMeshFragment StaticMeshFrag = ArcMass::Visualization::ToEngineFragment(StaticMeshConfigFrag);
	FArcVisualizationMeshFragment VisMeshFrag = ArcMass::Visualization::ToEngineFragment(VisMeshConfigFrag);

	TMap<FArcISMHolderKey, FMassEntityHandle>& Bucket = CellISMHolders.FindOrAdd(Cell);
	return ISMHolderRegistry.FindOrCreateHolder(Bucket, Key, StaticMeshFrag, VisMeshFrag, OverrideMats, EntityManager);
}

int32 UArcEntityVisualizationSubsystem::AddISMInstance(
	FMassEntityHandle HolderEntity,
	FMassEntityHandle SourceEntity,
	int32 SubIndex,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager,
	bool bDeferBoundsAndCullingRefresh)
{
	return ISMHolderRegistry.AddInstance(HolderEntity, SourceEntity, SubIndex, WorldTransform, EntityManager, bDeferBoundsAndCullingRefresh);
}

void UArcEntityVisualizationSubsystem::RemoveISMInstance(
	FMassEntityHandle HolderEntity,
	FMassEntityHandle SourceEntity,
	int32 SubIndex,
	const FIntVector& Cell,
	FMassEntityManager& EntityManager)
{
	const bool bEmptied = ISMHolderRegistry.RemoveInstance(
		HolderEntity,
		SourceEntity,
		SubIndex,
		EntityManager,
		[](FMassEntityManager& EM, FMassEntityHandle MovedSource, int32 NewIndex)
		{
			// The command may flush after the entity is gone (e.g. PIE teardown).
			if (!EM.IsEntityValid(MovedSource))
			{
				return;
			}
			FArcVisISMInstanceFragment* CacheFrag = EM.GetFragmentDataPtr<FArcVisISMInstanceFragment>(MovedSource);
			if (CacheFrag)
			{
				CacheFrag->InstanceIndex = NewIndex;
			}
		});

	if (bEmptied)
	{
		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] VisSubsystem RemoveISMInstance: holder=[%d:%d] emptied — evicting from cell (%d,%d,%d)",
			HolderEntity.Index, HolderEntity.SerialNumber, Cell.X, Cell.Y, Cell.Z);

		// Holder was destroyed by the registry — evict it from the cell bucket.
		TMap<FArcISMHolderKey, FMassEntityHandle>* CellHolders = CellISMHolders.Find(Cell);
		if (CellHolders)
		{
			for (TMap<FArcISMHolderKey, FMassEntityHandle>::TIterator It = CellHolders->CreateIterator(); It; ++It)
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
}

void UArcEntityVisualizationSubsystem::DestroyCellHolders(const FIntVector& Cell, FMassEntityManager& EntityManager)
{
	TMap<FArcISMHolderKey, FMassEntityHandle>* CellHolders = CellISMHolders.Find(Cell);
	if (!CellHolders)
	{
		return;
	}

	for (TPair<FArcISMHolderKey, FMassEntityHandle>& HolderPair : *CellHolders)
	{
		ISMHolderRegistry.ClearHolder(HolderPair.Value, EntityManager);
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

		// Track which holders received at least one new instance this batch so we can
		// recompute bounds and refresh culling once per holder instead of once per add.
		TSet<FMassEntityHandle> TouchedHolders;

		for (FArcPendingISMActivation& Activation : Pending)
		{
			if (!EntityManager.IsEntityValid(Activation.SourceEntity))
			{
				continue;
			}

			// Guard against same-frame activate->deactivate: if the flag was cleared between the
			// synchronous signal pass and this deferred flush, deinit already ran and will not
			// reclaim any instances we add here -- skip to avoid orphaned ISM instances.
			const FArcVisRepresentationFragment* RepPtr =
				EntityManager.GetFragmentDataPtr<FArcVisRepresentationFragment>(Activation.SourceEntity);
			if (!RepPtr || !RepPtr->bHasMeshRendering)
			{
				continue;
			}

			const FArcOverrideMaterialsFragment* OverrideMatsPtr =
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
				HolderEntity, Activation.SourceEntity, 0, Activation.WorldTransform, EntityManager,
				/*bDeferBoundsAndCullingRefresh=*/true);

			TouchedHolders.Add(HolderEntity);

			EntityManager.AddFragmentToEntity(
				Activation.SourceEntity,
				FArcVisISMInstanceFragment::StaticStruct(),
				[HolderEntity, InstanceIndex](void* Fragment, const UScriptStruct&)
			{
				FArcVisISMInstanceFragment* ISMFrag = static_cast<FArcVisISMInstanceFragment*>(Fragment);
				ISMFrag->HolderEntity = HolderEntity;
				ISMFrag->InstanceIndex = InstanceIndex; // cache only — authoritative index lives in ISMHolderRegistry.HolderEntityToDenseIndex
			});
		}

		// Recompute bounds and refresh culling once per holder that was touched this batch.
		// This replaces the O(N) x N_adds = O(N^2) per-holder work that would occur if each
		// AddISMInstance call recomputed bounds independently.
		for (FMassEntityHandle HolderEntity : TouchedHolders)
		{
			Subsystem->ISMHolderRegistry.RefreshHolderBounds(HolderEntity, EntityManager);
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
	Composition.Add<FArcRenderStateFragment>();
	Composition.Add<FArcRenderPrimitiveFragment>();
	Composition.Add<FArcMassRenderISMSkinnedFragment>();
	SkinnedISMHolderArchetype = EntityManager.CreateArchetype(Composition, FMassArchetypeCreationParams{TEXT("ArcVisSkinnedISMHolder")});

	FMassElementBitSet CompositionWithMats = Composition;
	CompositionWithMats.Add<FArcOverrideMaterialsFragment>();
	SkinnedISMHolderArchetypeWithMats = EntityManager.CreateArchetype(CompositionWithMats, FMassArchetypeCreationParams{TEXT("ArcVisSkinnedISMHolderMats")});

	bSkinnedISMHolderArchetypesInitialized = true;
}

FMassEntityHandle UArcEntityVisualizationSubsystem::FindOrCreateSkinnedISMHolder(
	const FIntVector& Cell,
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag,
	const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag,
	const FArcOverrideMaterialsFragment* OverrideMats,
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
	FArcVisualizationMeshFragment VisMeshFrag = ArcMass::Visualization::ToEngineFragment(VisMeshConfigFrag);
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
	FArcRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FArcRenderPrimitiveFragment>();
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
	SceneProxyDesc->bVisibleInReflections = VisMeshConfigFrag.VisibleInReflections;
	SceneProxyDesc->bVisibleInRayTracing = VisMeshConfigFrag.VisibleInRayTracing;
	SceneProxyDesc->bVisibleInReflectionCaptures = VisMeshConfigFrag.VisibleInReflectionCaptures;
	if (VisMeshConfigFrag.NeverDistanceCull)
	{
		SceneProxyDesc->CachedMaxDrawDistance = 0.0f;
	}
	SkinnedISMFrag.SceneProxyDesc = SceneProxyDesc;

	FArcRenderStateFragment& RenderState = EntityView.GetFragmentData<FArcRenderStateFragment>();
	RenderState.CreateRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>(
		HolderEntity, &EntityManager, PrimFrag, OverrideMats);

	CellHolders.Add(Key, HolderEntity);
	return HolderEntity;
}

int32 UArcEntityVisualizationSubsystem::AddSkinnedISMInstance(
	FMassEntityHandle HolderEntity,
	const FTransform& WorldTransform,
	uint32 AnimationIndex,
	FMassEntityManager& EntityManager,
	FMassEntityHandle SourceEntity,
	int32 SubIndex)
{
	FArcMassRenderISMSkinnedFragment* SkinnedISMFrag = EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(HolderEntity);
	FArcRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FArcRenderPrimitiveFragment>(HolderEntity);
	FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(HolderEntity);

	if (!SkinnedISMFrag || !PrimFrag || !RenderState)
	{
		return INDEX_NONE;
	}

	FArcSkinnedMeshInstanceData InstanceData;
	InstanceData.Transform = FTransform3f(WorldTransform);
	InstanceData.AnimationIndex = AnimationIndex;

	// Dense append — PerInstanceData is a TArray (swap-removed on RemoveSkinnedISMInstance to stay
	// hole-free), so TArray::Add returns the dense index that is the authoritative instance ordinal.
	const int32 DenseIndex = SkinnedISMFrag->PerInstanceData.Add(InstanceData);

	// Keep the flat stride-NumCustomDataFloats custom-data buffer parallel to PerInstanceData. The
	// runtime subsystem never writes custom data, but the block must exist so the swap-remove stride
	// math stays valid. No-op when NumCustomDataFloats == 0.
	if (SkinnedISMFrag->NumCustomDataFloats > 0)
	{
		SkinnedISMFrag->PerInstanceCustomData.AddZeroed(SkinnedISMFrag->NumCustomDataFloats);
	}

#if WITH_EDITOR
	// The runtime subsystem path does not produce typed-element hit proxies (those are an editor /
	// placed-entity registry concern). Keep PerInstanceHitProxy parallel only if a prior add already
	// populated it; otherwise it stays empty and the swap-remove guards on IsValidIndex.
	if (SkinnedISMFrag->PerInstanceHitProxy.Num() == DenseIndex && DenseIndex > 0)
	{
		SkinnedISMFrag->PerInstanceHitProxy.Add(TRefCountPtr<HArcTypeElementHandleHitProxy>());
	}
#endif // WITH_EDITOR

	// Record the {SourceEntity, SubIndex} key at this dense slot in BOTH authoritative structures,
	// mirroring FArcSkinnedMeshHolderRegistry. Removal resolves the live dense index from the reverse
	// map, so a sibling swap-remove can never stale a caller-cached index (the composite-holder fix).
	const FArcHolderInstanceKey InstanceKey{SourceEntity, SubIndex};
	TArray<FArcHolderInstanceKey>& InstanceKeys = SkinnedHolderInstanceKeys.FindOrAdd(HolderEntity);
	TMap<FArcHolderInstanceKey, int32>& EntityToDense = SkinnedHolderEntityToDenseIndex.FindOrAdd(HolderEntity);
	check(InstanceKeys.Num() == DenseIndex);
	InstanceKeys.Add(InstanceKey);
	EntityToDense.Add(InstanceKey, DenseIndex);

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

	// First add (no proxy yet) bootstrap-creates the proxy from the populated buffer; the manager
	// promotes Initial -> Tracked inside CreateRenderState. Every subsequent add goes through the
	// incremental append, which pushes only the new instance to the live proxy (no proxy recreation,
	// so the Nanite RT RayTracingId is preserved). Only when no live proxy exists (false) do we
	// bootstrap-create instead.
	FArcRenderStateHelper& SkinnedHelper = RenderState->GetRenderStateHelper();
	if (!SkinnedHelper.IsRenderStateCreated())
	{
		ArcMassEntityVisualization_Private::CreateRenderStateIfNeeded(SkinnedHelper);
	}
	else
	{
		FArcMassInstancedSkinnedMeshRenderStateHelper& SkinnedIncrementalHelper =
			RenderState->GetRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>();
		if (!SkinnedIncrementalHelper.AppendInstanceIncremental(DenseIndex))
		{
			ArcMassEntityVisualization_Private::CreateRenderStateIfNeeded(SkinnedHelper);
		}
	}

	return DenseIndex;
}

void UArcEntityVisualizationSubsystem::RemoveSkinnedISMInstance(
	FMassEntityHandle HolderEntity,
	const FArcHolderInstanceKey& Key,
	const FIntVector& Cell,
	FMassEntityManager& EntityManager)
{
	if (!HolderEntity.IsValid() || !EntityManager.IsEntityValid(HolderEntity))
	{
		return;
	}

	FArcMassRenderISMSkinnedFragment* SkinnedISMFrag = EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(HolderEntity);
	FArcRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FArcRenderPrimitiveFragment>(HolderEntity);
	FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(HolderEntity);

	if (!SkinnedISMFrag || !PrimFrag || !RenderState)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] VisSubsystem RemoveSkinnedISMInstance: holder=[%d:%d] source=[%d:%d] valid entity but missing fragments (Skinned=%d Prim=%d RS=%d)",
			HolderEntity.Index, HolderEntity.SerialNumber, Key.SourceEntity.Index, Key.SourceEntity.SerialNumber,
			SkinnedISMFrag != nullptr, PrimFrag != nullptr, RenderState != nullptr);
		return;
	}

	if (SkinnedISMFrag->PerInstanceData.Num() == 0)
	{
		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] VisSubsystem RemoveSkinnedISMInstance: holder=[%d:%d] source=[%d:%d] holder already emptied this batch — no-op",
			HolderEntity.Index, HolderEntity.SerialNumber, Key.SourceEntity.Index, Key.SourceEntity.SerialNumber);
		return;
	}

	TArray<FArcHolderInstanceKey>* InstanceKeys = SkinnedHolderInstanceKeys.Find(HolderEntity);
	TMap<FArcHolderInstanceKey, int32>* EntityToDense = SkinnedHolderEntityToDenseIndex.Find(HolderEntity);

	if (!InstanceKeys || !EntityToDense)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] VisSubsystem RemoveSkinnedISMInstance: holder=[%d:%d] source=[%d:%d] has %d instances but NO authoritative maps — bookkeeping desync",
			HolderEntity.Index, HolderEntity.SerialNumber, Key.SourceEntity.Index, Key.SourceEntity.SerialNumber,
			SkinnedISMFrag->PerInstanceData.Num());
		return;
	}

	// Resolve the CURRENT dense index from the authoritative reverse map. A sibling swap-remove may
	// have moved this key since the caller observed it, so the cached/old index is never trusted.
	const int32* IndexPtr = EntityToDense->Find(Key);
	if (!IndexPtr)
	{
		// Unknown key — already removed, or never tracked. No-op safely.
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] VisSubsystem RemoveSkinnedISMInstance: holder=[%d:%d] source=[%d:%d] key not tracked (already removed?) — no-op",
			HolderEntity.Index, HolderEntity.SerialNumber, Key.SourceEntity.Index, Key.SourceEntity.SerialNumber);
		return;
	}
	const int32 InstanceIndex = *IndexPtr;

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] VisSubsystem RemoveSkinnedISMInstance: holder=[%d:%d] source=[%d:%d] dense=%d count=%d",
		HolderEntity.Index, HolderEntity.SerialNumber, Key.SourceEntity.Index, Key.SourceEntity.SerialNumber,
		InstanceIndex, SkinnedISMFrag->PerInstanceData.Num());

	if (!SkinnedISMFrag->PerInstanceData.IsValidIndex(InstanceIndex))
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] VisSubsystem RemoveSkinnedISMInstance: holder=[%d:%d] source=[%d:%d] dense=%d OUT OF RANGE (count=%d) — map/array desync",
			HolderEntity.Index, HolderEntity.SerialNumber, Key.SourceEntity.Index, Key.SourceEntity.SerialNumber,
			InstanceIndex, SkinnedISMFrag->PerInstanceData.Num());
		return;
	}

	// Remove from the reverse map before mutating PerInstanceData.
	EntityToDense->Remove(Key);

	if (SkinnedISMFrag->PerInstanceData.Num() == 1)
	{
		// Last instance — destroy the holder entirely (DestroyRenderState tears down the proxy +
		// FPrimitiveSceneInfo; only this path is allowed to do so).
		InstanceKeys->Reset();

		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] VisSubsystem RemoveSkinnedISMInstance: holder=[%d:%d] LAST instance (source=[%d:%d]) — destroying render state + mesh object + deferring holder destroy",
			HolderEntity.Index, HolderEntity.SerialNumber, Key.SourceEntity.Index, Key.SourceEntity.SerialNumber);

		RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);

		FArcMassInstancedSkinnedMeshRenderStateHelper* SkinnedHelper =
			static_cast<FArcMassInstancedSkinnedMeshRenderStateHelper*>(&RenderState->GetRenderStateHelper());
		SkinnedHelper->DestroyMeshObject();

		EntityManager.Defer().DestroyEntity(HolderEntity);

		SkinnedHolderInstanceKeys.Remove(HolderEntity);
		SkinnedHolderEntityToDenseIndex.Remove(HolderEntity);

		TMap<FArcVisSkinnedISMHolderKey, FMassEntityHandle>* CellHolders = CellSkinnedISMHolders.Find(Cell);
		if (CellHolders)
		{
			for (TMap<FArcVisSkinnedISMHolderKey, FMassEntityHandle>::TIterator It = CellHolders->CreateIterator(); It; ++It)
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
		return;
	}

	// -----------------------------------------------------------------------------------------------
	// Dense swap-with-tail. The helper's RemoveInstanceIncremental calls InstanceDataManager
	// .RemoveAtSwap(InstanceIndex) (tail -> InstanceIndex); the fragment arrays MUST move the same
	// tail -> InstanceIndex so the manager's dense-index <-> id map stays in lockstep with the
	// fragment storage. This matches the Step-4 registry block exactly. INVARIANT: PerInstanceData,
	// PerInstanceCustomData (stride NumCustomDataFloats), PerInstanceHitProxy, and the dense-parallel
	// SkinnedHolderInstanceKeys array are ALL parallel and move together; the reverse map is repointed
	// for the moved tail key.
	// -----------------------------------------------------------------------------------------------
	const int32 LastIndex = SkinnedISMFrag->PerInstanceData.Num() - 1;
	const int32 Stride = SkinnedISMFrag->NumCustomDataFloats;

	if (InstanceIndex != LastIndex)
	{
		// Move the tail instance into the freed slot for every parallel array.
		SkinnedISMFrag->PerInstanceData[InstanceIndex] = SkinnedISMFrag->PerInstanceData[LastIndex];

		if (Stride > 0)
		{
			// Stride-aware slab copy: overwrite InstanceIndex's NumCustomDataFloats-wide block with
			// the tail's.
			FMemory::Memcpy(
				&SkinnedISMFrag->PerInstanceCustomData[InstanceIndex * Stride],
				&SkinnedISMFrag->PerInstanceCustomData[LastIndex * Stride],
				Stride * sizeof(float));
		}

#if WITH_EDITOR
		if (SkinnedISMFrag->PerInstanceHitProxy.IsValidIndex(LastIndex) && SkinnedISMFrag->PerInstanceHitProxy.IsValidIndex(InstanceIndex))
		{
			SkinnedISMFrag->PerInstanceHitProxy[InstanceIndex] = SkinnedISMFrag->PerInstanceHitProxy[LastIndex];
		}
#endif // WITH_EDITOR

		// Mirror the swap in the authoritative bookkeeping: the tail key now lives at InstanceIndex,
		// in BOTH the dense-parallel key array and the reverse map.
		const FArcHolderInstanceKey MovedKey = (*InstanceKeys)[LastIndex];
		(*InstanceKeys)[InstanceIndex] = MovedKey;
		(*EntityToDense)[MovedKey] = InstanceIndex;
	}

	// Pop the tail slot (always the last index regardless of whether a swap happened).
	SkinnedISMFrag->PerInstanceData.RemoveAt(LastIndex);
	InstanceKeys->RemoveAt(LastIndex);

	if (Stride > 0)
	{
		SkinnedISMFrag->PerInstanceCustomData.RemoveAt(LastIndex * Stride, Stride);
	}

#if WITH_EDITOR
	if (SkinnedISMFrag->PerInstanceHitProxy.IsValidIndex(LastIndex))
	{
		SkinnedISMFrag->PerInstanceHitProxy.RemoveAt(LastIndex);
	}
#endif // WITH_EDITOR

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

	// Incremental manager remove (RemoveAtSwap mirrors the dense swap above). Falls back to a full
	// MarkRenderStateDirty only when no live proxy exists.
	FArcMassInstancedSkinnedMeshRenderStateHelper& SkinnedHelper =
		RenderState->GetRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>();
	if (!SkinnedHelper.RemoveInstanceIncremental(InstanceIndex))
	{
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

			// Guard against same-frame activate->deactivate: if bHasMeshRendering was cleared
			// between the synchronous signal pass and this deferred flush, skip.
			const FArcVisRepresentationFragment* SkinnedRepPtr =
				EntityManager.GetFragmentDataPtr<FArcVisRepresentationFragment>(Activation.SourceEntity);
			if (!SkinnedRepPtr || !SkinnedRepPtr->bHasMeshRendering)
			{
				continue;
			}

			const FArcOverrideMaterialsFragment* OverrideMatsPtr =
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
				HolderEntity, Activation.WorldTransform, 0, EntityManager, Activation.SourceEntity);

			EntityManager.AddFragmentToEntity(
				Activation.SourceEntity,
				FArcVisISMInstanceFragment::StaticStruct(),
				[HolderEntity, InstanceIndex](void* Fragment, const UScriptStruct&)
			{
				FArcVisISMInstanceFragment* ISMFrag = static_cast<FArcVisISMInstanceFragment*>(Fragment);
				ISMFrag->HolderEntity = HolderEntity;
				ISMFrag->InstanceIndex = InstanceIndex; // informational only — removal is key-based ({SourceEntity, 0}), not by this cached index
			});
		}
	});
}

// ---------------------------------------------------------------------------
// UArcVisEntitySpawnLibrary
// ---------------------------------------------------------------------------

FMassEntityHandle UArcVisEntitySpawnLibrary::SpawnVisEntity(UObject* WorldContextObject, UArcMassEntityConfigAsset* EntityConfig, FTransform Transform)
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

	// Route through UArcMassSpawnerSubsystem so the spawn runs the ArcMass pipeline
	// (BuildSparseFragments, InitialSparseFragments, PostCommitInitializers) and seeds the
	// transform pre-Commit via FEntityBuilder. The engine UMassSpawnerSubsystem path skips all
	// of that and would set the transform after observers already fired.
	UArcMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UArcMassSpawnerSubsystem>();
	if (!SpawnerSubsystem)
	{
		return FMassEntityHandle();
	}

	return SpawnerSubsystem->SpawnEntity(EntityConfig, Transform);
}
