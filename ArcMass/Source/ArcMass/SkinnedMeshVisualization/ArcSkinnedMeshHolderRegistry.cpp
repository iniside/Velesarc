// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/SkinnedMeshVisualization/ArcSkinnedMeshHolderRegistry.h"

#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassInstancedSkinnedMeshRenderStateHelper.h"

#include "ArcMassRenderLogs.h"

#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityView.h"

#include "Mesh/ArcRenderStateHelper.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "Mesh/ArcISMHolderRegistry.h"

#include "Engine/SkinnedAsset.h"

// ---------------------------------------------------------------------------
// Internal helpers (file-scoped)
// ---------------------------------------------------------------------------

namespace ArcSkinnedMeshHolderRegistry_Private
{
	/**
	 * Create the GPU scene proxy for a skinned holder whose helper has no live proxy yet.
	 *
	 * MarkRenderStateDirty() is guarded on IsRenderStateCreated(), so it silently no-ops before the
	 * first proxy exists — creation must be driven directly.
	 */
	static void CreateRenderStateIfNeeded(FArcRenderStateHelper& Helper)
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

	/**
	 * Recompute accumulated bounds over all live PerInstanceData entries using the skinned asset's
	 * reference-pose bounds, and write the results into PrimFrag.
	 */
	static void RecalculateSkinnedBounds(
		FArcMassRenderISMSkinnedFragment& SkinnedFrag,
		const FArcMassSkinnedMeshFragment& SkinnedMeshFrag,
		FArcRenderPrimitiveFragment& PrimFrag)
	{
		USkinnedAsset* Asset = SkinnedMeshFrag.SkinnedAsset.Get();
		if (!Asset || SkinnedFrag.PerInstanceData.Num() == 0)
		{
			PrimFrag.LocalBounds = FBoxSphereBounds(ForceInit);
			PrimFrag.WorldBounds = FBoxSphereBounds(ForceInit);
			return;
		}

		const FBoxSphereBounds AssetBounds = Asset->GetBounds();
		FBox AccumulatedBounds(ForceInit);

		for (const FArcSkinnedMeshInstanceData& Inst : SkinnedFrag.PerInstanceData)
		{
			const FTransform InstTransform(Inst.Transform);
			AccumulatedBounds += AssetBounds.GetBox().TransformBy(InstTransform);
		}

		PrimFrag.LocalBounds = FBoxSphereBounds(AccumulatedBounds);
		PrimFrag.WorldBounds = FBoxSphereBounds(AccumulatedBounds);
	}
} // namespace ArcSkinnedMeshHolderRegistry_Private

// ---------------------------------------------------------------------------
// FArcSkinnedMeshHolderRegistry
// ---------------------------------------------------------------------------

void FArcSkinnedMeshHolderRegistry::InitializeArchetypes(FMassEntityManager& EntityManager)
{
	if (bArchetypesInitialized)
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

	bArchetypesInitialized = true;
}

FMassEntityHandle FArcSkinnedMeshHolderRegistry::FindOrCreateHolder(
	TMap<FArcSkinnedISMHolderKey, FMassEntityHandle>& Bucket,
	const FArcSkinnedISMHolderKey& Key,
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag,
	const FArcVisualizationMeshFragment& VisMeshFrag,
	const FArcOverrideMaterialsFragment* OverrideMats,
	FMassEntityManager& EntityManager)
{
	InitializeArchetypes(EntityManager);

	FMassEntityHandle* ExistingHolder = Bucket.Find(Key);
	if (ExistingHolder && ExistingHolder->IsValid() && EntityManager.IsEntityValid(*ExistingHolder))
	{
		return *ExistingHolder;
	}

	const FMassArchetypeHandle Archetype = OverrideMats ? SkinnedISMHolderArchetypeWithMats : SkinnedISMHolderArchetype;

	// Add the const shared fragments so FArcMassInstancedSkinnedMeshRenderStateHelper::
	// GetSkinnedMeshFragment() (GetConstSharedFragmentDataChecked) succeeds on first BuildInstanceData.
	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(FConstSharedStruct::Make(SkinnedMeshFrag));
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

	const FMassEntityHandle HolderEntity = CreatedEntities[0];
	FMassEntityView EntityView(EntityManager, HolderEntity);

	// Identity transform — instances carry their own world transforms.
	FTransformFragment& TransformFrag = EntityView.GetFragmentData<FTransformFragment>();
	TransformFrag.SetTransform(FTransform::Identity);

	FArcMassRenderISMSkinnedFragment& SkinnedISMFrag = EntityView.GetFragmentData<FArcMassRenderISMSkinnedFragment>();
	FArcRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FArcRenderPrimitiveFragment>();
	PrimFrag.bIsVisible = true;

	// Initialize SceneProxyDesc from the shared fragments — mirrors the runtime subsystem's
	// FindOrCreateSkinnedISMHolder shadow/rendering-prop assignment block.
	TSharedPtr<FInstancedSkinnedMeshSceneProxyDesc> SceneProxyDesc = MakeShared<FInstancedSkinnedMeshSceneProxyDesc>();
	SceneProxyDesc->SkinnedAsset = SkinnedMeshFrag.SkinnedAsset;
	SceneProxyDesc->TransformProvider = SkinnedMeshFrag.TransformProvider;
	SceneProxyDesc->CastShadow = VisMeshFrag.CastShadow;
	SceneProxyDesc->bCastShadowAsTwoSided = VisMeshFrag.CastShadowAsTwoSided;
	SceneProxyDesc->bCastHiddenShadow = VisMeshFrag.CastHiddenShadow;
	SceneProxyDesc->bCastFarShadow = VisMeshFrag.CastFarShadow;
	SceneProxyDesc->bCastInsetShadow = VisMeshFrag.CastInsetShadow;
	SceneProxyDesc->bCastContactShadow = VisMeshFrag.CastContactShadow;
	SceneProxyDesc->bReceivesDecals = VisMeshFrag.ReceivesDecals;
	SceneProxyDesc->bRenderCustomDepth = VisMeshFrag.RenderCustomDepth;
	SceneProxyDesc->CustomDepthStencilValue = static_cast<int32>(VisMeshFrag.CustomDepthStencilValue);
	SceneProxyDesc->TranslucencySortPriority = VisMeshFrag.TranslucencySortPriority;
	SceneProxyDesc->bAffectDynamicIndirectLighting = VisMeshFrag.AffectDynamicIndirectLighting;
	SceneProxyDesc->bAffectIndirectLightingWhileHidden = VisMeshFrag.AffectIndirectLightingWhileHidden;
	SceneProxyDesc->bVisibleInReflections = VisMeshFrag.VisibleInReflections;
	SceneProxyDesc->bVisibleInRayTracing = VisMeshFrag.VisibleInRayTracing;
	SceneProxyDesc->bVisibleInReflectionCaptures = VisMeshFrag.VisibleInReflectionCaptures;
	if (VisMeshFrag.NeverDistanceCull)
	{
		SceneProxyDesc->CachedMaxDrawDistance = 0.0f;
	}

	// Editor-picking requires per-instance hit proxies and selectability on the proxy descriptor.
	// These persist on the desc across all proxy recreations, so setting them once here is sufficient.
	SceneProxyDesc->bHasPerInstanceHitProxies = true;
	SceneProxyDesc->bSelectable = true;

	SkinnedISMFrag.SceneProxyDesc = SceneProxyDesc;

	// Create the render state helper. Canonical access elsewhere: FArcRenderStateFragment::
	// GetRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>().
	FArcRenderStateFragment& RenderState = EntityView.GetFragmentData<FArcRenderStateFragment>();
	RenderState.CreateRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>(
		HolderEntity, &EntityManager, PrimFrag, OverrideMats);

	Bucket.Add(Key, HolderEntity);

	// Initialize empty authoritative maps for this new holder.
	HolderInstanceKeys.Add(HolderEntity);
	HolderEntityToDenseIndex.Add(HolderEntity);

	return HolderEntity;
}

int32 FArcSkinnedMeshHolderRegistry::AddInstance(
	FMassEntityHandle Holder,
	FMassEntityHandle Source,
	int32 SubIndex,
	const FTransform& WorldTransform,
	uint32 AnimationIndex,
	FMassEntityManager& EntityManager,
	bool bDeferBoundsAndCullingRefresh,
	const FArcTypeElementInstanceId* InstanceId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisAddSkinnedISMInstance);

	FArcMassRenderISMSkinnedFragment* SkinnedFrag = EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(Holder);
	FArcRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FArcRenderPrimitiveFragment>(Holder);
	FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(Holder);

	if (!SkinnedFrag || !PrimFrag || !RenderState)
	{
		return INDEX_NONE;
	}

	FArcSkinnedMeshInstanceData InstanceData;
	InstanceData.Transform = FTransform3f(WorldTransform);
	InstanceData.AnimationIndex = AnimationIndex;

	// Dense append — PerInstanceData is a TArray (swap-removed on RemoveInstance to stay hole-free),
	// so TArray::Add returns the dense index that is the authoritative instance ordinal.
	const int32 Index = SkinnedFrag->PerInstanceData.Add(InstanceData);

	// Append the matching per-instance custom-data block (NumCustomDataFloats floats) so the flat
	// stride-NumCustomDataFloats buffer stays parallel to PerInstanceData. Zero-filled — callers that
	// drive custom data write it after the add. No-op when NumCustomDataFloats == 0.
	if (SkinnedFrag->NumCustomDataFloats > 0)
	{
		SkinnedFrag->PerInstanceCustomData.AddZeroed(SkinnedFrag->NumCustomDataFloats);
	}

#if WITH_EDITOR
	// Editor-only: append a typed-element hit proxy at the same dense index in PerInstanceHitProxy so
	// the instance is individually pickable in the viewport. PerInstanceHitProxy stays parallel to
	// PerInstanceData (same dense index, swap-removed in lockstep). The runtime subsystem never
	// populates this array so it stays empty for PIE/game — no runtime behavior change.
	if (GIsEditor && InstanceId && InstanceId->IsSet())
	{
		SkinnedFrag->PerInstanceHitProxy.Add(TRefCountPtr<HArcTypeElementHandleHitProxy>(new HArcTypeElementHandleHitProxy(*InstanceId)));
	}
#endif // WITH_EDITOR

	// Update authoritative book-keeping maps synchronously.
	TArray<FArcHolderInstanceKey>* InstanceKeys = HolderInstanceKeys.Find(Holder);
	TMap<FArcHolderInstanceKey, int32>* EntityToDense = HolderEntityToDenseIndex.Find(Holder);
	if (InstanceKeys && EntityToDense)
	{
		InstanceKeys->Add(FArcHolderInstanceKey{Source, SubIndex});
		EntityToDense->Add(FArcHolderInstanceKey{Source, SubIndex}, Index);
	}

	PrimFrag->bIsVisible = true;

	FArcMassInstancedSkinnedMeshRenderStateHelper& SkinnedHelper =
		RenderState->GetRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>();

	if (!bDeferBoundsAndCullingRefresh)
	{
		// Bounds first: BuildSceneDesc inside CreateRenderState reads them, and this branch has no
		// later push to the scene.
		const FArcMassSkinnedMeshFragment& SkinnedMeshFrag =
			EntityManager.GetConstSharedFragmentDataChecked<FArcMassSkinnedMeshFragment>(Holder);
		ArcSkinnedMeshHolderRegistry_Private::RecalculateSkinnedBounds(*SkinnedFrag, SkinnedMeshFrag, *PrimFrag);

		// Immediate mode: the buffer is complete after every add, so a missing proxy can be created
		// right here.
		if (!SkinnedHelper.AppendInstanceIncremental(Index))
		{
			ArcSkinnedMeshHolderRegistry_Private::CreateRenderStateIfNeeded(SkinnedHelper);
		}
		else
		{
			SkinnedHelper.UpdateTransform();
		}
	}
	else
	{
		// Deferred mode: an already-live proxy is patched per instance, but a holder without one stays
		// proxy-less until RefreshHolderBounds — creating it here would seed the instance manager from a
		// buffer holding only the instances added so far.
		SkinnedHelper.AppendInstanceIncremental(Index);
	}

	return Index;
}

bool FArcSkinnedMeshHolderRegistry::RemoveInstance(
	FMassEntityHandle Holder,
	FMassEntityHandle Source,
	int32 SubIndex,
	FMassEntityManager& EntityManager,
	TFunction<void(FMassEntityManager&, FMassEntityHandle MovedSource, int32 NewSlot)> OnInstanceMoved)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisRemoveSkinnedISMInstance);

	if (!Holder.IsValid() || !EntityManager.IsEntityValid(Holder))
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] Skinned RemoveInstance: holder=[%d:%d] invalid/stale (source=[%d:%d]) — no-op",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber);
		return false;
	}

	FArcMassRenderISMSkinnedFragment* SkinnedFrag = EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(Holder);
	FArcRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FArcRenderPrimitiveFragment>(Holder);
	FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(Holder);

	if (!SkinnedFrag || !PrimFrag || !RenderState)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] Skinned RemoveInstance: holder=[%d:%d] source=[%d:%d] valid entity but missing fragments (Skinned=%d Prim=%d RS=%d)",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber,
			SkinnedFrag != nullptr, PrimFrag != nullptr, RenderState != nullptr);
		return false;
	}

	if (SkinnedFrag->PerInstanceData.Num() == 0)
	{
		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] Skinned RemoveInstance: holder=[%d:%d] source=[%d:%d] holder already emptied this batch — no-op",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber);
		return false;
	}

	TArray<FArcHolderInstanceKey>* InstanceKeys = HolderInstanceKeys.Find(Holder);
	TMap<FArcHolderInstanceKey, int32>* EntityToDense = HolderEntityToDenseIndex.Find(Holder);

	if (!InstanceKeys || !EntityToDense)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] Skinned RemoveInstance: holder=[%d:%d] source=[%d:%d] has %d instances but NO authoritative maps — bookkeeping desync",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber, SkinnedFrag->PerInstanceData.Num());
		return false;
	}

	const FArcHolderInstanceKey LookupKey{Source, SubIndex};
	const int32* IndexPtr = EntityToDense->Find(LookupKey);
	if (!IndexPtr)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] Skinned RemoveInstance: holder=[%d:%d] source=[%d:%d] sub=%d key not in dense map (holder has %d instances)",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber, SubIndex,
			SkinnedFrag->PerInstanceData.Num());
		return false;
	}
	const int32 Index = *IndexPtr;

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] Skinned RemoveInstance: holder=[%d:%d] source=[%d:%d] sub=%d dense=%d count=%d",
		Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber, SubIndex, Index,
		SkinnedFrag->PerInstanceData.Num());

	// Remove from reverse map before mutating PerInstanceData.
	EntityToDense->Remove(LookupKey);

	if (SkinnedFrag->PerInstanceData.Num() == 1)
	{
		// Last instance — destroy the holder entirely.
		InstanceKeys->Reset();

		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] Skinned RemoveInstance: holder=[%d:%d] LAST instance (source=[%d:%d]) — destroying render state + mesh object + deferring holder destroy",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber);

		RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);

		FArcMassInstancedSkinnedMeshRenderStateHelper& SkinnedHelper =
			RenderState->GetRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>();
		SkinnedHelper.DestroyMeshObject();

		EntityManager.Defer().DestroyEntity(Holder);

		HolderInstanceKeys.Remove(Holder);
		HolderEntityToDenseIndex.Remove(Holder);

		return true;
	}

	// -----------------------------------------------------------------------------------------------
	// Dense swap-with-tail. INVARIANT: PerInstanceData, PerInstanceCustomData (stride
	// NumCustomDataFloats), PerInstanceHitProxy, and HolderInstanceKeys are ALL parallel and must move
	// in lockstep so the three sources of truth agree:
	//   1. PerInstanceData[i]               — dense GPU instance data
	//   2. HolderInstanceKeys[Holder][i]    — dense-index -> {Source, SubIndex}
	//   3. HolderEntityToDenseIndex[...]    — {Source, SubIndex} -> dense-index (authoritative)
	// FArcVisISMInstanceFragment::InstanceIndex on the moved source entity is a CACHE that is patched
	// via the OnInstanceMoved deferred command below — off-by-one here is silent visual desync.
	// -----------------------------------------------------------------------------------------------
	const int32 LastIndex = SkinnedFrag->PerInstanceData.Num() - 1;
	const int32 Stride = SkinnedFrag->NumCustomDataFloats;

	if (Index != LastIndex)
	{
		// Move the tail instance into the freed slot for every parallel array.
		SkinnedFrag->PerInstanceData[Index] = SkinnedFrag->PerInstanceData[LastIndex];

		if (Stride > 0)
		{
			// Stride-aware slab copy: overwrite Index's NumCustomDataFloats-wide block with the tail's.
			FMemory::Memcpy(
				&SkinnedFrag->PerInstanceCustomData[Index * Stride],
				&SkinnedFrag->PerInstanceCustomData[LastIndex * Stride],
				Stride * sizeof(float));
		}

#if WITH_EDITOR
		if (SkinnedFrag->PerInstanceHitProxy.IsValidIndex(LastIndex) && SkinnedFrag->PerInstanceHitProxy.IsValidIndex(Index))
		{
			SkinnedFrag->PerInstanceHitProxy[Index] = SkinnedFrag->PerInstanceHitProxy[LastIndex];
		}
#endif // WITH_EDITOR

		// Mirror the swap in the authoritative bookkeeping: the tail key now lives at Index.
		const FArcHolderInstanceKey MovedKey = (*InstanceKeys)[LastIndex];
		(*InstanceKeys)[Index] = MovedKey;
		(*EntityToDense)[MovedKey] = Index;

		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] Skinned RemoveInstance: swap-with-tail moved source=[%d:%d] from dense=%d to dense=%d (cache patch deferred)",
			MovedKey.SourceEntity.Index, MovedKey.SourceEntity.SerialNumber, LastIndex, Index);

		// Patch the moved source entity's cache fragment (FArcVisISMInstanceFragment::InstanceIndex)
		// via the caller-supplied OnInstanceMoved. Captured BY VALUE (owning TFunction) and run inside
		// a deferred set-command AFTER RemoveInstance returns — a TFunctionRef would dangle. Composite
		// entities lacking the cache fragment make OnInstanceMoved a no-op for themselves — correct.
		if (OnInstanceMoved)
		{
			const FMassEntityHandle MovedSourceEntity = MovedKey.SourceEntity;
			const int32 NewIndexCopy = Index;
			EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(
				[MovedSourceEntity, NewIndexCopy, OnInstanceMovedFn = MoveTemp(OnInstanceMoved)](FMassEntityManager& EM) mutable
				{
					// The command may flush after the entity is gone (e.g. PIE teardown).
					if (!EM.IsEntityValid(MovedSourceEntity))
					{
						return;
					}
					OnInstanceMovedFn(EM, MovedSourceEntity, NewIndexCopy);
				});
		}
	}

	// Pop the tail slot (always the last index regardless of whether a swap happened).
	SkinnedFrag->PerInstanceData.RemoveAt(LastIndex);
	InstanceKeys->RemoveAt(LastIndex);

	if (Stride > 0)
	{
		SkinnedFrag->PerInstanceCustomData.RemoveAt(LastIndex * Stride, Stride);
	}

#if WITH_EDITOR
	if (SkinnedFrag->PerInstanceHitProxy.IsValidIndex(LastIndex))
	{
		SkinnedFrag->PerInstanceHitProxy.RemoveAt(LastIndex);
	}
#endif // WITH_EDITOR

	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag =
		EntityManager.GetConstSharedFragmentDataChecked<FArcMassSkinnedMeshFragment>(Holder);
	ArcSkinnedMeshHolderRegistry_Private::RecalculateSkinnedBounds(*SkinnedFrag, SkinnedMeshFrag, *PrimFrag);

	// Incremental manager remove (RemoveAtSwap mirrors the dense swap above). Falls back to a full
	// MarkRenderStateDirty only when no live proxy exists.
	FArcMassInstancedSkinnedMeshRenderStateHelper& SkinnedHelper =
		RenderState->GetRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>();
	if (!SkinnedHelper.RemoveInstanceIncremental(Index))
	{
		RenderState->GetRenderStateHelper().MarkRenderStateDirty();
	}

	return false;
}

void FArcSkinnedMeshHolderRegistry::ClearHolder(FMassEntityHandle Holder, FMassEntityManager& EntityManager)
{
	if (!Holder.IsValid() || !EntityManager.IsEntityValid(Holder))
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] Skinned ClearHolder: holder=[%d:%d] invalid/stale — no-op",
			Holder.Index, Holder.SerialNumber);
		return;
	}

	UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] Skinned ClearHolder: holder=[%d:%d] — destroying render state + mesh object + deferring holder destroy",
		Holder.Index, Holder.SerialNumber);

	FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(Holder);
	if (RenderState)
	{
		RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);

		// Cast to the skinned helper and destroy the FSkeletalMeshObject to avoid leaking GPU resources.
		FArcMassInstancedSkinnedMeshRenderStateHelper& SkinnedHelper =
			RenderState->GetRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>();
		SkinnedHelper.DestroyMeshObject();
	}

	EntityManager.Defer().DestroyEntity(Holder);

	HolderInstanceKeys.Remove(Holder);
	HolderEntityToDenseIndex.Remove(Holder);
}

void FArcSkinnedMeshHolderRegistry::RefreshHolderBounds(FMassEntityHandle Holder, FMassEntityManager& EntityManager)
{
	if (!EntityManager.IsEntityValid(Holder))
	{
		return;
	}

	FArcMassRenderISMSkinnedFragment* SkinnedFrag = EntityManager.GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(Holder);
	FArcRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FArcRenderPrimitiveFragment>(Holder);
	FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(Holder);

	if (!SkinnedFrag || !PrimFrag || !RenderState)
	{
		return;
	}

	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag =
		EntityManager.GetConstSharedFragmentDataChecked<FArcMassSkinnedMeshFragment>(Holder);
	ArcSkinnedMeshHolderRegistry_Private::RecalculateSkinnedBounds(*SkinnedFrag, SkinnedMeshFrag, *PrimFrag);

	FArcMassInstancedSkinnedMeshRenderStateHelper& SkinnedHelper =
		RenderState->GetRenderStateHelper<FArcMassInstancedSkinnedMeshRenderStateHelper>();

	// End of a deferred batch — the buffer is fully populated only now, and CreateRenderState seeds the
	// instance manager from it (ShouldCreateRenderState already requires a non-empty buffer). Idempotent
	// for holders that already own a proxy, and it must run after the bounds recalc above because
	// BuildSceneDesc reads them.
	ArcSkinnedMeshHolderRegistry_Private::CreateRenderStateIfNeeded(SkinnedHelper);

	// UpdateTransform pushes through the SceneDesc that only BuildSceneDesc (inside CreateRenderState)
	// fills in, so it needs a live proxy.
	if (SkinnedHelper.GetSceneProxy() != nullptr)
	{
		SkinnedHelper.UpdateTransform();
	}
}
