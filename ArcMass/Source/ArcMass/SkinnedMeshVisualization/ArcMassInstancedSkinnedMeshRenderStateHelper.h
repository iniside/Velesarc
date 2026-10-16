// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mesh/ArcMeshRenderStateHelper.h"
#include "InstancedSkinnedMeshSceneProxyDesc.h"
#include "InstanceData/InstanceDataManager.h"
#include "Components/ComponentInterfaces.h"

struct FArcMassRenderISMSkinnedFragment;
struct FArcMassSkinnedMeshFragment;
struct FInstanceDataManagerSourceDataDesc;

/**
 * Render state helper for instanced skinned meshes in Mass.
 * Implements ISkinnedMeshComponent to satisfy the renderer without an actor component.
 * Follows the same pattern as FArcISMRenderStateHelper but for skinned assets.
 */
struct ARCMASS_API FArcMassInstancedSkinnedMeshRenderStateHelper : public FArcMeshRenderStateHelper, public ISkinnedMeshComponent
{
public:
	using Super = FArcMeshRenderStateHelper;

	FArcMassInstancedSkinnedMeshRenderStateHelper(
		FMassEntityHandle InEntityHandle,
		TNotNull<FMassEntityManager*> InEntityManager,
		const FArcRenderPrimitiveFragment& RenderPrimitiveFragment,
		const FArcOverrideMaterialsFragment* OverrideMaterialsFragment);

	virtual ~FArcMassInstancedSkinnedMeshRenderStateHelper() override;

	/** Delete the MeshObject. Called only when the holder entity is destroyed. */
	void DestroyMeshObject();

	/** Editor-teardown hook (FArcRenderStateHelper): release the FSkeletalMeshObject + its update
	 *  handle on world cleanup, so the per-FScene FSkeletalMeshUpdateChannel is not torn down with
	 *  live handles. Delegates to the idempotent DestroyMeshObject(). */
	virtual void ReleaseMeshObjectForTeardown() override;


	//~ Begin FArcRenderStateHelper interface
	virtual bool ShouldCreateRenderState() const override;
	virtual void CreateRenderState(FRegisterComponentContext* Context) override;
	virtual void DestroyRenderState(FArcDestroyRenderStateContext* Context) override;
	//~ End FArcRenderStateHelper interface
protected:
	//~ Begin FMassPrimitiveRenderStateHelper interface
	virtual FPrimitiveSceneProxyDesc& GetSceneProxyDesc() override;
	virtual const FPrimitiveSceneProxyDesc& GetSceneProxyDesc() const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy(UE::ArcMassRender::ESceneProxyCreationError* OutError) override;
	virtual void InitializeSceneProxyDescDynamicProperties() override;
	virtual void ResetSceneProxyDescUnsupportedProperties() override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	virtual int32 GetNumMaterials() const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	//~ End FMassPrimitiveRenderStateHelper interface

	//~ Begin FArcMeshRenderStateHelper interface
	virtual void GetDefaultMaterialSlotsOverlayMaterial(TArray<TObjectPtr<UMaterialInterface>>& AssetMaterialSlotOverlayMaterials) const override;
	virtual const TArray<TObjectPtr<UMaterialInterface>>& GetComponentMaterialSlotsOverlayMaterial() const override;
	virtual UMaterialInterface* GetOverlayMaterial() const override;
	//~ End FArcMeshRenderStateHelper interface

	//~ Begin ISkinnedMeshComponent interface
	virtual USkinnedAsset* GetSkinnedAsset() const override;
	virtual IPrimitiveComponent* GetPrimitiveComponentInterface() override;
#if WITH_EDITOR
	virtual void PostAssetCompilation() override {}
	virtual void PostSkeletalHierarchyChange() override {}
#endif
	//~ End ISkinnedMeshComponent interface

	/**
	 * Build the source-data descriptor consumed by FInstanceDataManager::FlushChanges. Ported from the
	 * engine template FInstancedSkinnedMeshComponentHelper::GetComponentDesc; reads PerInstanceData,
	 * PerInstanceCustomData (and editor hit proxies) synchronously via a by-value/this-captured
	 * BuildChangeSet lambda. bHasPerInstanceDynamicData is forced false (no prev-transforms /
	 * attachment, and it guards a null-component motion-vector deref inside FlushChanges).
	 */
	FInstanceDataManagerSourceDataDesc MakeSkinnedSourceDesc();

	//~ Incremental instance updates (mirrors FArcISMRenderStateHelper). Each returns false when no live
	//~ scene proxy exists (caller falls back to MarkRenderStateDirty / Destroy+Create); on true the manager
	//~ delta was flushed and the scene instances were pushed via UpdatePrimitiveInstances.
public:
	/** Append: the dense slot Index has already been written into PerInstanceData by the caller. */
	bool AppendInstanceIncremental(int32 Index);

	/**
	 * Remove the manager entry for dense slot Index (RemoveAtSwap). CONTRACT: the caller owns the matching
	 * swap-remove of the fragment's PerInstanceData / PerInstanceCustomData / PerInstanceHitProxy, plus
	 * repointing the moved tail entity's registry maps / cache fragment. This call only mutates the
	 * manager (which maintains its own dense-index ↔ id map) and flushes.
	 */
	bool RemoveInstanceIncremental(int32 Index);

	/** Transform-only delta for dense slot Index (PerInstanceData[Index].Transform already updated). */
	bool UpdateInstanceTransformIncremental(int32 Index);

	/** Animation/skinning-offset delta for dense slot Index (PerInstanceData[Index].AnimationIndex updated). */
	bool UpdateInstanceAnimationIncremental(int32 Index);

protected:
	const FArcMassRenderISMSkinnedFragment& GetRenderISMSkinnedFragment() const;
	FArcMassRenderISMSkinnedFragment& GetMutableRenderISMSkinnedFragment();

	const FArcMassSkinnedMeshFragment& GetSkinnedMeshFragment() const;

private:
	/** Owns the FInstanceDataSceneProxy and the dense-index ↔ stable-id tracking. Constructed with a
	 *  null UPrimitiveComponent — this helper is an IPrimitiveComponent, not a UObject component. */
	FInstanceDataManager InstanceDataManager{ nullptr };

	static TArray<TObjectPtr<UMaterialInterface>> EmptyMaterialSlotOverlays;
};
