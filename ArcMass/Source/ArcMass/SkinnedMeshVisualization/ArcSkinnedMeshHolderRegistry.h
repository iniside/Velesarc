// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Mass/ArchetypeTypes.h"

// FArcHolderInstanceKey is used as a value type in TArray/TMap members — full type required.
#include "Mesh/ArcISMHolderRegistry.h"

// Forward declarations — callers include the fragments directly if they need full types.
struct FMassEntityManager;
struct FArcMassSkinnedMeshFragment;
struct FArcVisualizationMeshFragment;
struct FArcOverrideMaterialsFragment;
struct FArcTypeElementInstanceId;

/**
 * Key for skinned ISM holder lookup — identifies a unique skinned asset + config combination.
 *
 * ConfigHash is a by-value hash of every proxy-baked rendering property in the source config
 * plus the full ordered material-override presence. Two entities share a holder ONLY when every
 * rendered property matches.
 *
 * Upgraded from the runtime subsystem's 3-bool FArcVisSkinnedISMHolderKey to a hash so future
 * rendering props don't require adding new bool members and rebreaking operator==.
 */
struct ARCMASS_API FArcSkinnedISMHolderKey
{
	TWeakObjectPtr<const USkinnedAsset> Asset;
	uint32 ConfigHash = 0;

	friend bool operator==(const FArcSkinnedISMHolderKey& A, const FArcSkinnedISMHolderKey& B)
	{
		return A.Asset == B.Asset && A.ConfigHash == B.ConfigHash;
	}

	friend uint32 GetTypeHash(const FArcSkinnedISMHolderKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Asset), Key.ConfigHash);
	}
};

/**
 * Cell-agnostic holder-mechanics registry for skinned (instanced skeletal) ISM holders.
 *
 * Editor-only usage: drives componentless skinned proxy creation for placed-entity preview,
 * mirroring the static-mesh FArcISMHolderRegistry API exactly so actor call sites pattern-match.
 *
 * Lives in the ArcMass module (not ArcMassRender) because it is editor-only and depends on
 * FArcMassRenderISMSkinnedFragment + FArcMassInstancedSkinnedMeshRenderStateHelper which also
 * live in ArcMass. Moving those down to ArcMassRender would cause .generated.h churn and a
 * potential ArcMassRender -> ArcMass dep cycle.
 *
 * Holder-type invariant (relied on by the mixed-type actor dispatch in Steps 6-7):
 *   A static holder carries FArcRenderISMFragment.
 *   A skinned holder carries FArcMassRenderISMSkinnedFragment.
 *   These are mutually exclusive by archetype construction; an entity never carries both.
 *
 * Storage model (mirrors the static FArcISMHolderRegistry):
 *   PerInstanceData is a dense TArray kept hole-free via swap-with-tail on removal. The skinned
 *   helper drives incremental add/remove/transform updates through FInstanceDataManager, so a
 *   structural change pushes only a delta (no full proxy rebuild) when a live proxy exists.
 *
 * Book-keeping invariants:
 *   HolderInstanceKeys[Holder][DenseIndex]               -> FArcHolderInstanceKey{Source, SubIndex}
 *                                   Parallel to PerInstanceData; no-holes invariant (swap-with-tail).
 *   HolderEntityToDenseIndex[Holder][{Source, SubIndex}] -> DenseIndex
 *                                   Authoritative reverse map for O(1) lookup during RemoveInstance.
 *   Both maps are updated SYNCHRONOUSLY inside Add/RemoveInstance, in lockstep with the dense
 *   PerInstanceData / PerInstanceCustomData (stride NumCustomDataFloats) / PerInstanceHitProxy
 *   swap-remove. The FArcVisISMInstanceFragment::InstanceIndex on source entities is a CACHE,
 *   patched on swap only via the OnInstanceMoved deferred command — never read during a removal pass.
 *
 * Thread safety: NOT thread-safe. All calls must be issued from the game thread or inside a
 * deferred-command lambda that serialises with the entity manager.
 */
struct ARCMASS_API FArcSkinnedMeshHolderRegistry
{
	/** Parallel to dense PerInstanceData — stores FArcHolderInstanceKey per dense index (no holes). */
	TMap<FMassEntityHandle, TArray<FArcHolderInstanceKey>> HolderInstanceKeys;

	/** Authoritative reverse map: Holder -> {SourceEntity, SubIndex} -> dense index in PerInstanceData. */
	TMap<FMassEntityHandle, TMap<FArcHolderInstanceKey, int32>> HolderEntityToDenseIndex;

	/** Archetype without material override: {FTransformFragment, FArcRenderStateFragment,
	 *  FArcRenderPrimitiveFragment, FArcMassRenderISMSkinnedFragment}. */
	FMassArchetypeHandle SkinnedISMHolderArchetype;

	/** Archetype with FArcOverrideMaterialsFragment added as a shared fragment. */
	FMassArchetypeHandle SkinnedISMHolderArchetypeWithMats;

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
	 * Bucket is a cell-local (or actor-local) map owned by the caller.
	 * SkinnedMeshFrag / VisMeshFrag / OverrideMats are the ALREADY-BUILT engine shared fragments —
	 * this registry never sees ArcMass config fragments.
	 *
	 * Critically: SkinnedMeshFrag and VisMeshFrag are added to a FMassArchetypeSharedFragmentValues
	 * passed to BatchCreateEntities so that FArcMassInstancedSkinnedMeshRenderStateHelper::
	 * GetSkinnedMeshFragment() (GetConstSharedFragmentDataChecked) succeeds on first BuildInstanceData.
	 *
	 * Returns the holder FMassEntityHandle (valid on success, invalid on allocation failure).
	 */
	FMassEntityHandle FindOrCreateHolder(
		TMap<FArcSkinnedISMHolderKey, FMassEntityHandle>& Bucket,
		const FArcSkinnedISMHolderKey& Key,
		const FArcMassSkinnedMeshFragment& SkinnedMeshFrag,
		const FArcVisualizationMeshFragment& VisMeshFrag,
		const FArcOverrideMaterialsFragment* OverrideMats,
		FMassEntityManager& EntityManager);

	/**
	 * Add a source entity's instance to the given skinned ISM holder.
	 *
	 * SubIndex=0 for single-mesh entities.
	 * Returns the dense index assigned to this instance (INDEX_NONE on failure).
	 * bDeferBoundsAndCullingRefresh=true skips per-add bounds recalc and leaves proxy creation to
	 * RefreshHolderBounds, which the caller MUST call once per holder after the batch.
	 * InstanceId (editor only): when non-null and set, a per-instance HArcTypeElementHandleHitProxy
	 * is appended to PerInstanceHitProxy at the same dense index, enabling viewport picking.
	 */
	int32 AddInstance(
		FMassEntityHandle Holder,
		FMassEntityHandle Source,
		int32 SubIndex,
		const FTransform& WorldTransform,
		uint32 AnimationIndex,
		FMassEntityManager& EntityManager,
		bool bDeferBoundsAndCullingRefresh,
		const FArcTypeElementInstanceId* InstanceId = nullptr);

	/**
	 * Remove the instance identified by {Source, SubIndex} from the given skinned ISM holder.
	 *
	 * Multi-instance path: dense swap-with-tail on PerInstanceData / PerInstanceCustomData (stride
	 * NumCustomDataFloats) / PerInstanceHitProxy, repoints both authoritative maps for the moved tail
	 * key, then drives the incremental manager remove (MarkRenderStateDirty only as the no-proxy
	 * fallback). The OnInstanceMoved callback is INVOKED (via a deferred FMassDeferredSetCommand) so the
	 * moved source entity's FArcVisISMInstanceFragment::InstanceIndex cache is patched to the freed
	 * slot — captured by value; composite entities lacking the cache fragment make it a no-op for
	 * themselves. Pass a real callback (the cache-patch closure); pass nullptr only when no cache exists.
	 *
	 * Last-instance path: destroys the render state + mesh object, defers holder-entity destruction,
	 * and wipes both maps for the holder.
	 *
	 * Returns true when the holder was emptied (caller should evict it from the bucket).
	 * Returns false when other instances remain.
	 *
	 * Does NOT touch any bucket map — bucket eviction is the caller's responsibility.
	 */
	bool RemoveInstance(
		FMassEntityHandle Holder,
		FMassEntityHandle Source,
		int32 SubIndex,
		FMassEntityManager& EntityManager,
		TFunction<void(FMassEntityManager&, FMassEntityHandle MovedSource, int32 NewSlot)> OnInstanceMoved);

	/**
	 * Tear down a holder's render state + bookkeeping maps, then queue holder-entity destruction
	 * via EntityManager.Defer(). Does NOT touch any bucket map.
	 *
	 * Also calls DestroyMeshObject() (skinned-specific) to release the FSkeletalMeshObject and
	 * avoid leaking GPU resources.
	 */
	void ClearHolder(FMassEntityHandle Holder, FMassEntityManager& EntityManager);

	/**
	 * Recompute bounds from live PerInstanceData, create the scene proxy if the deferred add path left
	 * the holder without one, and push the bounds via FArcMassInstancedSkinnedMeshRenderStateHelper::
	 * UpdateTransform() (no proxy churn).
	 *
	 * Called once per holder at the end of a batch-add pass to amortise O(N^2) -> O(N).
	 */
	void RefreshHolderBounds(FMassEntityHandle Holder, FMassEntityManager& EntityManager);
};
