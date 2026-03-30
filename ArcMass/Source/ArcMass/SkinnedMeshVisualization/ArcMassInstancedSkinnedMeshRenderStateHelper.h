// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassMeshRenderStateHelper.h"
#include "InstancedSkinnedMeshSceneProxyDesc.h"
#include "Components/ComponentInterfaces.h"

struct FArcMassRenderISMSkinnedFragment;
struct FArcMassSkinnedMeshFragment;

/**
 * Render state helper for instanced skinned meshes in Mass.
 * Implements ISkinnedMeshComponent to satisfy the renderer without an actor component.
 * Follows the same pattern as FMassISMRenderStateHelper but for skinned assets.
 */
struct ARCMASS_API FArcMassInstancedSkinnedMeshRenderStateHelper : public FMassMeshRenderStateHelper, public ISkinnedMeshComponent
{
public:
	using Super = FMassMeshRenderStateHelper;

	FArcMassInstancedSkinnedMeshRenderStateHelper(
		FMassEntityHandle InEntityHandle,
		TNotNull<FMassEntityManager*> InEntityManager,
		const FMassRenderPrimitiveFragment& RenderPrimitiveFragment,
		const FMassOverrideMaterialsFragment* OverrideMaterialsFragment);

	virtual ~FArcMassInstancedSkinnedMeshRenderStateHelper() override;

	/** Delete the MeshObject. Called only when the holder entity is destroyed. */
	void DestroyMeshObject();


	//~ Begin FMassRenderStateHelper interface
	virtual bool ShouldCreateRenderState() const override;
	virtual void CreateRenderState(FRegisterComponentContext* Context) override;
	virtual void DestroyRenderState(FMassDestroyRenderStateContext* Context) override;
	//~ End FMassRenderStateHelper interface
protected:
	//~ Begin FMassPrimitiveRenderStateHelper interface
	virtual FPrimitiveSceneProxyDesc& GetSceneProxyDesc() override;
	virtual const FPrimitiveSceneProxyDesc& GetSceneProxyDesc() const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy(ESceneProxyCreationError* OutError) override;
	virtual void InitializeSceneProxyDescDynamicProperties() override;
	virtual void ResetSceneProxyDescUnsupportedProperties() override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	virtual int32 GetNumMaterials() const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	//~ End FMassPrimitiveRenderStateHelper interface

	//~ Begin FMassMeshRenderStateHelper interface
	virtual void GetDefaultMaterialSlotsOverlayMaterial(TArray<TObjectPtr<UMaterialInterface>>& AssetMaterialSlotOverlayMaterials) const override;
	virtual const TArray<TObjectPtr<UMaterialInterface>>& GetComponentMaterialSlotsOverlayMaterial() const override;
	virtual UMaterialInterface* GetOverlayMaterial() const override;
	//~ End FMassMeshRenderStateHelper interface

	//~ Begin ISkinnedMeshComponent interface
	virtual USkinnedAsset* GetSkinnedAsset() const override;
	virtual IPrimitiveComponent* GetPrimitiveComponentInterface() override;
#if WITH_EDITOR
	virtual void PostAssetCompilation() override {}
	virtual void PostSkeletalHierarchyChange() override {}
#endif
	//~ End ISkinnedMeshComponent interface

	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe>& BuildInstanceData();

	const FArcMassRenderISMSkinnedFragment& GetRenderISMSkinnedFragment() const;
	FArcMassRenderISMSkinnedFragment& GetMutableRenderISMSkinnedFragment();

	const FArcMassSkinnedMeshFragment& GetSkinnedMeshFragment() const;

private:
	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> DataProxy{};
	static TArray<TObjectPtr<UMaterialInterface>> EmptyMaterialSlotOverlays;
};
