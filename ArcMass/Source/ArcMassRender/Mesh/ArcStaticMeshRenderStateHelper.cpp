// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcStaticMeshRenderStateHelper.h"

#include "Engine/StaticMesh.h"
#include "Mass/EntityManager.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "Physics/MassEnginePhysicsFragments.h"
#include "Rendering/NaniteResourcesHelper.h"
#include "StaticMeshComponentHelper.h"
#include "StaticMeshResources.h"
#include "StaticMeshSceneProxyDesc.h"

#if WITH_EDITOR
#include "ObjectCacheEventSink.h"
#include "WorldPartition/HLOD/HLODActor.h"
#endif

// ----------------------------------------------------------------------//
// FArcBaseStaticMeshRenderStateHelper
//----------------------------------------------------------------------//
FArcBaseStaticMeshRenderStateHelper::FArcBaseStaticMeshRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager, const FArcRenderPrimitiveFragment& RenderPrimitiveFragment, const FArcOverrideMaterialsFragment* OverrideMaterialsFragment)
	: Super(InEntityHandle, InEntityManager, RenderPrimitiveFragment, OverrideMaterialsFragment)
{
}

const Nanite::FResources* FArcBaseStaticMeshRenderStateHelper::GetNaniteResources() const
{
	if (GetStaticMesh() && GetStaticMesh()->GetRenderData())
	{
		return GetStaticMesh()->GetRenderData()->GetNaniteResources();
	}
	return nullptr;
}

#if WITH_EDITOR
void FArcBaseStaticMeshRenderStateHelper::NotifyRenderStateChanged()
{
	Super::NotifyRenderStateChanged();

	FObjectCacheEventSink::NotifyStaticMeshChanged_Concurrent(this);
}
#endif

int32 FArcBaseStaticMeshRenderStateHelper::GetNumMaterials() const
{
	return GetStaticMesh() ? GetStaticMesh()->GetStaticMaterials().Num() : 0;
}

UMaterialInterface* FArcBaseStaticMeshRenderStateHelper::GetMaterial(int32 MaterialIndex) const
{
	return GetMaterial(MaterialIndex, false);
}

const FCollisionResponseContainer& FArcBaseStaticMeshRenderStateHelper::GetCollisionResponseToChannels() const
{
	if (FMassPhysicsCollisionSettingsFragment* CollisionSettingsFragment = GetEntityManager().GetConstSharedFragmentDataPtr<FMassPhysicsCollisionSettingsFragment>(EntityHandle))
	{
		return CollisionSettingsFragment->CollisionResponse;
	}

	return FCollisionResponseContainer::GetDefaultResponseContainer();
}

UMaterialInterface* FArcBaseStaticMeshRenderStateHelper::GetNaniteAuditMaterial(int32 MaterialIndex) const
{
	return GetMaterial(MaterialIndex, true);
}

bool FArcBaseStaticMeshRenderStateHelper::UseNaniteOverrideMaterials(bool bDoingMaterialAudit) const
{
	return Nanite::FNaniteResourcesHelper::UseNaniteOverrideMaterials(*this, bDoingMaterialAudit);
}

bool FArcBaseStaticMeshRenderStateHelper::ShouldCreateNaniteProxy(Nanite::FMaterialAudit* OutNaniteMaterials) const
{
	return Nanite::FNaniteResourcesHelper::ShouldCreateNaniteProxy(GetStaticMeshSceneProxyDesc(), OutNaniteMaterials);
}

UMaterialInterface* FArcBaseStaticMeshRenderStateHelper::GetMaterial(int32 MaterialIndex, bool bDoingNaniteMaterialAudit) const
{
	IMaterial* Material = FStaticMeshComponentHelper::GetMaterial(*this, MaterialIndex, bDoingNaniteMaterialAudit);
	return Material ? Material->GetSourceAsset() : nullptr;
}

bool FArcBaseStaticMeshRenderStateHelper::HasValidNaniteData() const
{
	return Nanite::FNaniteResourcesHelper::HasValidNaniteData(*this);
}

void FArcBaseStaticMeshRenderStateHelper::ResetSceneProxyDescUnsupportedProperties()
{
	Super::ResetSceneProxyDescUnsupportedProperties();

	// Unsupported properties
	FStaticMeshSceneProxyDesc& SceneProxyDesc = GetStaticMeshSceneProxyDesc();
	SceneProxyDesc.LODData = TArrayView<struct FStaticMeshComponentLODInfo>();
	SceneProxyDesc.LODParentPrimitive = nullptr;
	SceneProxyDesc.MeshPaintTexture = nullptr;
	SceneProxyDesc.MeshPaintTextureCoordinateIndex = 0;
#if STATICMESH_ENABLE_DEBUG_RENDERING
	SceneProxyDesc.bDrawMeshCollisionIfComplex = false;
	SceneProxyDesc.bDrawMeshCollisionIfSimple = false;
#endif // STATICMESH_ENABLE_DEBUG_RENDERING
	SceneProxyDesc.bDisplayNaniteFallbackMesh = false;
#if WITH_EDITORONLY_DATA
	SceneProxyDesc.SectionIndexPreview = INDEX_NONE;
	SceneProxyDesc.MaterialIndexPreview = INDEX_NONE;
	SceneProxyDesc.SelectedEditorMaterial = INDEX_NONE;
	SceneProxyDesc.SelectedEditorSection = INDEX_NONE;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITORONLY_DATA
	SceneProxyDesc.MaterialStreamingRelativeBoxes = TArrayView<uint32>();
#endif // WITH_EDITORONLY_DATA

	// Properties that will be initialized by InitializeSceneProxyDescDynamicProperties
	SceneProxyDesc.NaniteResources = nullptr;
	SceneProxyDesc.BodySetup = nullptr;
	SceneProxyDesc.MaterialRelevance = FMaterialRelevance();
	SceneProxyDesc.bUseProvidedMaterialRelevance = false;
}

void FArcBaseStaticMeshRenderStateHelper::InitializeSceneProxyDescDynamicProperties()
{
	Super::InitializeSceneProxyDescDynamicProperties();

	FStaticMeshSceneProxyDesc& SceneProxyDesc = GetStaticMeshSceneProxyDesc();
	// Substitutes incomplete-shader slots with default surface material (crash-safe grey); returns empty view when no overrides and nothing was substituted.
	// Cast strips const: SceneProxyDesc.OverrideMaterials is a non-const TArrayView; the buffer is owned by the base-class member until the next call.
	TConstArrayView<TObjectPtr<UMaterialInterface>> OverrideMaterials = BuildRenderSafeOverrideMaterials(GetNumMaterials(), [this](int32 SlotIndex){ return GetMaterial(SlotIndex, false); });
	SceneProxyDesc.OverrideMaterials = (TArrayView<TObjectPtr<UMaterialInterface>>&)OverrideMaterials;
	SceneProxyDesc.NaniteResources = GetNaniteResources();
	SceneProxyDesc.SetMaterialRelevance(GetMaterialRelevance(GetScene()->GetShaderPlatform()));
	SceneProxyDesc.SetCollisionResponseToChannels(GetCollisionResponseToChannels());

	// Add LODData support
	// Add LODParentPrimitive support
	// Add MeshPaintTexture/MeshPaintTextureCoordinateIndex support
}

FPrimitiveSceneProxy* FArcBaseStaticMeshRenderStateHelper::CreateSceneProxy(UE::ArcMassRender::ESceneProxyCreationError* OutError)
{
	check(GetWorld());
	FSceneInterface* Scene = GetScene();
	UStaticMesh* StaticMesh = GetStaticMesh();
	check(Scene);
	check(StaticMesh);
	check(StaticMesh->GetRenderData());
	check(StaticMesh->GetRenderData()->IsInitialized());
	check(!StaticMesh->IsCompiling());

	if (OutError)
	{
		*OutError = UE::ArcMassRender::ESceneProxyCreationError::None;
	}

	InitializeSceneProxyDescDynamicProperties();

	const FStaticMeshSceneProxyDesc& SceneProxyDesc = GetStaticMeshSceneProxyDesc();
	check(SceneProxyDesc.Scene);
	check(SceneProxyDesc.Scene == Scene);
	check(SceneProxyDesc.World == Scene->GetWorld());
	check(SceneProxyDesc.FeatureLevel == Scene->GetFeatureLevel());
	check(SceneProxyDesc.ComponentId == GetPrimitiveSceneId());

	FStaticMeshComponentHelper::ESceneProxyCreationError SceneProxyCreationError = FStaticMeshComponentHelper::ESceneProxyCreationError::None;
	FPrimitiveSceneProxy* SceneProxy = FStaticMeshComponentHelper::CreateSceneProxy<FArcBaseStaticMeshRenderStateHelper, /*bRenderDataReady=*/true>(*this, &SceneProxyCreationError);

	if (SceneProxy == nullptr && OutError)
	{
		switch (SceneProxyCreationError)
		{
		case FStaticMeshComponentHelper::ESceneProxyCreationError::WaitingPSOs:
			*OutError = UE::ArcMassRender::ESceneProxyCreationError::WaitingPSOs;
			break;

		default:
			*OutError = UE::ArcMassRender::ESceneProxyCreationError::InvalidMesh;
			break;
		}
	}

	return SceneProxy;
}


FPrimitiveSceneProxy* FArcBaseStaticMeshRenderStateHelper::CreateStaticMeshSceneProxy(const Nanite::FMaterialAudit& NaniteMaterials, bool bCreateNanite)
{
	check(GetWorld());
	const FStaticMeshSceneProxyDesc& SceneProxyDesc = GetStaticMeshSceneProxyDesc();
	check(SceneProxyDesc.Scene);

	if (bCreateNanite)
	{
		PrimitiveSceneData.SceneProxy = ::new Nanite::FSceneProxy(NaniteMaterials, SceneProxyDesc);
	}
	else
	{
		PrimitiveSceneData.SceneProxy = ::new FStaticMeshSceneProxy(SceneProxyDesc, false);
	}
	return PrimitiveSceneData.SceneProxy;
}

void FArcBaseStaticMeshRenderStateHelper::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	FStaticMeshComponentHelper::GetUsedMaterials(*this, OutMaterials, bGetDebugMaterials);
}

void FArcBaseStaticMeshRenderStateHelper::GetDefaultMaterialSlotsOverlayMaterial(TArray<TObjectPtr<UMaterialInterface>>& OutMaterialSlotOverlayMaterials) const
{
	OutMaterialSlotOverlayMaterials.Reset();
	if (GetStaticMesh())
	{
		const TArray<FStaticMaterial>& StaticMaterials = GetStaticMesh()->GetStaticMaterials();
		for (const FStaticMaterial& StaticMaterial : StaticMaterials)
		{
			OutMaterialSlotOverlayMaterials.Add(StaticMaterial.OverlayMaterialInterface);
		}
	}
}

const TArray<TObjectPtr<UMaterialInterface>>& FArcBaseStaticMeshRenderStateHelper::GetComponentMaterialSlotsOverlayMaterial() const
{
	return GetStaticMeshSceneProxyDesc().MaterialSlotsOverlayMaterial;
}

#if WITH_EDITOR
void FArcBaseStaticMeshRenderStateHelper::PostStaticMeshCompilation()
{
	MarkRenderStateDirty();
}
#endif // WITH_EDITOR

UStaticMesh* FArcBaseStaticMeshRenderStateHelper::GetStaticMesh() const
{
	return GetStaticMeshSceneProxyDesc().StaticMesh;
}

IPrimitiveComponent* FArcBaseStaticMeshRenderStateHelper::GetPrimitiveComponentInterface()
{
	return this;
}

UObject const* FArcBaseStaticMeshRenderStateHelper::AdditionalStatObject() const
{
	return GetStaticMesh();
}

UMaterialInterface* FArcBaseStaticMeshRenderStateHelper::GetOverlayMaterial() const
{
	return GetStaticMeshSceneProxyDesc().OverlayMaterial;
}

bool FArcBaseStaticMeshRenderStateHelper::IsReverseCulling() const
{
	return GetStaticMeshSceneProxyDesc().IsReverseCulling();
}

bool FArcBaseStaticMeshRenderStateHelper::IsDisallowNanite() const
{
	return GetStaticMeshSceneProxyDesc().IsDisallowNanite();
}

bool FArcBaseStaticMeshRenderStateHelper::IsForceDisableNanite() const
{
	return GetStaticMeshSceneProxyDesc().IsForceDisableNanite();
}

bool FArcBaseStaticMeshRenderStateHelper::IsForceNaniteForMasked() const
{
	return GetStaticMeshSceneProxyDesc().IsForceNaniteForMasked();
}

int32 FArcBaseStaticMeshRenderStateHelper::GetForcedLodModel() const
{
	return GetStaticMeshSceneProxyDesc().GetForcedLodModel();
}

bool FArcBaseStaticMeshRenderStateHelper::GetOverrideMinLOD() const
{
	return GetStaticMeshSceneProxyDesc().bOverrideMinLOD;
}

int32 FArcBaseStaticMeshRenderStateHelper::GetMinLOD() const
{
	return GetStaticMeshSceneProxyDesc().MinLOD;
}

bool FArcBaseStaticMeshRenderStateHelper::GetForceNaniteForMasked() const
{
	return GetStaticMeshSceneProxyDesc().bForceNaniteForMasked;
}

int32 FArcBaseStaticMeshRenderStateHelper::GetWorldPositionOffsetDisableDistance() const
{
	return GetStaticMeshSceneProxyDesc().GetWorldPositionOffsetDisableDistance();
}

float FArcBaseStaticMeshRenderStateHelper::GetNanitePixelProgrammableDistance() const
{
	return GetStaticMeshSceneProxyDesc().GetNanitePixelProgrammableDistance();
}

bool FArcBaseStaticMeshRenderStateHelper::EvaluateWorldPositionOffsetInRayTracing() const
{
	return GetStaticMeshSceneProxyDesc().EvaluateWorldPositionOffsetInRayTracing();
}

#if WITH_EDITORONLY_DATA
bool FArcBaseStaticMeshRenderStateHelper::IsDisplayNaniteFallbackMesh() const
{
	return GetStaticMeshSceneProxyDesc().IsDisplayNaniteFallbackMesh();
}
#endif


#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
void FArcBaseStaticMeshRenderStateHelper::CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams)
{
	LODData.Reset();
	FStaticMeshComponentHelper::CollectPSOPrecacheData(*this, BasePrecachePSOParams, OutParams);
}
#endif

// ----------------------------------------------------------------------//
// FArcStaticMeshRenderStateHelper
//----------------------------------------------------------------------//
FArcStaticMeshRenderStateHelper::FArcStaticMeshRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager, const FArcRenderPrimitiveFragment& RenderPrimitiveFragment, const FArcOverrideMaterialsFragment* OverrideMaterialsFragment, const FArcRenderStaticMeshFragment& RenderStaticMeshFragment)
	: Super(InEntityHandle, InEntityManager, RenderPrimitiveFragment, OverrideMaterialsFragment)
{
}

const FArcRenderStaticMeshFragment& FArcStaticMeshRenderStateHelper::GetRenderStaticMeshFragment() const
{
	return GetEntityManager().GetFragmentDataChecked<FArcRenderStaticMeshFragment>(EntityHandle);
}

FArcRenderStaticMeshFragment& FArcStaticMeshRenderStateHelper::GetMutableRenderStaticMeshFragment()
{
	return GetEntityManager().GetFragmentDataChecked<FArcRenderStaticMeshFragment>(EntityHandle);
}

FPrimitiveSceneProxyDesc& FArcStaticMeshRenderStateHelper::GetSceneProxyDesc()
{
	return GetStaticMeshSceneProxyDesc();
}

const FPrimitiveSceneProxyDesc& FArcStaticMeshRenderStateHelper::GetSceneProxyDesc() const
{
	return GetStaticMeshSceneProxyDesc();
}

FStaticMeshSceneProxyDesc& FArcStaticMeshRenderStateHelper::GetStaticMeshSceneProxyDesc()
{
	FArcRenderStaticMeshFragment& RenderStaticMeshFragment = GetMutableRenderStaticMeshFragment();
	checkf(RenderStaticMeshFragment.StaticMeshSceneProxyDesc, TEXT("Expecting a valid scene proxy desc"));
	return *RenderStaticMeshFragment.StaticMeshSceneProxyDesc;
}

const FStaticMeshSceneProxyDesc& FArcStaticMeshRenderStateHelper::GetStaticMeshSceneProxyDesc() const
{
	const FArcRenderStaticMeshFragment& RenderStaticMeshFragment = GetRenderStaticMeshFragment();
	checkf(RenderStaticMeshFragment.StaticMeshSceneProxyDesc, TEXT("Expecting a valid scene proxy desc"));
	return *RenderStaticMeshFragment.StaticMeshSceneProxyDesc;
}
