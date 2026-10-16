// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/SkinnedMeshVisualization/ArcMassInstancedSkinnedMeshRenderStateHelper.h"

#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMassRenderLogs.h"
#include "Mass/EntityManager.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "Animation/TransformProviderData.h"
#include "Engine/SkinnedAsset.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SceneInterface.h"
#include "SkeletalRenderPublic.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "InstanceDataSceneProxy.h"
#include "InstanceData/InstanceUpdateChangeSet.h"

namespace ArcSkinnedISMHelper_Private
{
	// Render updates become unsafe the moment the world starts tearing down: the FScene and its
	// FPrimitiveSceneProxy are being destroyed, and the manager's FInstanceDataSceneProxy drops to a
	// single ref (unattached), so FInstanceDataManager::FlushChanges dispatches its delta Update
	// IMMEDIATELY on a worker thread (not ordered behind a render command) — racing the RT as it frees
	// the instance buffers, which overruns InstanceToPrimitiveRelative inside
	// FUpdatableInstanceDataSceneProxy::Update. The crash fired on the incremental Remove issued right
	// after LogWorld "BeginTearingDown", before UArcEntityVisualizationSubsystem::PreDeinitialize
	// strips the proxies (diagnosed via temporary LogArcSkinnedISMDiag instrumentation, since removed).
	//
	// During teardown the holder/proxy are about to be destroyed wholesale, so skipping the incremental
	// GPU flush is harmless. This guard only changes teardown behaviour — steady-state play still takes
	// the incremental delta path, preserving the RT-BLAS-churn fix it exists for.
	static bool IsRenderTeardownInProgress(const UWorld* World)
	{
		return World == nullptr || World->bIsTearingDown || World->IsBeingCleanedUp();
	}
}

TArray<TObjectPtr<UMaterialInterface>> FArcMassInstancedSkinnedMeshRenderStateHelper::EmptyMaterialSlotOverlays;

FArcMassInstancedSkinnedMeshRenderStateHelper::FArcMassInstancedSkinnedMeshRenderStateHelper(
	FMassEntityHandle InEntityHandle,
	TNotNull<FMassEntityManager*> InEntityManager,
	const FArcRenderPrimitiveFragment& RenderPrimitiveFragment,
	const FArcOverrideMaterialsFragment* OverrideMaterialsFragment)
	: Super(InEntityHandle, InEntityManager, RenderPrimitiveFragment, OverrideMaterialsFragment)
{
}

FArcMassInstancedSkinnedMeshRenderStateHelper::~FArcMassInstancedSkinnedMeshRenderStateHelper()
{
	// MeshObject is intentionally NOT deleted here — it's preserved across proxy recreations.
	// Call DestroyMeshObject() explicitly when the holder entity is destroyed.
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::DestroyMeshObject()
{
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	if (ISMFrag.MeshObject)
	{
		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] Skinned DestroyMeshObject: helper=%p entity=[%d:%d] meshObject=%p — ReleaseResources + BeginCleanup",
			this, EntityHandle.Index, EntityHandle.SerialNumber, ISMFrag.MeshObject);
		ISMFrag.MeshObject->ReleaseResources();
		BeginCleanup(ISMFrag.MeshObject);
		ISMFrag.MeshObject = nullptr;
	}
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::ReleaseMeshObjectForTeardown()
{
	DestroyMeshObject();
}

bool FArcMassInstancedSkinnedMeshRenderStateHelper::ShouldCreateRenderState() const
{
	const FArcMassRenderISMSkinnedFragment& ISMFrag = GetRenderISMSkinnedFragment();
	return ISMFrag.PerInstanceData.Num() > 0 && ISMFrag.SceneProxyDesc.IsValid();
}

FInstanceDataManagerSourceDataDesc FArcMassInstancedSkinnedMeshRenderStateHelper::MakeSkinnedSourceDesc()
{
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = GetSkinnedMeshFragment();
	const FArcMassRenderISMSkinnedFragment& ISMFrag = GetRenderISMSkinnedFragment();

	USkinnedAsset* Asset = SkinnedMeshFrag.SkinnedAsset.Get();
	const FRenderBounds MeshBounds = Asset ? FRenderBounds(Asset->GetBounds()) : FRenderBounds();

	const int32 NumInstances = ISMFrag.PerInstanceData.Num();

	const int32 NumCustomDataFloats = (NumInstances > 0 && ISMFrag.PerInstanceCustomData.Num() > 0)
		? ISMFrag.PerInstanceCustomData.Num() / NumInstances
		: 0;

	UTransformProviderData* TransformProvider = SkinnedMeshFrag.TransformProvider;
	const bool bHasPerInstanceLocalBounds = TransformProvider && TransformProvider->HasAnimationBounds();

	FInstanceDataManagerSourceDataDesc ComponentDesc;
	ComponentDesc.PrimitiveMaterialDesc = GetUsedMaterialPropertyDesc(GetScene()->GetShaderPlatform());

	FInstanceDataFlags Flags;
	Flags.bHasPerInstanceRandom = ComponentDesc.PrimitiveMaterialDesc.bAnyMaterialHasPerInstanceRandom;
	Flags.bHasPerInstanceCustomData = NumCustomDataFloats > 0;
	Flags.bHasPerInstanceHierarchyOffset = false;
	Flags.bHasPerInstanceLocalBounds = bHasPerInstanceLocalBounds;
	// MANDATORY: never true here — no CPU prev-transforms / bone attachment, and it guards a
	// null-component motion-vector deref inside FlushChanges.
	Flags.bHasPerInstanceDynamicData = false;
	Flags.bHasPerInstanceSkinningData = (TransformProvider != nullptr);
	Flags.bHasPerInstanceLMSMUVBias = false;
#if WITH_EDITOR
	Flags.bHasPerInstanceEditorData = GIsEditor != 0 && ISMFrag.PerInstanceHitProxy.Num() > 0;
#endif // WITH_EDITOR
	ComponentDesc.Flags = Flags;

	ComponentDesc.MeshBounds = MeshBounds;
	ComponentDesc.NumCustomDataFloats = NumCustomDataFloats;
	ComponentDesc.NumInstances = NumInstances;
	ComponentDesc.PrimitiveLocalToWorld = GetRenderMatrix();
	ComponentDesc.ComponentMobility = GetMobility();

	const FTransform ComponentTransform = GetComponentTransform();

	ComponentDesc.BuildChangeSet =
	[
		this,
		ComponentTransform,
		TransformProvider,
		MeshBounds,
		NumCustomDataFloats,
		bHasPerInstanceLocalBounds
	] (FInstanceUpdateChangeSet& ChangeSet)
	{
		const FArcMassRenderISMSkinnedFragment& BuildFrag = GetRenderISMSkinnedFragment();

		// Transforms (always required)
		ChangeSet.GetTransformWriter().Gather(
			[&BuildFrag](int32 InstanceIndex) -> FRenderTransform
			{
				return FRenderTransform(BuildFrag.PerInstanceData[InstanceIndex].Transform.ToMatrixWithScale());
			});

		// Custom data
		ChangeSet.GetCustomDataWriter().Gather(MakeArrayView(BuildFrag.PerInstanceCustomData), NumCustomDataFloats);

		// Skinning data (per-instance animation-bank offset)
		if (TransformProvider)
		{
			ChangeSet.GetSkinningDataWriter().Gather(
				[&BuildFrag, TransformProvider, ComponentTransform](int32 InstanceIndex) -> uint32
				{
					check(BuildFrag.PerInstanceData.IsValidIndex(InstanceIndex));
					const FArcSkinnedMeshInstanceData& Instance = BuildFrag.PerInstanceData[InstanceIndex];
					FSkinnedMeshInstanceData SkinData;
					SkinData.Transform = Instance.Transform;
					SkinData.AnimationIndex = Instance.AnimationIndex;
					return TransformProvider->GetSkinningDataOffset(InstanceIndex, ComponentTransform, SkinData);
				});
		}
		else
		{
			ChangeSet.GetSkinningDataWriter().Gather(0u);
		}

		// Local bounds
		if (TransformProvider && bHasPerInstanceLocalBounds)
		{
			ChangeSet.GetLocalBoundsWriter().Gather(
				[&BuildFrag, TransformProvider, MeshBounds](int32 InstanceIndex) -> FRenderBounds
				{
					const uint32 AnimationIndex = BuildFrag.PerInstanceData[InstanceIndex].AnimationIndex;
					FRenderBounds AnimationBounds;
					if (TransformProvider->GetAnimationBounds(AnimationIndex, AnimationBounds))
					{
						return AnimationBounds;
					}
					return MeshBounds;
				});
		}
		else
		{
			ChangeSet.GetLocalBoundsWriter().Gather(MeshBounds);
		}

#if WITH_EDITOR
		if (ChangeSet.Flags.bHasPerInstanceEditorData && BuildFrag.PerInstanceHitProxy.Num() > 0)
		{
			// FInstanceUpdateChangeSet::SetEditorData is Engine-internal (no ENGINE_API). Replicate its
			// body using the public change-set members + the exported MakeOpaqueHitProxyContainer, matching
			// the static path (ArcISMRenderStateHelper.cpp). Dense — hit proxies are kept parallel to
			// PerInstanceData by the callers.
			TArray<TRefCountPtr<HHitProxy>> HitProxies;
			HitProxies.Reserve(BuildFrag.PerInstanceHitProxy.Num());
			for (const TRefCountPtr<HArcTypeElementHandleHitProxy>& HitProxy : BuildFrag.PerInstanceHitProxy)
			{
				HitProxies.Add(TRefCountPtr<HHitProxy>(HitProxy.GetReference()));
			}

			ChangeSet.HitProxyContainer = MakeOpaqueHitProxyContainer(HitProxies);
			ChangeSet.InstanceEditorData.Reset(HitProxies.Num());
			for (int32 ProxyIndex = 0; ProxyIndex < HitProxies.Num(); ++ProxyIndex)
			{
				FColor HitProxyColor = FColor::Black;
				if (HitProxies[ProxyIndex].IsValid())
				{
					HitProxyColor = HitProxies[ProxyIndex]->Id.GetColor();
				}
				ChangeSet.InstanceEditorData.Add(FInstanceEditorData::Pack(HitProxyColor, /*bSelected*/ false));
			}
			ChangeSet.SelectedInstances.Init(false, HitProxies.Num());
		}
#endif // WITH_EDITOR
	};

	return ComponentDesc;
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::CreateRenderState(FRegisterComponentContext* Context)
{
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = GetSkinnedMeshFragment();

	USkinnedAsset* Asset = SkinnedMeshFrag.SkinnedAsset.Get();
	if (!Asset)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] Skinned CreateRenderState: helper=%p entity=[%d:%d] SkinnedAsset not resident — no proxy",
			this, EntityHandle.Index, EntityHandle.SerialNumber);
		return;
	}

	FSkeletalMeshRenderData* RenderData = Asset->GetResourceForRendering();
	if (!RenderData)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] Skinned CreateRenderState: helper=%p entity=[%d:%d] asset '%ls' has no render data — no proxy",
			this, EntityHandle.Index, EntityHandle.SerialNumber, *GetPathNameSafe(Asset));
		return;
	}

	// Create MeshObject if not already created (first time or after holder destruction)
	if (!ISMFrag.MeshObject && ISMFrag.SceneProxyDesc.IsValid())
	{
		ISMFrag.SceneProxyDesc->Scene = GetScene();
		ISMFrag.MeshObject = FInstancedSkinnedMeshSceneProxyDesc::CreateMeshObject(
			*ISMFrag.SceneProxyDesc, RenderData, GetScene()->GetFeatureLevel());
		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] Skinned CreateRenderState: helper=%p entity=[%d:%d] created meshObject=%p",
			this, EntityHandle.Index, EntityHandle.SerialNumber, ISMFrag.MeshObject);
	}

	if (!ISMFrag.MeshObject)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] Skinned CreateRenderState: helper=%p entity=[%d:%d] no MeshObject (descValid=%d) — no proxy",
			this, EntityHandle.Index, EntityHandle.SerialNumber, ISMFrag.SceneProxyDesc.IsValid());
		return;
	}

	// Submit animation changes before building instance data
	UTransformProviderData* TransformProvider = SkinnedMeshFrag.TransformProvider;
	if (TransformProvider && TransformProvider->IsEnabled())
	{
		TransformProvider->SubmitChanges();
	}

	// Set mesh object on descriptor for proxy creation
	ISMFrag.SceneProxyDesc->MeshObject = ISMFrag.MeshObject;

	// Seed the manager with one entry per current instance (tail-append → identity id map). The manager
	// is a persistent member; DestroyRenderState (base class) never resets it, so it must be FULLY
	// emptied before the dense Add sequence below. ClearInstances resets the id↔index maps AND change
	// tracking (engine precedent: InstancedSkinnedMeshComponent.cpp OnRegister Reset(N)+ClearChangeTracking,
	// and ClearInstances at :1826). ClearChangeTracking alone leaves IndexToIdMap populated, so on a
	// rebuild every re-Add became an INSERT — doubling the map into a non-identity mapping that
	// disagreed with the rebuilt proxy and crashed the first RemoveAtSwap delta flush
	// (RangeCheck in FUpdatableInstanceDataSceneProxy::Update on a worker task).
	// This is the only legal call site (the incremental hot path must never reset the manager, or it
	// silently forces a full rebuild). We address instances by dense index only and discard the id.
	InstanceDataManager.ClearInstances();
	for (int32 Index = 0; Index < ISMFrag.PerInstanceData.Num(); ++Index)
	{
		InstanceDataManager.Add(Index);
	}

	// The manager owns the data proxy; hand it to the scene-proxy desc.
	ISMFrag.SceneProxyDesc->InstanceDataSceneProxy = InstanceDataManager.GetOrCreateProxy();

	// Full Build — Initial state ⇒ FlushChanges does a full update then promotes the manager to Tracked.
	InstanceDataManager.FlushChanges(MakeSkinnedSourceDesc());

	// Delegate to base class which calls CreateSceneProxy() then registers with scene
	Super::CreateRenderState(Context);
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::DestroyRenderState(FArcDestroyRenderStateContext* Context)
{
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] Skinned DestroyRenderState: helper=%p entity=[%d:%d] meshObject=%p (preserved) instances=%d",
		this, EntityHandle.Index, EntityHandle.SerialNumber, ISMFrag.MeshObject, ISMFrag.PerInstanceData.Num());
	ISMFrag.InstanceDataSceneProxy.Reset();
	// MeshObject is NOT deleted here — preserved across proxy recreations
	// Clear the override-materials view so the long-lived SceneProxyDesc does not retain
	// a dangling view into the helper-owned substitution buffer after this helper is gone.
	if (ISMFrag.SceneProxyDesc.IsValid())
	{
		ISMFrag.SceneProxyDesc->OverrideMaterials = TArrayView<const TObjectPtr<UMaterialInterface>>();
	}

	Super::DestroyRenderState(Context);
}

FPrimitiveSceneProxy* FArcMassInstancedSkinnedMeshRenderStateHelper::CreateSceneProxy(UE::ArcMassRender::ESceneProxyCreationError* OutError)
{
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	if (!ISMFrag.SceneProxyDesc.IsValid() || !ISMFrag.MeshObject || ISMFrag.PerInstanceData.Num() == 0)
	{
		return nullptr;
	}

	InitializeSceneProxyDescDynamicProperties();

	const bool bHideSkin = false;
	const bool bNanite = ISMFrag.SceneProxyDesc->ShouldNaniteSkin();
	const bool bEnabled = true;

	PrimitiveSceneData.SceneProxy = FInstancedSkinnedMeshSceneProxyDesc::CreateSceneProxy(
		*ISMFrag.SceneProxyDesc, bHideSkin, bNanite, bEnabled);

	return PrimitiveSceneData.SceneProxy;
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::InitializeSceneProxyDescDynamicProperties()
{
	Super::InitializeSceneProxyDescDynamicProperties();

	// Restore InstanceDataSceneProxy after ResetSceneProxyDescUnsupportedProperties nulled it
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	if (ISMFrag.SceneProxyDesc.IsValid())
	{
		// Re-assign the manager-owned data proxy so a prior ResetSceneProxyDescUnsupportedProperties null
		// is repaired before any proxy (re)creation.
		ISMFrag.SceneProxyDesc->InstanceDataSceneProxy = InstanceDataManager.GetOrCreateProxy();
		// Apply per-entity material overrides (FArcOverrideMaterialsFragment on the holder) to the
		// skinned proxy desc, mirroring the static path. Re-applied on every proxy (re)creation.
		// Routes through BuildRenderSafeOverrideMaterials so incomplete-shader slots are replaced
		// with the default surface material instead of crashing the render thread.
		ISMFrag.SceneProxyDesc->OverrideMaterials = BuildRenderSafeOverrideMaterials(GetNumMaterials(), [this](int32 SlotIndex){ return GetMaterial(SlotIndex); });
	}
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::ResetSceneProxyDescUnsupportedProperties()
{
	Super::ResetSceneProxyDescUnsupportedProperties();

	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	if (ISMFrag.SceneProxyDesc.IsValid())
	{
		ISMFrag.SceneProxyDesc->InstanceDataSceneProxy = nullptr;
	}
}

// ---------------------------------------------------------------------------
// Incremental instance updates (generic FInstanceDataManager API)
// ---------------------------------------------------------------------------

bool FArcMassInstancedSkinnedMeshRenderStateHelper::AppendInstanceIncremental(int32 Index)
{
	if (ArcSkinnedISMHelper_Private::IsRenderTeardownInProgress(GetWorld()))
	{
		return true; // Handled (no-op): never dispatch an instance-data delta into a dying scene.
	}

	if (GetSceneProxy() == nullptr)
	{
		return false;
	}

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] Skinned AppendIncremental: helper=%p entity=[%d:%d] idx=%d fragCount=%d state=%d -> FlushChanges",
		this, EntityHandle.Index, EntityHandle.SerialNumber, Index,
		GetRenderISMSkinnedFragment().PerInstanceData.Num(), static_cast<int32>(InstanceDataManager.GetState()));

	// Mutate manager first (MakeSkinnedSourceDesc reads the count / data after the mutation).
	InstanceDataManager.Add(Index);

	InstanceDataManager.FlushChanges(MakeSkinnedSourceDesc());
	GetScene()->UpdatePrimitiveInstances(&SceneDesc);
	return true;
}

bool FArcMassInstancedSkinnedMeshRenderStateHelper::RemoveInstanceIncremental(int32 Index)
{
	if (GetSceneProxy() == nullptr)
	{
		return false;
	}

	if (ArcSkinnedISMHelper_Private::IsRenderTeardownInProgress(GetWorld()))
	{
		// Confirmed crash site: incremental removes fire during World::BeginTearingDown. The caller has
		// already updated its bookkeeping; the holder is about to be destroyed, so skip the delta flush.
		return true; // Handled (no-op) — do NOT fall back to MarkRenderStateDirty during teardown.
	}

	UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] Skinned RemoveIncremental: helper=%p entity=[%d:%d] idx=%d fragCount=%d state=%d -> FlushChanges",
		this, EntityHandle.Index, EntityHandle.SerialNumber, Index,
		GetRenderISMSkinnedFragment().PerInstanceData.Num(), static_cast<int32>(InstanceDataManager.GetState()));

	// Only the manager entry is removed here; the caller owns the lockstep swap-remove of the dense
	// fragment arrays and the registry-map / cache-fragment repointing (see header contract).
	InstanceDataManager.RemoveAtSwap(Index);

	InstanceDataManager.FlushChanges(MakeSkinnedSourceDesc());
	GetScene()->UpdatePrimitiveInstances(&SceneDesc);
	return true;
}

bool FArcMassInstancedSkinnedMeshRenderStateHelper::UpdateInstanceTransformIncremental(int32 Index)
{
	if (ArcSkinnedISMHelper_Private::IsRenderTeardownInProgress(GetWorld()))
	{
		return true; // Handled (no-op): no instance-data delta into a dying scene. (See Remove for the confirmed case.)
	}

	if (GetSceneProxy() == nullptr)
	{
		return false;
	}

	UE_LOGF(LogArcMassVisDiag, VeryVerbose,"[ArcVisDiag] Skinned TransformIncremental: helper=%p entity=[%d:%d] idx=%d fragCount=%d state=%d -> FlushChanges",
		this, EntityHandle.Index, EntityHandle.SerialNumber, Index,
		GetRenderISMSkinnedFragment().PerInstanceData.Num(), static_cast<int32>(InstanceDataManager.GetState()));

	// A stale index here corrupts the delta changeset and crashes the async proxy Update task
	// (FUpdatableInstanceDataSceneProxy::Update RangeCheck) — catch it on the calling thread.
	if (!GetRenderISMSkinnedFragment().PerInstanceData.IsValidIndex(Index))
	{
		UE_LOGF(LogArcMassVisDiag, Error,"[ArcVisDiag] Skinned TransformIncremental: helper=%p entity=[%d:%d] STALE idx=%d (fragCount=%d) — skipping delta",
			this, EntityHandle.Index, EntityHandle.SerialNumber, Index,
			GetRenderISMSkinnedFragment().PerInstanceData.Num());
		return true;
	}

	// Delta record only — must be in Tracked state or this silently forces a full rebuild (churn returns).
	ensure(InstanceDataManager.GetState() == FInstanceDataManager::ETrackingState::Tracked);
	InstanceDataManager.TransformChanged(Index);

	InstanceDataManager.FlushChanges(MakeSkinnedSourceDesc());
	GetScene()->UpdatePrimitiveInstances(&SceneDesc);
	return true;
}

bool FArcMassInstancedSkinnedMeshRenderStateHelper::UpdateInstanceAnimationIncremental(int32 Index)
{
	if (ArcSkinnedISMHelper_Private::IsRenderTeardownInProgress(GetWorld()))
	{
		return true; // Handled (no-op): no instance-data delta into a dying scene. (See Remove for the confirmed case.)
	}

	if (GetSceneProxy() == nullptr)
	{
		return false;
	}

	// Delta record only — must be in Tracked state or this silently forces a full rebuild (churn returns).
	ensure(InstanceDataManager.GetState() == FInstanceDataManager::ETrackingState::Tracked);
	InstanceDataManager.SkinningDataChanged(Index);

	InstanceDataManager.FlushChanges(MakeSkinnedSourceDesc());
	GetScene()->UpdatePrimitiveInstances(&SceneDesc);
	return true;
}

FPrimitiveSceneProxyDesc& FArcMassInstancedSkinnedMeshRenderStateHelper::GetSceneProxyDesc()
{
	FArcMassRenderISMSkinnedFragment& ISMFrag = GetMutableRenderISMSkinnedFragment();
	checkf(ISMFrag.SceneProxyDesc.IsValid(), TEXT("Expecting a valid scene proxy desc"));
	return *ISMFrag.SceneProxyDesc;
}

const FPrimitiveSceneProxyDesc& FArcMassInstancedSkinnedMeshRenderStateHelper::GetSceneProxyDesc() const
{
	const FArcMassRenderISMSkinnedFragment& ISMFrag = GetRenderISMSkinnedFragment();
	checkf(ISMFrag.SceneProxyDesc.IsValid(), TEXT("Expecting a valid scene proxy desc"));
	return *ISMFrag.SceneProxyDesc;
}

UMaterialInterface* FArcMassInstancedSkinnedMeshRenderStateHelper::GetMaterial(int32 MaterialIndex) const
{
	TConstArrayView<TObjectPtr<UMaterialInterface>> Overrides = GetOverrideMaterials();
	if (Overrides.IsValidIndex(MaterialIndex) && Overrides[MaterialIndex])
	{
		return Overrides[MaterialIndex];
	}
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = GetSkinnedMeshFragment();
	USkinnedAsset* Asset = SkinnedMeshFrag.SkinnedAsset.Get();
	if (Asset && Asset->GetMaterials().IsValidIndex(MaterialIndex))
	{
		return Asset->GetMaterials()[MaterialIndex].MaterialInterface;
	}
	return nullptr;
}

int32 FArcMassInstancedSkinnedMeshRenderStateHelper::GetNumMaterials() const
{
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = GetSkinnedMeshFragment();
	USkinnedAsset* Asset = SkinnedMeshFrag.SkinnedAsset.Get();
	return Asset ? Asset->GetMaterials().Num() : 0;
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool /*bGetDebugMaterials*/) const
{
	const int32 NumMaterials = GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		UMaterialInterface* Material = GetMaterial(MaterialIndex);
		if (Material)
		{
			OutMaterials.Add(Material);
		}
	}
}

void FArcMassInstancedSkinnedMeshRenderStateHelper::GetDefaultMaterialSlotsOverlayMaterial(TArray<TObjectPtr<UMaterialInterface>>& /*AssetMaterialSlotOverlayMaterials*/) const
{
}

const TArray<TObjectPtr<UMaterialInterface>>& FArcMassInstancedSkinnedMeshRenderStateHelper::GetComponentMaterialSlotsOverlayMaterial() const
{
	return EmptyMaterialSlotOverlays;
}

UMaterialInterface* FArcMassInstancedSkinnedMeshRenderStateHelper::GetOverlayMaterial() const
{
	return nullptr;
}

// ISkinnedMeshComponent
USkinnedAsset* FArcMassInstancedSkinnedMeshRenderStateHelper::GetSkinnedAsset() const
{
	const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = GetSkinnedMeshFragment();
	return SkinnedMeshFrag.SkinnedAsset.Get();
}

IPrimitiveComponent* FArcMassInstancedSkinnedMeshRenderStateHelper::GetPrimitiveComponentInterface()
{
	return this;
}

// Fragment accessors
const FArcMassRenderISMSkinnedFragment& FArcMassInstancedSkinnedMeshRenderStateHelper::GetRenderISMSkinnedFragment() const
{
	return GetEntityManager().GetFragmentDataChecked<FArcMassRenderISMSkinnedFragment>(EntityHandle);
}

FArcMassRenderISMSkinnedFragment& FArcMassInstancedSkinnedMeshRenderStateHelper::GetMutableRenderISMSkinnedFragment()
{
	return GetEntityManager().GetFragmentDataChecked<FArcMassRenderISMSkinnedFragment>(EntityHandle);
}

const FArcMassSkinnedMeshFragment& FArcMassInstancedSkinnedMeshRenderStateHelper::GetSkinnedMeshFragment() const
{
	return GetEntityManager().GetConstSharedFragmentDataChecked<FArcMassSkinnedMeshFragment>(EntityHandle);
}
