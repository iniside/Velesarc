// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcStaticMeshRenderStateHelper.h"
#include "InstancedStaticMeshSceneProxyDesc.h"
#include "InstancedStaticMesh/ISMInstanceDataManager.h"

struct FArcRenderISMFragment;

#define UE_API ARCMASSRENDER_API
/**
 * Helper to handle all the communication to the renderer from Mass for an instanced static mesh
 */
struct UE_API FArcISMRenderStateHelper : public FArcBaseStaticMeshRenderStateHelper
{
public:
	using Super = FArcBaseStaticMeshRenderStateHelper;

	FArcISMRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager, const FArcRenderPrimitiveFragment& RenderPrimitiveFragment, const FArcOverrideMaterialsFragment* OverrideMaterialsFragment, const FArcRenderISMFragment& RenderStaticMeshFragment);
	virtual ~FArcISMRenderStateHelper() override = default;

	/**
	 * Incrementally add instance at DenseIndex into the live scene proxy via GPU-Scene.
	 * Returns false when no proxy exists yet; the instance stays in PerInstanceSMData and is picked
	 * up when FArcISMHolderRegistry::RefreshHolderBounds creates the proxy from the complete buffer.
	 * Bounds refresh (UpdateTransform) is the caller's responsibility after recalculating
	 * fragment bounds — do NOT call UpdateTransform inside this method.
	 */
	bool AddInstanceIncremental(int32 DenseIndex);

	/**
	 * Incrementally remove instance at DenseIndex from the live scene proxy via GPU-Scene
	 * using swap-with-last semantics.
	 * Returns false when the proxy does not yet exist; caller falls back to MarkRenderStateDirty().
	 */
	bool RemoveInstanceIncremental(int32 DenseIndex);

	/**
	 * Incrementally patch the transform of a single instance at DenseIndex into the live scene
	 * proxy via GPU-Scene delta update (marks only one TransformChanged bit — no full rebuild).
	 * The caller is responsible for updating PerInstanceSMData[DenseIndex].Transform before
	 * calling this method, so that MakeComponentDesc() picks up the new value.
	 * Returns false when the proxy does not yet exist; caller falls back to MarkRenderStateDirty().
	 */
	bool UpdateInstanceTransformIncremental(int32 DenseIndex);

protected:
	//~ Begin FArcPrimitiveRenderStateHelper interface
	virtual FPrimitiveSceneProxyDesc& GetSceneProxyDesc() override;
	virtual const FPrimitiveSceneProxyDesc& GetSceneProxyDesc() const override;
	virtual void ResetSceneProxyDescUnsupportedProperties() override;
	//~ End FArcPrimitiveRenderStateHelper interface

	//~ Begin FArcBaseStaticMeshRenderStateHelper interface
	virtual FStaticMeshSceneProxyDesc& GetStaticMeshSceneProxyDesc() override;
	virtual const FStaticMeshSceneProxyDesc& GetStaticMeshSceneProxyDesc() const override;
	virtual FPrimitiveSceneProxy* CreateStaticMeshSceneProxy(const Nanite::FMaterialAudit& NaniteMaterials, bool bCreateNanite) override;
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	virtual void CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams) override;
#endif
	//~ End FArcBaseStaticMeshRenderStateHelper interface

	/** Build an FInstanceUpdateComponentDesc that describes the current per-instance state. */
	FInstanceUpdateComponentDesc MakeComponentDesc() const;

	const FArcRenderISMFragment& GetRenderISMFragment() const;
	FArcRenderISMFragment& GetMutableRenderISMFragment();

protected:

	virtual void InitializeSceneProxyDescDynamicProperties() override;

	/** Owned manager drives incremental GPU-Scene updates without proxy teardown. */
	FPrimitiveInstanceDataManager InstanceDataManager{nullptr};

	friend class FInstancedStaticMeshComponentHelper;
};
#undef UE_API
