// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Mass/ArchetypeTypes.h"

// Forward declarations — callers include the fragments directly if they need full types.
struct FMassEntityManager;
struct FArcStaticMeshFragment;
struct FArcVisualizationMeshFragment;
struct FArcOverrideMaterialsFragment;
struct FArcTypeElementInstanceId;

/**
 * Key for ISM holder lookup — identifies a unique mesh+config combination.
 *
 * ConfigHash is a by-VALUE hash of every proxy-baked property in the source config fragment
 * plus the full ordered material-override array.  Two entities share a holder ONLY when every
 * rendered property matches.
 *
 * NOTE: if per-instance custom data (PerInstanceSMCustomData stride) is ever introduced, that
 * stride MUST also be folded into ConfigHash, because holders are not shared across differing
 * strides.  Today stride is 0 everywhere.
 *
 * Renamed from FArcVisISMHolderKey (subsystem-internal) to FArcISMHolderKey (registry-public).
 */
struct ARCMASSRENDER_API FArcISMHolderKey
{
	TWeakObjectPtr<const UStaticMesh> Mesh;
	uint32 ConfigHash = 0;

	friend bool operator==(const FArcISMHolderKey& A, const FArcISMHolderKey& B)
	{
		return A.Mesh == B.Mesh && A.ConfigHash == B.ConfigHash;
	}

	friend uint32 GetTypeHash(const FArcISMHolderKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Mesh), Key.ConfigHash);
	}
};

/**
 * Key for the ISM holder's authoritative reverse maps.
 * Single-mesh entities use SubIndex=0; composite entities use the per-sub-mesh ordinal
 * so that N instances from the same source entity in the same holder all have unique keys.
 */
struct ARCMASSRENDER_API FArcHolderInstanceKey
{
	FMassEntityHandle SourceEntity;
	int32 SubIndex = 0; // 0 for single-mesh; per-sub-mesh ordinal for composite

	friend bool operator==(const FArcHolderInstanceKey& A, const FArcHolderInstanceKey& B)
	{
		return A.SourceEntity == B.SourceEntity && A.SubIndex == B.SubIndex;
	}

	friend uint32 GetTypeHash(const FArcHolderInstanceKey& K)
	{
		return HashCombine(GetTypeHash(K.SourceEntity), ::GetTypeHash(K.SubIndex));
	}
};

/**
 * Cell-agnostic holder-mechanics registry for static-mesh ISM holders.
 *
 * Owns the dense-index book-keeping and archetype handles that were previously tangled into
 * UArcEntityVisualizationSubsystem.  The subsystem keeps spatial/grid/signal POLICY and calls
 * into this registry for the per-holder mechanics.
 *
 * Dependency direction:
 *   ArcMass → ArcMassRender (one-way).  This type lives in ArcMassRender and must NEVER pull
 *   in ArcMass config types (FArcMassStaticMeshConfigFragment, FArcMassVisualizationMeshConfigFragment).
 *   The subsystem converts those to engine fragments before passing them here.
 *
 * Thread safety: NOT thread-safe.  All calls must be issued from the game thread or inside a
 * deferred-command lambda that serialises with the entity manager.
 *
 * Authoritative book-keeping invariants:
 *   HolderInstanceKeys[Holder][DenseIndex] -> FArcHolderInstanceKey{SourceEntity, SubIndex}
 *     — parallel to PerInstanceSMData; invariant: no holes (swap-with-tail on remove).
 *   HolderEntityToDenseIndex[Holder][{SourceEntity, SubIndex}] -> DenseIndex
 *     — reverse map for O(1) lookup during RemoveInstance.
 *   Both maps are updated SYNCHRONOUSLY inside Add/RemoveInstance so that batch removals from
 *   a single holder within one deferred command see the latest indices.  The
 *   FArcVisISMInstanceFragment::InstanceIndex cache on source entities is updated only via
 *   EntityManager.Defer() and must never be read during a removal pass.
 */
struct ARCMASSRENDER_API FArcISMHolderRegistry
{
	/** Parallel to PerInstanceSMData — no holes invariant. */
	TMap<FMassEntityHandle, TArray<FArcHolderInstanceKey>> HolderInstanceKeys;

	/** Reverse map for O(1) dense-index lookup. */
	TMap<FMassEntityHandle, TMap<FArcHolderInstanceKey, int32>> HolderEntityToDenseIndex;

	/** Archetype without material override (identity: FTransform + FArcRenderState + FArcRenderPrimitive + FArcRenderISM). */
	FMassArchetypeHandle ISMHolderArchetype;

	/** Archetype with FArcOverrideMaterialsFragment shared fragment. */
	FMassArchetypeHandle ISMHolderArchetypeWithMats;

	bool bArchetypesInitialized = false;

	// -------------------------------------------------------------------------
	// Lifecycle
	// -------------------------------------------------------------------------

	/** One-time archetype bootstrap — idempotent (guarded by bArchetypesInitialized). */
	void InitializeArchetypes(FMassEntityManager& EntityManager);

	// -------------------------------------------------------------------------
	// Per-holder operations
	// -------------------------------------------------------------------------

	/**
	 * Look up or create the holder entity in Bucket for the given Key.
	 *
	 * Bucket is a cell-local map owned by the subsystem (e.g. CellISMHolders.FindOrAdd(Cell)).
	 * StaticMeshFrag / VisMeshFrag / OverrideMats are the ALREADY-CONVERTED engine render
	 * fragments — this registry never sees ArcMass config types.
	 *
	 * Returns the holder FMassEntityHandle (valid on success, invalid on allocation failure).
	 */
	FMassEntityHandle FindOrCreateHolder(
		TMap<FArcISMHolderKey, FMassEntityHandle>& Bucket,
		const FArcISMHolderKey& Key,
		const FArcStaticMeshFragment& StaticMeshFrag,
		const FArcVisualizationMeshFragment& VisMeshFrag,
		const FArcOverrideMaterialsFragment* OverrideMats,
		FMassEntityManager& EntityManager);

	/**
	 * Add a source entity's instance to the given ISM holder.
	 *
	 * SubIndex=0 for single-mesh entities; composite callers pass the per-sub-mesh ordinal.
	 * Returns the dense index assigned to this instance (INDEX_NONE on failure).
	 * bDeferBoundsAndCullingRefresh=true skips per-add bounds recalc and leaves proxy creation to
	 * RefreshHolderBounds, which the caller MUST call once per holder after the batch.
	 * InstanceId (editor only): when non-null and set, a per-instance HArcTypeElementHandleHitProxy
	 * is created in PerInstanceHitProxy at the same dense index, enabling viewport picking. The proxy
	 * stores this lightweight id (not a refcounted handle) and resolves the handle on demand at click
	 * time — see FArcTypeElementInstanceId.
	 */
	int32 AddInstance(
		FMassEntityHandle Holder,
		FMassEntityHandle Source,
		int32 SubIndex,
		const FTransform& WorldTransform,
		FMassEntityManager& EntityManager,
		bool bDeferBoundsAndCullingRefresh,
		const FArcTypeElementInstanceId* InstanceId = nullptr);

	/**
	 * Remove the instance identified by {Source, SubIndex} from the given ISM holder.
	 *
	 * Multi-instance path: swap-with-tail, update both authoritative maps, then issue a
	 * deferred set-command that calls OnInstanceMoved to patch the swapped entity's cache
	 * fragment.  OnInstanceMoved is captured BY VALUE — it is an owning TFunction; the patch
	 * runs in a deferred command AFTER RemoveInstance returns so a TFunctionRef would dangle.
	 *
	 * Last-instance path: destroys the render state, defers holder-entity destruction, and
	 * wipes both maps for the holder.
	 *
	 * Returns true when the holder was emptied (caller should evict it from its cell bucket).
	 * Returns false when other instances remain.
	 *
	 * Does NOT touch any cell map — cell eviction is the subsystem's responsibility.
	 */
	bool RemoveInstance(
		FMassEntityHandle Holder,
		FMassEntityHandle Source,
		int32 SubIndex,
		FMassEntityManager& EntityManager,
		TFunction<void(FMassEntityManager&, FMassEntityHandle MovedSource, int32 NewIndex)> OnInstanceMoved);

	/**
	 * Tear down a holder's render state + bookkeeping maps, then queue holder-entity destruction
	 * via EntityManager.Defer().  Does NOT touch any cell map.
	 *
	 * Used by DestroyCellHolders and PreDeinitialize so those paths don't orphan the registry maps.
	 */
	void ClearHolder(FMassEntityHandle Holder, FMassEntityManager& EntityManager);

	/**
	 * Recompute bounds (local + world) for the given holder, create its scene proxy if the deferred
	 * add path left it without one, and refresh culling via FArcISMRenderStateHelper::UpdateTransform().
	 *
	 * Called once per holder at the end of a batch-activate pass (amortises O(N^2) → O(N)).
	 */
	void RefreshHolderBounds(FMassEntityHandle Holder, FMassEntityManager& EntityManager);
};
