// Copyright Lukasz Baran. All Rights Reserved.

#include "Mesh/ArcISMHolderRegistry.h"

#include "ArcMassRenderLogs.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "Mass/EntityManager.h"
#include "Mass/CommandBuffer.h"
#include "Mass/EntityView.h"
#include "Mass/Commands.h"
#include "Mass/EntityFragments.h"
#include "Mesh/ArcRenderStateHelper.h"
#include "Mesh/ArcISMRenderStateHelper.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "Mesh/ArcRenderMeshUtils.h"

// ---------------------------------------------------------------------------
// Internal helpers (file-scoped)
// ---------------------------------------------------------------------------

namespace ArcISMHolderRegistry_Private
{
	/**
	 * Create the GPU scene proxy for a holder whose helper has no live proxy yet.
	 *
	 * MarkRenderStateDirty() is guarded on IsRenderStateCreated()/IsRenderStateDelayed(), so it
	 * silently no-ops before the first proxy exists — creation must be driven directly.
	 */
	static void CreateRenderStateIfNeeded(FArcRenderStateHelper& Helper)
	{
		if (Helper.IsRenderStateCreated() || !Helper.ShouldCreateRenderState())
		{
			UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM CreateRenderStateIfNeeded: SKIPPED — alreadyCreated=%d shouldCreate=%d",
				Helper.IsRenderStateCreated(), Helper.ShouldCreateRenderState());
			return;
		}

		FRegisterComponentContext BatchingContext(nullptr);
		BatchingContext.Initialize(Helper.GetWorld());
		Helper.CreateRenderState(&BatchingContext);
		BatchingContext.Process();
	}
} // namespace ArcISMHolderRegistry_Private

// ---------------------------------------------------------------------------
// FArcISMHolderRegistry
// ---------------------------------------------------------------------------

void FArcISMHolderRegistry::InitializeArchetypes(FMassEntityManager& EntityManager)
{
	if (bArchetypesInitialized)
	{
		return;
	}

	FMassElementBitSet Composition;
	Composition.Add<FTransformFragment>();
	Composition.Add<FArcRenderStateFragment>();
	Composition.Add<FArcRenderPrimitiveFragment>();
	Composition.Add<FArcRenderISMFragment>();
	ISMHolderArchetype = EntityManager.CreateArchetype(Composition, FMassArchetypeCreationParams{TEXT("ArcVisISMHolder")});

	FMassElementBitSet CompositionWithMats = Composition;
	CompositionWithMats.Add<FArcOverrideMaterialsFragment>();
	ISMHolderArchetypeWithMats = EntityManager.CreateArchetype(CompositionWithMats, FMassArchetypeCreationParams{TEXT("ArcVisISMHolderMats")});

	bArchetypesInitialized = true;
}

FMassEntityHandle FArcISMHolderRegistry::FindOrCreateHolder(
	TMap<FArcISMHolderKey, FMassEntityHandle>& Bucket,
	const FArcISMHolderKey& Key,
	const FArcStaticMeshFragment& StaticMeshFrag,
	const FArcVisualizationMeshFragment& VisMeshFrag,
	const FArcOverrideMaterialsFragment* OverrideMats,
	FMassEntityManager& EntityManager)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisFindOrCreateISMHolder);

	InitializeArchetypes(EntityManager);

	FMassEntityHandle* ExistingHolder = Bucket.Find(Key);
	if (ExistingHolder && ExistingHolder->IsValid() && EntityManager.IsEntityValid(*ExistingHolder))
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM FindOrCreateHolder: REUSED holder=[%d:%d] (bucket has %d keys)",
			ExistingHolder->Index, ExistingHolder->SerialNumber, Bucket.Num());
		return *ExistingHolder;
	}

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM FindOrCreateHolder: CREATING new holder — staleEntry=%d mats=%d bucket=%d",
		ExistingHolder != nullptr, OverrideMats != nullptr, Bucket.Num());

	// Create new holder entity.
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

	// Initialize transform (identity — instances carry own world transforms).
	FTransformFragment& TransformFrag = EntityView.GetFragmentData<FTransformFragment>();
	TransformFrag.SetTransform(FTransform::Identity);

	// Initialize ISM scene proxy desc.
	FArcRenderISMFragment& ISMFrag = EntityView.GetFragmentData<FArcRenderISMFragment>();
	FArcRenderPrimitiveFragment& PrimFrag = EntityView.GetFragmentData<FArcRenderPrimitiveFragment>();
	PrimFrag.bIsVisible = true;

	checkf(ISMFrag.InstancedStaticMeshSceneProxyDesc, TEXT("Default ctor should create desc"));
	UE::ArcMassRender::Mesh::InitializeInstanceStaticMeshSceneProxyDescFromFragment(
		StaticMeshFrag, VisMeshFrag, TransformFrag, *ISMFrag.InstancedStaticMeshSceneProxyDesc);

	// Create render state helper.
	FArcRenderStateFragment& RenderState = EntityView.GetFragmentData<FArcRenderStateFragment>();
	RenderState.CreateRenderStateHelper<FArcISMRenderStateHelper>(
		HolderEntity, &EntityManager, PrimFrag, OverrideMats, ISMFrag);

	Bucket.Add(Key, HolderEntity);

	// Create the authoritative dense-index maps for this new holder (empty at creation).
	HolderInstanceKeys.Add(HolderEntity);
	HolderEntityToDenseIndex.Add(HolderEntity);

	return HolderEntity;
}

int32 FArcISMHolderRegistry::AddInstance(
	FMassEntityHandle Holder,
	FMassEntityHandle Source,
	int32 SubIndex,
	const FTransform& WorldTransform,
	FMassEntityManager& EntityManager,
	bool bDeferBoundsAndCullingRefresh,
	const FArcTypeElementInstanceId* InstanceId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisAddISMInstance);

	FArcRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FArcRenderISMFragment>(Holder);
	FArcRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FArcRenderPrimitiveFragment>(Holder);
	FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(Holder);

	if (!ISMFrag || !PrimFrag || !RenderState)
	{
		return INDEX_NONE;
	}

	// Append to dense PerInstanceSMData — invariant: no holes (swap-with-tail on remove keeps it dense).
	FInstancedStaticMeshInstanceData InstanceData;
	InstanceData.Transform = WorldTransform.ToMatrixWithScale();
	// TSparseArray::Add returns the sparse index.  Because we never RemoveAt a middle slot
	// (only ever remove the current tail via swap-with-tail), dense ordinal == sparse index invariantly.
	const int32 DenseIndex = ISMFrag->PerInstanceSMData.Add(InstanceData);

#if WITH_EDITOR
	// Editor-only: mirror the SM data slot with a typed-element hit proxy so the instance is
	// individually pickable in the viewport. PerInstanceHitProxy stays parallel to PerInstanceSMData
	// (same dense index); runtime visualization callers pass no id and leave it empty. The proxy
	// stores the lightweight id (never a refcounted handle — see FArcTypeElementInstanceId).
	if (GIsEditor && InstanceId && InstanceId->IsSet())
	{
		ISMFrag->PerInstanceHitProxy.Insert(DenseIndex, TRefCountPtr<HArcTypeElementHandleHitProxy>(new HArcTypeElementHandleHitProxy(*InstanceId)));
	}
#endif // WITH_EDITOR

	// Update authoritative maps synchronously.
	TArray<FArcHolderInstanceKey>* InstanceKeys = HolderInstanceKeys.Find(Holder);
	TMap<FArcHolderInstanceKey, int32>* EntityToDense = HolderEntityToDenseIndex.Find(Holder);
	if (InstanceKeys && EntityToDense)
	{
		InstanceKeys->Add(FArcHolderInstanceKey{Source, SubIndex});
		EntityToDense->Add(FArcHolderInstanceKey{Source, SubIndex}, DenseIndex);
	}
	else
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] ISM AddInstance: holder=[%d:%d] source=[%d:%d] has NO authoritative maps — instance added at dense=%d but untracked",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber, DenseIndex);
	}

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM AddInstance: holder=[%d:%d] source=[%d:%d] sub=%d dense=%d count=%d loc=(%.0f,%.0f,%.0f) scale=(%.2f,%.2f,%.2f)",
		Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber, SubIndex, DenseIndex,
		ISMFrag->PerInstanceSMData.Num(),
		WorldTransform.GetLocation().X, WorldTransform.GetLocation().Y, WorldTransform.GetLocation().Z,
		WorldTransform.GetScale3D().X, WorldTransform.GetScale3D().Y, WorldTransform.GetScale3D().Z);

	if (!bDeferBoundsAndCullingRefresh)
	{
		// Recalculate bounds.
		FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Holder);
		if (TransformFrag)
		{
			PrimFrag->LocalBounds = UE::ArcMassRender::Mesh::CalculateInstancedStaticMeshBounds(
				*TransformFrag, *ISMFrag, UE::ArcMassRender::Mesh::EBoundsType::LocalBounds);
			PrimFrag->WorldBounds = UE::ArcMassRender::Mesh::CalculateInstancedStaticMeshBounds(
				*TransformFrag, *ISMFrag, UE::ArcMassRender::Mesh::EBoundsType::WorldBounds);
		}

		PrimFrag->bIsVisible = true;

		// Immediate mode: the buffer is complete after every add, so a missing proxy can be
		// created right here.
		FArcISMRenderStateHelper& ISMHelper = RenderState->GetRenderStateHelper<FArcISMRenderStateHelper>();
		if (!ISMHelper.AddInstanceIncremental(DenseIndex))
		{
			ArcISMHolderRegistry_Private::CreateRenderStateIfNeeded(ISMHelper);
		}
		else if (TransformFrag)
		{
			// Proxy exists and was patched incrementally — refresh culling bounds.
			ISMHelper.UpdateTransform();
		}
	}
	else
	{
		// Deferred mode: an already-live proxy is patched per instance, but a holder without one
		// stays proxy-less until RefreshHolderBounds — creating it here would seed the instance
		// manager from a buffer holding only the instances added so far.
		PrimFrag->bIsVisible = true;

		FArcISMRenderStateHelper& ISMHelper = RenderState->GetRenderStateHelper<FArcISMRenderStateHelper>();
		ISMHelper.AddInstanceIncremental(DenseIndex);
	}

	return DenseIndex;
}

bool FArcISMHolderRegistry::RemoveInstance(
	FMassEntityHandle Holder,
	FMassEntityHandle Source,
	int32 SubIndex,
	FMassEntityManager& EntityManager,
	TFunction<void(FMassEntityManager&, FMassEntityHandle MovedSource, int32 NewIndex)> OnInstanceMoved)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisRemoveISMInstance);

	if (!Holder.IsValid() || !EntityManager.IsEntityValid(Holder))
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM RemoveInstance: holder=[%d:%d] invalid/stale (source=[%d:%d]) — no-op",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber);
		return false;
	}

	FArcRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FArcRenderISMFragment>(Holder);
	FArcRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FArcRenderPrimitiveFragment>(Holder);
	FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(Holder);

	if (!ISMFrag || !PrimFrag || !RenderState)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] ISM RemoveInstance: holder=[%d:%d] source=[%d:%d] valid entity but missing fragments (ISM=%d Prim=%d RS=%d)",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber,
			ISMFrag != nullptr, PrimFrag != nullptr, RenderState != nullptr);
		return false;
	}

	// Holder already emptied and queued for deferred destruction by a prior call in the same batch.
	if (ISMFrag->PerInstanceSMData.Num() == 0)
	{
		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] ISM RemoveInstance: holder=[%d:%d] source=[%d:%d] holder already emptied this batch — no-op",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber);
		return false;
	}

	// Resolve the live dense index from the authoritative map (never read the cached instance
	// fragment during removal — it may be stale or absent for composite entities).
	TArray<FArcHolderInstanceKey>* InstanceKeys = HolderInstanceKeys.Find(Holder);
	TMap<FArcHolderInstanceKey, int32>* EntityToDense = HolderEntityToDenseIndex.Find(Holder);

	if (!InstanceKeys || !EntityToDense)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] ISM RemoveInstance: holder=[%d:%d] source=[%d:%d] has %d instances but NO authoritative maps — bookkeeping desync",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber, ISMFrag->PerInstanceSMData.Num());
		return false;
	}

	const FArcHolderInstanceKey LookupKey{Source, SubIndex};
	const int32* DenseIndexPtr = EntityToDense->Find(LookupKey);
	if (!DenseIndexPtr)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] ISM RemoveInstance: holder=[%d:%d] source=[%d:%d] sub=%d key not in dense map (holder has %d instances)",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber, SubIndex,
			ISMFrag->PerInstanceSMData.Num());
		return false;
	}
	const int32 DenseIndex = *DenseIndexPtr;

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM RemoveInstance: holder=[%d:%d] source=[%d:%d] sub=%d dense=%d count=%d",
		Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber, SubIndex, DenseIndex,
		ISMFrag->PerInstanceSMData.Num());

	// Remove from authoritative maps before mutating PerInstanceSMData.
	EntityToDense->Remove(LookupKey);

	if (ISMFrag->PerInstanceSMData.Num() == 1)
	{
		// Last instance — destroy the holder entirely.
		InstanceKeys->Reset();

		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] ISM RemoveInstance: holder=[%d:%d] LAST instance (source=[%d:%d]) — destroying render state + deferring holder destroy",
			Holder.Index, Holder.SerialNumber, Source.Index, Source.SerialNumber);

		RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);
		EntityManager.Defer().DestroyEntity(Holder);

		// Clean up authoritative maps for this holder.
		HolderInstanceKeys.Remove(Holder);
		HolderEntityToDenseIndex.Remove(Holder);

		// PerInstanceSMData mutation is intentionally skipped — entity is being destroyed.
		// Cell-map eviction is the caller's responsibility.
		return true;
	}

	// Swap-with-tail — INVARIANT: never RemoveAt a middle slot.
	// This keeps PerInstanceSMData hole-free so dense ordinal == sparse index for all remaining slots.
	const int32 LastDenseIndex = ISMFrag->PerInstanceSMData.Num() - 1;

	if (DenseIndex != LastDenseIndex)
	{
		// Move the tail instance into the removed slot.
		ISMFrag->PerInstanceSMData[DenseIndex] = ISMFrag->PerInstanceSMData[LastDenseIndex];

#if WITH_EDITOR
		if (ISMFrag->PerInstanceHitProxy.IsValidIndex(LastDenseIndex) && ISMFrag->PerInstanceHitProxy.IsValidIndex(DenseIndex))
		{
			ISMFrag->PerInstanceHitProxy[DenseIndex] = ISMFrag->PerInstanceHitProxy[LastDenseIndex];
		}
#endif // WITH_EDITOR

		// Mirror the swap in the authoritative arrays.
		FArcHolderInstanceKey MovedKey = (*InstanceKeys)[LastDenseIndex];
		(*InstanceKeys)[DenseIndex] = MovedKey;
		(*EntityToDense)[MovedKey] = DenseIndex;

		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM RemoveInstance: swap-with-tail moved source=[%d:%d] from dense=%d to dense=%d (cache patch deferred)",
			MovedKey.SourceEntity.Index, MovedKey.SourceEntity.SerialNumber, LastDenseIndex, DenseIndex);

		// Update the swapped entity's cached fragment via deferred command.
		// This is a cache-only update — the authoritative index is already correct above.
		// We use Defer() because the deactivate processor may be iterating that archetype read-only
		// on this same frame; a direct write would alias.
		// OnInstanceMoved is captured BY VALUE (owning TFunction) — the patch runs inside a deferred
		// command AFTER RemoveInstance returns; a TFunctionRef would dangle.
		// Note: composite entities lack the cache fragment, so OnInstanceMoved no-ops for them — correct.
		FMassEntityHandle MovedSourceEntity = MovedKey.SourceEntity;
		const int32 NewDenseIndexCopy = DenseIndex;
		EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(
			[MovedSourceEntity, NewDenseIndexCopy, OnInstanceMovedFn = MoveTemp(OnInstanceMoved)](FMassEntityManager& EM) mutable
			{
				// The command may flush after the entity is gone (e.g. PIE teardown).
				if (!EM.IsEntityValid(MovedSourceEntity))
				{
					return;
				}
				OnInstanceMovedFn(EM, MovedSourceEntity, NewDenseIndexCopy);
			});
	}

	// Remove the tail slot (always the last index regardless of swap).
	ISMFrag->PerInstanceSMData.RemoveAt(LastDenseIndex);
	InstanceKeys->RemoveAt(LastDenseIndex);

#if WITH_EDITOR
	if (ISMFrag->PerInstanceHitProxy.IsValidIndex(LastDenseIndex))
	{
		ISMFrag->PerInstanceHitProxy.RemoveAt(LastDenseIndex);
	}
#endif // WITH_EDITOR

	// Recalculate bounds.
	FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Holder);
	if (TransformFrag)
	{
		PrimFrag->LocalBounds = UE::ArcMassRender::Mesh::CalculateInstancedStaticMeshBounds(
			*TransformFrag, *ISMFrag, UE::ArcMassRender::Mesh::EBoundsType::LocalBounds);
		PrimFrag->WorldBounds = UE::ArcMassRender::Mesh::CalculateInstancedStaticMeshBounds(
			*TransformFrag, *ISMFrag, UE::ArcMassRender::Mesh::EBoundsType::WorldBounds);
	}

	// Drive the incremental render-state helper.
	FArcISMRenderStateHelper& ISMHelper = RenderState->GetRenderStateHelper<FArcISMRenderStateHelper>();
	if (!ISMHelper.RemoveInstanceIncremental(DenseIndex))
	{
		ISMHelper.MarkRenderStateDirty();
	}
	else if (TransformFrag)
	{
		// Proxy was patched incrementally — refresh culling bounds.
		ISMHelper.UpdateTransform();
	}

	return false;
}

void FArcISMHolderRegistry::ClearHolder(FMassEntityHandle Holder, FMassEntityManager& EntityManager)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisClearISMHolder);

	if (!Holder.IsValid() || !EntityManager.IsEntityValid(Holder))
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM ClearHolder: holder=[%d:%d] invalid/stale — no-op",
			Holder.Index, Holder.SerialNumber);
		return;
	}

	UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] ISM ClearHolder: holder=[%d:%d] — destroying render state + deferring holder destroy",
		Holder.Index, Holder.SerialNumber);

	FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(Holder);
	if (RenderState)
	{
		RenderState->GetRenderStateHelper().DestroyRenderState(nullptr);
	}

	EntityManager.Defer().DestroyEntity(Holder);

	// Erase authoritative maps for this holder.
	HolderInstanceKeys.Remove(Holder);
	HolderEntityToDenseIndex.Remove(Holder);
}

void FArcISMHolderRegistry::RefreshHolderBounds(FMassEntityHandle Holder, FMassEntityManager& EntityManager)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisRefreshISMHolderBounds);

	if (!EntityManager.IsEntityValid(Holder))
	{
		return;
	}

	FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Holder);
	FArcRenderISMFragment* ISMFrag = EntityManager.GetFragmentDataPtr<FArcRenderISMFragment>(Holder);
	FArcRenderPrimitiveFragment* PrimFrag = EntityManager.GetFragmentDataPtr<FArcRenderPrimitiveFragment>(Holder);
	FArcRenderStateFragment* RenderState = EntityManager.GetFragmentDataPtr<FArcRenderStateFragment>(Holder);

	if (!TransformFrag || !ISMFrag || !PrimFrag || !RenderState)
	{
		return;
	}

	// The bounds math below assumes a holder at identity: instances carry component-relative
	// transforms that equal world transforms only under this invariant. The holder transform is
	// *set* to identity in FindOrCreateHolder and never mutated, so exact-identity Equals holds.
	checkSlow(TransformFrag->GetTransform().Equals(FTransform::Identity));

	PrimFrag->LocalBounds = UE::ArcMassRender::Mesh::CalculateInstancedStaticMeshBounds(
		*TransformFrag, *ISMFrag, UE::ArcMassRender::Mesh::EBoundsType::LocalBounds);
	PrimFrag->WorldBounds = UE::ArcMassRender::Mesh::CalculateInstancedStaticMeshBounds(
		*TransformFrag, *ISMFrag, UE::ArcMassRender::Mesh::EBoundsType::WorldBounds);

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM RefreshHolderBounds: holder=[%d:%d] count=%d visible=%d proxy=%p world origin=(%.0f,%.0f,%.0f) extent=(%.0f,%.0f,%.0f) local origin=(%.0f,%.0f,%.0f) extent=(%.0f,%.0f,%.0f)",
		Holder.Index, Holder.SerialNumber, ISMFrag->PerInstanceSMData.Num(), PrimFrag->bIsVisible,
		RenderState->GetRenderStateHelper<FArcISMRenderStateHelper>().GetSceneProxy(),
		PrimFrag->WorldBounds.Origin.X, PrimFrag->WorldBounds.Origin.Y, PrimFrag->WorldBounds.Origin.Z,
		PrimFrag->WorldBounds.BoxExtent.X, PrimFrag->WorldBounds.BoxExtent.Y, PrimFrag->WorldBounds.BoxExtent.Z,
		PrimFrag->LocalBounds.Origin.X, PrimFrag->LocalBounds.Origin.Y, PrimFrag->LocalBounds.Origin.Z,
		PrimFrag->LocalBounds.BoxExtent.X, PrimFrag->LocalBounds.BoxExtent.Y, PrimFrag->LocalBounds.BoxExtent.Z);

	FArcISMRenderStateHelper& ISMHelper = RenderState->GetRenderStateHelper<FArcISMRenderStateHelper>();

	// End of a deferred batch — the buffer is fully populated only now, and CreateStaticMeshSceneProxy
	// seeds the instance manager from it (and check()s it is non-empty).  Idempotent for holders that
	// already own a proxy.
	if (ISMFrag->PerInstanceSMData.Num() > 0)
	{
		ArcISMHolderRegistry_Private::CreateRenderStateIfNeeded(ISMHelper);
	}

	// UpdatePrimitiveTransform dereferences SceneDesc.PrimitiveSceneData, which only BuildSceneDesc
	// (inside CreateRenderState) fills in — a proxy-less holder would crash.
	if (ISMHelper.GetSceneProxy() != nullptr)
	{
		ISMHelper.UpdateTransform();
	}
}
