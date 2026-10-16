// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcMeshRenderStateHelper.h"
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
#include "StaticMeshComponentLODInfo.h"
#endif

class FColorVertexBuffer;
class UMaterialInterface;
struct FStaticMeshSceneProxyDesc;
struct FArcRenderStaticMeshFragment;
namespace Nanite
{
	struct FResources;
	struct FMaterialAudit;
}

/**
 * Base helper to handle all the communication to the renderer from Mass for all type of static meshes
 */
struct ARCMASSRENDER_API FArcBaseStaticMeshRenderStateHelper : public FArcMeshRenderStateHelper, public IStaticMeshComponent
{
public:
	using Super = FArcMeshRenderStateHelper;

	FArcBaseStaticMeshRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager, const FArcRenderPrimitiveFragment& RenderPrimitiveFragment, const FArcOverrideMaterialsFragment* OverrideMaterialsFragment);
	virtual ~FArcBaseStaticMeshRenderStateHelper() override = default;

	const FCollisionResponseContainer& GetCollisionResponseToChannels() const;
	UMaterialInterface* GetNaniteAuditMaterial(int32 MaterialIndex) const;
	const Nanite::FResources* GetNaniteResources() const;
	bool HasValidNaniteData() const;
	bool UseNaniteOverrideMaterials(bool bDoingMaterialAudit) const;
	bool ShouldCreateNaniteProxy(Nanite::FMaterialAudit* OutNaniteMaterials) const;
	virtual UObject const* AdditionalStatObject() const override;
	bool IsReverseCulling() const;
	bool IsDisallowNanite() const;
	bool IsForceDisableNanite() const;
	bool IsForceNaniteForMasked() const;
	int32 GetForcedLodModel() const;
	bool GetOverrideMinLOD() const;
	int32 GetMinLOD() const;
	bool GetForceNaniteForMasked() const;
	int32 GetWorldPositionOffsetDisableDistance() const;
	float GetNanitePixelProgrammableDistance() const;
	bool EvaluateWorldPositionOffsetInRayTracing() const;
#if WITH_EDITORONLY_DATA
	bool IsDisplayNaniteFallbackMesh() const;
#endif

	//~ Begin IStaticMeshComponent interface
#if WITH_EDITOR
	virtual void OnMeshRebuild(bool bRenderDataChanged) override {}
	virtual void PreStaticMeshCompilation() override {}
	virtual void PostStaticMeshCompilation() override;
#endif
	virtual UStaticMesh* GetStaticMesh() const override;
	virtual IPrimitiveComponent* GetPrimitiveComponentInterface() override;
	//~ End IStaticMeshComponent interface

	//~ Begin FArcPrimitiveRenderStateHelper interface
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	virtual int32 GetNumMaterials() const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
#if WITH_EDITOR
	virtual void NotifyRenderStateChanged() override;
#endif
	//~ End FArcPrimitiveRenderStateHelper interface

protected:
	//~ Begin FArcPrimitiveRenderStateHelper interface
	virtual void ResetSceneProxyDescUnsupportedProperties() override;
	virtual void InitializeSceneProxyDescDynamicProperties() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy(UE::ArcMassRender::ESceneProxyCreationError* OutError = nullptr) override;
	//~ End FArcPrimitiveRenderStateHelper interface

	//~ Begin FArcMeshRenderStateHelper interface
	virtual UMaterialInterface* GetOverlayMaterial() const override;
	virtual const TArray<TObjectPtr<UMaterialInterface>>& GetComponentMaterialSlotsOverlayMaterial() const override;
	virtual void GetDefaultMaterialSlotsOverlayMaterial(TArray<TObjectPtr<UMaterialInterface>>& OutMaterialSlotOverlayMaterials) const override;
	//~ End FArcMeshRenderStateHelper interface

	virtual FStaticMeshSceneProxyDesc& GetStaticMeshSceneProxyDesc() = 0;
	virtual const FStaticMeshSceneProxyDesc& GetStaticMeshSceneProxyDesc() const = 0;
	virtual FPrimitiveSceneProxy* CreateStaticMeshSceneProxy(const Nanite::FMaterialAudit& NaniteMaterials, bool bCreateNanite);
	UMaterialInterface* GetMaterial(int32 MaterialIndex, bool bDoingNaniteMaterialAudit) const;
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	virtual void CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams);

	// Read by the engine collect templates as Component.LODData; always empty, matching the desc LODData that ResetSceneProxyDescUnsupportedProperties clears before every proxy build.
	TArray<FStaticMeshComponentLODInfo> LODData;
#endif

	friend class FMeshComponentHelper;
	friend class FPrimitiveComponentHelper;
	friend class FStaticMeshComponentHelper;
	friend class FInstancedStaticMeshComponentHelper;
};

/**
 * Helper to handle all the communication to the renderer from Mass for all type of static meshes
 */
struct ARCMASSRENDER_API FArcStaticMeshRenderStateHelper : public FArcBaseStaticMeshRenderStateHelper
{
public:
	using Super = FArcBaseStaticMeshRenderStateHelper;

	FArcStaticMeshRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager, const FArcRenderPrimitiveFragment& RenderPrimitiveFragment, const FArcOverrideMaterialsFragment* OverrideMaterialsFragment, const FArcRenderStaticMeshFragment& RenderStaticMeshFragment);
	virtual ~FArcStaticMeshRenderStateHelper() = default;

	// StaticMesh Fragments helpers
	const FArcRenderStaticMeshFragment& GetRenderStaticMeshFragment() const;
	FArcRenderStaticMeshFragment& GetMutableRenderStaticMeshFragment();

protected:
	//~ Begin FArcPrimitiveRenderStateHelper interface
	virtual FPrimitiveSceneProxyDesc& GetSceneProxyDesc() override;
	virtual const FPrimitiveSceneProxyDesc& GetSceneProxyDesc() const override;
	//~ End FArcPrimitiveRenderStateHelper interface

	//~ Begin FArcBaseStaticMeshRenderStateHelper interface
	virtual FStaticMeshSceneProxyDesc& GetStaticMeshSceneProxyDesc() override;
	virtual const FStaticMeshSceneProxyDesc& GetStaticMeshSceneProxyDesc() const override;
	//~ End FArcBaseStaticMeshRenderStateHelper interface
};
