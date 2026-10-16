// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Mass/EntityElementTypes.h"
#include "Mass/ExternalSubsystemTraits.h"
#include "Elements/Framework/TypedElementHandle.h"

#include "ArcRenderMeshFragments.generated.h"

struct FTransformFragment;
class URuntimeVirtualTexture;
struct FStaticMeshSceneProxyDesc;
struct FInstancedStaticMeshSceneProxyDesc;
struct FArcRenderStateHelper;

#define UE_API ARCMASSRENDER_API

/**
 * Const shared fragment holding a pointer to the static mesh associated to the entity
 */
USTRUCT()
struct FArcStaticMeshFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	FArcStaticMeshFragment() = default;
	UE_API explicit FArcStaticMeshFragment(TNotNull<const UStaticMesh*> InMesh);

	UPROPERTY(VisibleAnywhere, Category=Debug)
	TWeakObjectPtr<const UStaticMesh> Mesh;
};

/**
 * Const shared fragment holding all the visualization flags for a mesh
 */
USTRUCT()
struct FArcVisualizationMeshFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Mesh)
	int64 CustomDepthStencilValue = 0;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	int32 TranslucencySortPriority = 0;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	FLightingChannels LightingChannels;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 HiddenInGame : 1 = false;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 CastShadow : 1 = true;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 CastShadowAsTwoSided : 1 = false;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 CastHiddenShadow : 1 = false;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 AffectDynamicIndirectLighting : 1 = true;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 AffectIndirectLightingWhileHidden : 1 = false;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 ReceivesDecals : 1 = true;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 NeverDistanceCull : 1 = false;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 CastFarShadow : 1 = false;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 CastInsetShadow : 1 = false;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 CastContactShadow : 1 = true;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 RenderCustomDepth : 1 = false;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 VisibleInReflections : 1 = true;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 VisibleInRayTracing : 1 = true;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 VisibleInReflectionCaptures : 1 = true;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	float MinDrawDistance = 0.f;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	float LDMaxDrawDistance = 0.f;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	ERendererStencilMask CustomDepthStencilWriteMask = ERendererStencilMask::ERSM_Default;
};

/**
 * Const shared fragment holding the editor visualization flags for a mesh
 */
USTRUCT()
struct FArcEditorVisualizationMeshFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

#if  WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 bShouldRenderSelected : 1 = false;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 bHiddenInEditor : 1 = false;
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	uint8 bLevelInstanceEditingState : 1 = false;
#endif //  WITH_EDITORONLY_DATA
};

/**
 * Const shared fragment holding the list of material override for a mesh
 */
USTRUCT()
struct FArcOverrideMaterialsFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Debug)
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
};

template<>
struct TMassFragmentTraits<FArcOverrideMaterialsFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

template<>
struct TMassFragmentTraits<FArcVisualizationMeshFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Fragment holding rendering information of all the different primitives
 */
USTRUCT(MinimalAPI)
struct FArcRenderPrimitiveFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient, VisibleAnywhere, Category = Debug)
	FBoxSphereBounds LocalBounds = {};
	UPROPERTY(Transient, VisibleAnywhere, Category = Debug)
	FBoxSphereBounds WorldBounds = {}; // Maybe not here...
	UPROPERTY(Transient, VisibleAnywhere, Category = Debug)
	FCustomPrimitiveData CustomPrimitiveData;
	UPROPERTY(Transient, VisibleAnywhere, Category = Debug)
	TEnumAsByte<enum EDetailMode> DetailMode = EDetailMode::DM_Low;
	UPROPERTY(Transient, VisibleAnywhere, Category = Debug)
	uint8 bIsVisible : 1 = true;
	UPROPERTY(Transient, VisibleAnywhere, Category = Debug)
	uint8 bRasterizeAsFilledConvexVolume : 1 = false;
	UPROPERTY(Transient, VisibleAnywhere, Category = Debug)
	TArray<TObjectPtr<URuntimeVirtualTexture>> RuntimeVirtualTextures;
};

template<>
struct TMassFragmentTraits<FArcRenderPrimitiveFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};


/**
 * Fragment holding the rendering information for a static mesh
 */
USTRUCT()
struct FArcRenderStaticMeshFragment : public FMassFragment
{
	GENERATED_BODY()

	UE_API FArcRenderStaticMeshFragment();
	UE_API explicit FArcRenderStaticMeshFragment(const TSharedRef<FStaticMeshSceneProxyDesc>& InStaticMeshSceneProxyDesc);

	UE_API UStaticMesh* GetStaticMesh();
	UE_API const UStaticMesh* GetStaticMesh() const;

	TSharedPtr<FStaticMeshSceneProxyDesc> StaticMeshSceneProxyDesc = nullptr;
};

template<>
struct TMassFragmentTraits<FArcRenderStaticMeshFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Render-side identity for a pickable Mass-entity instance.
 *
 * Deliberately holds NO refcounted FTypedElementHandle. Hit proxies are owned by the scene proxy
 * and cleaned up asynchronously on the render thread (FPendingCleanupObjects), so a stored handle
 * can outlive its element; releasing that stale handle's ref crashes (check(AllocationFlags) in
 * FTypedElementReferences::ReleaseRef once the element's internal data was Reset on destroy). The
 * engine avoids this the same way (HInstancedStaticMeshInstance stores Component+index, not a
 * handle). The handle is resolved on demand at click time via GetArcResolveTypeElementHandleDelegate().
 */
struct FArcTypeElementInstanceId
{
	/** Owning partition actor (UObject base — ArcMassRender must not name the concrete ArcMass actor). */
	TWeakObjectPtr<UObject> Owner;
	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;

	bool IsSet() const
	{
		return Owner.IsValid() && EntryIndex != INDEX_NONE && TransformIndex != INDEX_NONE;
	}
};

#if WITH_EDITOR
/**
 * Resolves a render-side FArcTypeElementInstanceId to a live FTypedElementHandle on demand.
 * ArcMass binds this at module startup (it owns the typed-element owner store); ArcMassRender may
 * not depend on ArcMass — that would close a module cycle — so the dependency is inverted here.
 */
DECLARE_DELEGATE_RetVal_OneParam(FTypedElementHandle, FArcResolveTypeElementHandleDelegate, const FArcTypeElementInstanceId&);
ARCMASSRENDER_API FArcResolveTypeElementHandleDelegate& GetArcResolveTypeElementHandleDelegate();

/**
 * Hit proxy to use along with the FArcTypeElementFragment
 */
struct ARCMASSRENDER_API HArcTypeElementHandleHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	HArcTypeElementHandleHitProxy(const FArcTypeElementInstanceId& InInstanceId) :
		HHitProxy(HPP_World),
		InstanceId(InInstanceId)
	{
	}

	HArcTypeElementHandleHitProxy(const FArcTypeElementInstanceId& InInstanceId, int32 InMaterialIndex, int32 InSectionIndex) :
		HHitProxy(HPP_World),
		InstanceId(InInstanceId),
		SectionIndex(InSectionIndex),
		MaterialIndex(InMaterialIndex)
	{
	}

	FArcTypeElementInstanceId InstanceId;
	int32 SectionIndex = -1;
	int32 MaterialIndex = -1;

	virtual bool AlwaysAllowsTranslucentPrimitives() const override
	{
		return false;
	}

	virtual FTypedElementHandle GetElementHandle() const override
	{
		const FArcResolveTypeElementHandleDelegate& Resolver = GetArcResolveTypeElementHandleDelegate();
		return Resolver.IsBound() ? Resolver.Execute(InstanceId) : FTypedElementHandle();
	}

	virtual EMouseCursor::Type GetMouseCursor() override;
};
#endif // WITH_EDITOR

/**
 * Fragment holding rendering information for a instanced static mesh
 */
USTRUCT()
struct ARCMASSRENDER_API FArcRenderISMFragment : public FMassFragment
{
	GENERATED_BODY()

	FArcRenderISMFragment();
	explicit FArcRenderISMFragment(const TSharedRef<FInstancedStaticMeshSceneProxyDesc>& InInstancedStaticMeshSceneProxyDesc);

	UStaticMesh* GetStaticMesh();
	const UStaticMesh* GetStaticMesh() const;

#if WITH_EDITOR
	TSparseArray<TRefCountPtr<HArcTypeElementHandleHitProxy>> PerInstanceHitProxy;
#endif // WITH_EDITOR
	TSparseArray<FInstancedStaticMeshInstanceData> PerInstanceSMData;

	TArray<float> PerInstanceSMCustomData;
	TArray<FInstancedStaticMeshRandomSeed> AdditionalRandomSeeds;

	TSharedPtr<FInstancedStaticMeshSceneProxyDesc> InstancedStaticMeshSceneProxyDesc = nullptr;
	int32 InstancingRandomSeed = 0;
};

template<>
struct TMassFragmentTraits<FArcRenderISMFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};


/**
 * Fragment holding the render-side identity used to mint a pickable typed element for the entity.
 * Stores the lightweight id (not a refcounted FTypedElementHandle) so it is safe to destruct on the
 * game thread after the element owner has been torn down — see FArcTypeElementInstanceId.
 */
USTRUCT()
struct FArcTypeElementFragment : public FMassFragment
{
	GENERATED_BODY()

	FArcTypeElementInstanceId InstanceId;
};

template<>
struct TMassFragmentTraits<FArcTypeElementFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Optional const shared fragment for ISM grouping scoping.
 * When present, its GroupId is included in the ISM hash, ensuring entities
 * with different GroupIds get separate ISM entities even if they share the
 * same mesh/vis flags. Use cases: per-cell scoping, per-level scoping, etc.
 */
USTRUCT()
struct FArcISMGroupingFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	FArcISMGroupingFragment() = default;
	explicit FArcISMGroupingFragment(const FGuid& InGroupId)
		: GroupId(InGroupId)
	{}

	UPROPERTY(VisibleAnywhere, Category = Debug)
	FGuid GroupId;
};

#undef UE_API
