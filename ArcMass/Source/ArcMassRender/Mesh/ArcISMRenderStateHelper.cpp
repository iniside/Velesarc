// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcISMRenderStateHelper.h"

#include "ArcMassRenderLogs.h"

#include "InstanceData/InstanceDataHelpers.h"
#include "InstanceData/InstanceUpdateChangeSet.h"
#include "InstanceDataSceneProxy.h"
#include "InstancedStaticMeshComponentHelper.h"
#include "InstancedStaticMesh/ISMInstanceUpdateChangeSet.h"
#include "Mass/EntityManager.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "NaniteSceneProxy.h"
#include "SceneInterface.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

FArcISMRenderStateHelper::FArcISMRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager, const FArcRenderPrimitiveFragment& RenderPrimitiveFragment, const FArcOverrideMaterialsFragment* OverrideMaterialsFragment, const FArcRenderISMFragment& RenderStaticMeshFragment)
	: Super(InEntityHandle, InEntityManager, RenderPrimitiveFragment, OverrideMaterialsFragment)
{
}

// ---------------------------------------------------------------------------
// Scene-proxy desc overrides
// ---------------------------------------------------------------------------

void FArcISMRenderStateHelper::ResetSceneProxyDescUnsupportedProperties()
{
	Super::ResetSceneProxyDescUnsupportedProperties();

	FArcRenderISMFragment& RenderISMFragment = GetMutableRenderISMFragment();

	checkf(RenderISMFragment.InstancedStaticMeshSceneProxyDesc, TEXT("Expecting a valid scene proxy desc"));
	// C4: re-assign the manager proxy so a reset does not orphan the data proxy.
	RenderISMFragment.InstancedStaticMeshSceneProxyDesc->InstanceDataSceneProxy = InstanceDataManager.GetOrCreateProxy(GetScene()->GetFeatureLevel());
#if WITH_EDITOR
	RenderISMFragment.InstancedStaticMeshSceneProxyDesc->bHasSelectedInstances = false;
#endif// WITH_EDITOR
}

// ---------------------------------------------------------------------------
// MakeComponentDesc  (§2 + C3 + C5)
// ---------------------------------------------------------------------------

namespace ArcISMRenderStateHelperPrivate
{
	// Fill the change set from PerInstanceSMData — called by FlushChanges on the game thread.
	// All captures by value so the lambda is self-contained when FlushChanges enqueues the RT task.
	static TFunction<void(FISMInstanceUpdateChangeSet&)> MakeBuildChangeSetCallback(
		TArray<FMatrix>           InTransforms,
		TArray<float>             InCustomData,
		int32                     InNumCustomDataFloats,
		bool                      bHasCustomData,
		bool                      bHasRandom,
		int32                     InInstancingRandomSeed,
		TArray<FInstancedStaticMeshRandomSeed> InAdditionalRandomSeeds,
		FRenderBounds             InMeshBounds,
		bool                      bInHasEditorData,
		TArray<TRefCountPtr<HHitProxy>> InHitProxies)
	{
		return [
			Transforms             = MoveTemp(InTransforms),
			CustomData             = MoveTemp(InCustomData),
			NumCustomDataFloats    = InNumCustomDataFloats,
			bHasCustomData,
			bHasRandom,
			InstancingRandomSeed   = InInstancingRandomSeed,
			AdditionalRandomSeeds  = MoveTemp(InAdditionalRandomSeeds),
			MeshBounds             = InMeshBounds,
			bHasEditorData         = bInHasEditorData,
			HitProxies             = MoveTemp(InHitProxies)
		](FISMInstanceUpdateChangeSet& ChangeSet) mutable
		{
			// Transforms (always required)
			ChangeSet.SetInstanceTransforms(MakeStridedView(Transforms));

			// Shared mesh-local bounds (no per-instance local bounds on this path)
			ChangeSet.SetSharedLocalBounds(MeshBounds);

			// Custom data
			if (bHasCustomData && NumCustomDataFloats > 0 && !CustomData.IsEmpty())
			{
				ChangeSet.SetCustomData(TArrayView<const float>(CustomData), NumCustomDataFloats);
			}

			// Per-instance random IDs — generated into a local array; the setter copies
			if (bHasRandom)
			{
				ChangeSet.GeneratePerInstanceRandomIds = [InstancingRandomSeed, AdditionalRandomSeeds](TArray<float>& OutRandomIDs) mutable
				{
					FInstanceDataHelpers::GenerateInstanceRandomIDs(InstancingRandomSeed, AdditionalRandomSeeds, OutRandomIDs);
				};
			}

#if WITH_EDITOR
			if (bHasEditorData && !HitProxies.IsEmpty())
			{
				// FInstanceUpdateChangeSet::SetEditorData is Engine-internal (no ENGINE_API), so we replicate
				// its body here using the exported MakeOpaqueHitProxyContainer plus the public change-set members.
				ChangeSet.HitProxyContainer = MakeOpaqueHitProxyContainer(HitProxies);
				ChangeSet.InstanceEditorData.Reset(HitProxies.Num());
				for (int32 ProxyIndex = 0; ProxyIndex < HitProxies.Num(); ++ProxyIndex)
				{
					FColor HitProxyColor(ForceInit);
					if (HitProxies[ProxyIndex].IsValid())
					{
						HitProxyColor = HitProxies[ProxyIndex]->Id.GetColor();
					}
					ChangeSet.InstanceEditorData.Add(FInstanceEditorData::Pack(HitProxyColor, /*bSelected*/false));
				}
				ChangeSet.SelectedInstances.Init(false, HitProxies.Num());
			}
#endif // WITH_EDITOR
		};
	}
} // namespace ArcISMRenderStateHelperPrivate

FInstanceUpdateComponentDesc FArcISMRenderStateHelper::MakeComponentDesc() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMMakeComponentDesc);

	const FArcRenderISMFragment& RenderISMFragment = GetRenderISMFragment();

	// C3: count comes from the manager, NOT PerInstanceSMData.Num().
	const int32 NumInstances = InstanceDataManager.GetMaxInstanceIndex();

	const FPrimitiveMaterialPropertyDescriptor PrimitiveMaterialDesc = GetUsedMaterialPropertyDesc(GetScene()->GetShaderPlatform());

	const int32 RawCustomDataFloats = (RenderISMFragment.PerInstanceSMCustomData.Num() > 0 && RenderISMFragment.PerInstanceSMData.Num() > 0)
		? RenderISMFragment.PerInstanceSMCustomData.Num() / RenderISMFragment.PerInstanceSMData.Num()
		: 0;

	const bool bHasCustomData = PrimitiveMaterialDesc.bAnyMaterialHasPerInstanceCustomData && RawCustomDataFloats > 0;
	const bool bHasRandom     = PrimitiveMaterialDesc.bAnyMaterialHasPerInstanceRandom && NumInstances > 0;

	// Collect transforms into a dense array (TSparseArray is not guaranteed contiguous)
	TArray<FMatrix> Transforms;
	Transforms.Reserve(NumInstances);
	for (const FInstancedStaticMeshInstanceData& SM : RenderISMFragment.PerInstanceSMData)
	{
		Transforms.Add(SM.Transform);
	}

#if WITH_EDITOR
	const bool bHasEditorData = GIsEditor && RenderISMFragment.PerInstanceHitProxy.Num() > 0;
	TArray<TRefCountPtr<HHitProxy>> HitProxies;
	if (bHasEditorData)
	{
		HitProxies.Reserve(RenderISMFragment.PerInstanceHitProxy.Num());
		for (const TRefCountPtr<HArcTypeElementHandleHitProxy>& HitProxy : RenderISMFragment.PerInstanceHitProxy)
		{
			// Upcast to the base HHitProxy ref type (no implicit TRefCountPtr<Derived>->TRefCountPtr<Base> via Add).
			HitProxies.Add(TRefCountPtr<HHitProxy>(HitProxy.GetReference()));
		}
	}
#else
	const bool bHasEditorData = false;
	TArray<TRefCountPtr<HHitProxy>> HitProxies;
#endif // WITH_EDITOR

	FInstanceDataFlags Flags;
	// C5 hard requirement: never set bHasPerInstanceDynamicData — null-component motion-vector deref crashes
	Flags.bHasPerInstanceDynamicData  = false;
	Flags.bHasPerInstanceCustomData   = bHasCustomData;
	Flags.bHasPerInstanceRandom       = bHasRandom;
	Flags.bHasPerInstanceLocalBounds  = false; // shared bounds path
	Flags.bHasPerInstanceEditorData   = bHasEditorData;

	FInstanceUpdateComponentDesc Desc;
	Desc.NumSourceInstances    = NumInstances;
	Desc.NumProxyInstances     = NumInstances;
	Desc.NumCustomDataFloats   = bHasCustomData ? RawCustomDataFloats : 0;
	Desc.ComponentMobility     = EComponentMobility::Movable;
	Desc.PrimitiveLocalToWorld = GetRenderMatrix();
	Desc.StaticMeshBounds      = GetStaticMesh()->GetBounds();
	Desc.Flags                 = Flags;
	Desc.PrimitiveMaterialDesc = PrimitiveMaterialDesc;

	Desc.BuildChangeSet = ArcISMRenderStateHelperPrivate::MakeBuildChangeSetCallback(
		MoveTemp(Transforms),
		bHasCustomData ? RenderISMFragment.PerInstanceSMCustomData : TArray<float>{},
		bHasCustomData ? RawCustomDataFloats : 0,
		bHasCustomData,
		bHasRandom,
		RenderISMFragment.InstancingRandomSeed,
		RenderISMFragment.AdditionalRandomSeeds,
		GetStaticMesh()->GetBounds(),
		bHasEditorData,
		MoveTemp(HitProxies)
	);

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM MakeComponentDesc: numInstances=%d transforms=%d buffer=%d customData=%d random=%d editorData=%d",
		NumInstances, Desc.NumProxyInstances, RenderISMFragment.PerInstanceSMData.Num(),
		bHasCustomData, bHasRandom, bHasEditorData);

	return Desc;
}

// ---------------------------------------------------------------------------
// CreateStaticMeshSceneProxy  (§2 + §3 + C4)
// ---------------------------------------------------------------------------

FPrimitiveSceneProxy* FArcISMRenderStateHelper::CreateStaticMeshSceneProxy(const Nanite::FMaterialAudit& NaniteMaterials, bool bCreateNanite)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMCreateSceneProxy);

	FArcRenderISMFragment& RenderISMFragment = GetMutableRenderISMFragment();

	check(GetWorld());
	check(RenderISMFragment.InstancedStaticMeshSceneProxyDesc);
	check(RenderISMFragment.InstancedStaticMeshSceneProxyDesc->Scene);
	check(RenderISMFragment.PerInstanceSMData.Num() > 0);

	// C4: (re)assign the data proxy at the start so a prior ResetSceneProxyDescUnsupportedProperties
	// null is repaired before any data is written.
	RenderISMFragment.InstancedStaticMeshSceneProxyDesc->InstanceDataSceneProxy =
		InstanceDataManager.GetOrCreateProxy(GetScene()->GetFeatureLevel());

	// Reset id tracking so the tail-append seed below holds its invariant on rebuild.
	// InstanceDataManager is a persistent member and DestroyRenderState (base class) never
	// resets it, so a prior build leaves NumInstances != 0 and Add(0, bInsert=false) would
	// fail check(InInstanceAddAtIndex == GetMaxInstanceIndex()). No-op on a fresh manager.
	InstanceDataManager.ClearIdTracking(0);

	// §2 step 1: seed the manager with one entry per current instance (tail-append → identity id map).
	{
		int32 Index = 0;
		for (const FInstancedStaticMeshInstanceData& SM : RenderISMFragment.PerInstanceSMData)
		{
			InstanceDataManager.Add(Index, /*bInsert=*/false);
			++Index;
		}
	}

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM CreateProxy: seeded %d instances, manager reports %d, nanite=%d",
		RenderISMFragment.PerInstanceSMData.Num(), InstanceDataManager.GetMaxInstanceIndex(), bCreateNanite);

	// §2 step 3: full Build — Initial state ⇒ bNeedFullUpdate is true inside FlushChanges.
	InstanceDataManager.FlushChanges(MakeComponentDesc(), /*bNewPrimitiveProxy=*/true);

	// Construct the primitive proxy; it reads the manager-owned data proxy.
	if (bCreateNanite)
	{
		PrimitiveSceneData.SceneProxy = ::new Nanite::FSceneProxy(NaniteMaterials, *RenderISMFragment.InstancedStaticMeshSceneProxyDesc);
	}
	else
	{
		PrimitiveSceneData.SceneProxy = ::new FInstancedStaticMeshSceneProxy(*RenderISMFragment.InstancedStaticMeshSceneProxyDesc, RenderISMFragment.InstancedStaticMeshSceneProxyDesc->FeatureLevel);
	}
	return PrimitiveSceneData.SceneProxy;
}

// ---------------------------------------------------------------------------
// Incremental add / remove  (§3)
// ---------------------------------------------------------------------------

bool FArcISMRenderStateHelper::AddInstanceIncremental(int32 DenseIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMAddInstanceIncremental);

	if (GetSceneProxy() == nullptr)
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM AddInstanceIncremental: dense=%d NO PROXY — buffered until proxy creation", DenseIndex);
		return false;
	}

	// Mutate manager first (C3: MakeComponentDesc reads count after mutation).
	InstanceDataManager.Add(DenseIndex, /*bInsert=*/false);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMFlushChanges);
		InstanceDataManager.FlushChanges(MakeComponentDesc(), /*bNewPrimitiveProxy=*/false);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMUpdatePrimitiveInstances);
		GetScene()->UpdatePrimitiveInstances(&SceneDesc);
	}

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM AddInstanceIncremental: dense=%d manager=%d buffer=%d proxy=%p sceneDescProxy=%p",
		DenseIndex, InstanceDataManager.GetMaxInstanceIndex(), GetRenderISMFragment().PerInstanceSMData.Num(),
		GetSceneProxy(), SceneDesc.PrimitiveSceneData ? SceneDesc.PrimitiveSceneData->SceneProxy : nullptr);

	return true;
}

bool FArcISMRenderStateHelper::RemoveInstanceIncremental(int32 DenseIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMRemoveInstanceIncremental);

	if (GetSceneProxy() == nullptr)
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] ISM RemoveInstanceIncremental: dense=%d NO PROXY — no-op", DenseIndex);
		return false;
	}

	// Mutate manager first (C3: MakeComponentDesc reads count after mutation).
	InstanceDataManager.RemoveAtSwap(DenseIndex);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMFlushChanges);
		InstanceDataManager.FlushChanges(MakeComponentDesc(), /*bNewPrimitiveProxy=*/false);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMUpdatePrimitiveInstances);
		GetScene()->UpdatePrimitiveInstances(&SceneDesc);
	}

	return true;
}

bool FArcISMRenderStateHelper::UpdateInstanceTransformIncremental(int32 DenseIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMUpdateInstanceTransformIncremental);

	if (GetSceneProxy() == nullptr)
	{
		return false;
	}

	InstanceDataManager.TransformChanged(DenseIndex);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMFlushChanges);
		InstanceDataManager.FlushChanges(MakeComponentDesc(), /*bNewPrimitiveProxy=*/false);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ArcISMUpdatePrimitiveInstances);
		GetScene()->UpdatePrimitiveInstances(&SceneDesc);
	}

	return true;
}

// ---------------------------------------------------------------------------
// PSO precache
// ---------------------------------------------------------------------------

#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
void FArcISMRenderStateHelper::CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams)
{
	LODData.Reset();
	FInstancedStaticMeshComponentHelper::CollectPSOPrecacheData(*this, BasePrecachePSOParams, OutParams);
}
#endif

// ---------------------------------------------------------------------------
// Fragment accessors
// ---------------------------------------------------------------------------

const FArcRenderISMFragment& FArcISMRenderStateHelper::GetRenderISMFragment() const
{
	return GetEntityManager().GetFragmentDataChecked<FArcRenderISMFragment>(EntityHandle);
}

FArcRenderISMFragment& FArcISMRenderStateHelper::GetMutableRenderISMFragment()
{
	return GetEntityManager().GetFragmentDataChecked<FArcRenderISMFragment>(EntityHandle);
}

// ---------------------------------------------------------------------------
// Scene-proxy desc dynamic properties
// ---------------------------------------------------------------------------

void FArcISMRenderStateHelper::InitializeSceneProxyDescDynamicProperties()
{
	FArcBaseStaticMeshRenderStateHelper::InitializeSceneProxyDescDynamicProperties();

#if WITH_EDITOR
	FStaticMeshSceneProxyDesc& SceneProxyDesc = GetStaticMeshSceneProxyDesc();

	FArcRenderISMFragment& RenderISMFragment = GetMutableRenderISMFragment();
	SceneProxyDesc.bHasPerInstanceHitProxies = RenderISMFragment.PerInstanceHitProxy.Num() > 0;
#endif // WITH_EDITOR
}

FPrimitiveSceneProxyDesc& FArcISMRenderStateHelper::GetSceneProxyDesc()
{
	return GetStaticMeshSceneProxyDesc();
}

const FPrimitiveSceneProxyDesc& FArcISMRenderStateHelper::GetSceneProxyDesc() const
{
	return GetStaticMeshSceneProxyDesc();
}

FStaticMeshSceneProxyDesc& FArcISMRenderStateHelper::GetStaticMeshSceneProxyDesc()
{
	FArcRenderISMFragment& RenderISMFragment = GetMutableRenderISMFragment();
	checkf(RenderISMFragment.InstancedStaticMeshSceneProxyDesc, TEXT("Expecting a valid scene proxy desc"));
	return *RenderISMFragment.InstancedStaticMeshSceneProxyDesc;
}

const FStaticMeshSceneProxyDesc& FArcISMRenderStateHelper::GetStaticMeshSceneProxyDesc() const
{
	const FArcRenderISMFragment& RenderISMFragment = GetRenderISMFragment();
	checkf(RenderISMFragment.InstancedStaticMeshSceneProxyDesc, TEXT("Expecting a valid scene proxy desc"));
	return *RenderISMFragment.InstancedStaticMeshSceneProxyDesc;
}
