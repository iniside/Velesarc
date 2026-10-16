// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "InstancedSkinnedMeshSceneProxyDesc.h"

// HArcTypeElementHandleHitProxy lives in ArcMassRender. Full include required (not just a
// forward-decl) because TRefCountPtr<HArcTypeElementHandleHitProxy> needs a complete type for
// its destructor. ArcMass already depends on ArcMassRender so this adds no new module dep.
#include "Mesh/ArcRenderMeshFragments.h"

#include "ArcMassSkinnedMeshFragments.generated.h"

class USkinnedAsset;
class UTransformProviderData;
class FSkeletalMeshObject;
class FInstanceDataSceneProxy;
struct FArcMassInstancedSkinnedMeshRenderStateHelper;

/** Per-instance transform and animation index, stored in a dense array on the ISM holder. */
struct FArcSkinnedMeshInstanceData
{
	FTransform3f Transform;
	uint32 AnimationIndex = 0;
};

/** Const shared fragment — one per skinned mesh asset. Analogous to FArcStaticMeshFragment. */
USTRUCT()
struct ARCMASS_API FArcMassSkinnedMeshFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USkinnedAsset> SkinnedAsset;

	UPROPERTY()
	TObjectPtr<UTransformProviderData> TransformProvider = nullptr;
};

/** Per-entity fragment on ISM holder entities. Stores all instance data for a group of
 *  skinned mesh instances sharing the same asset. Analogous to FArcRenderISMFragment. */
USTRUCT()
struct ARCMASS_API FArcMassRenderISMSkinnedFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Per-instance transform and animation index. Dense array; stable render-thread identity is
	 *  provided by FInstanceDataManager + FArcMassInstancedSkinnedMeshRenderStateHelper::InstanceIds,
	 *  not by container slots. Removal is swap-remove (mirrors UInstancedSkinnedMeshComponent). */
	TArray<FArcSkinnedMeshInstanceData> PerInstanceData;

	/** Per-instance custom shader floats, packed contiguously (NumCustomDataFloats per instance). */
	TArray<float> PerInstanceCustomData;

	/** Scene proxy descriptor, initialized from shared fragments. */
	TSharedPtr<FInstancedSkinnedMeshSceneProxyDesc> SceneProxyDesc;

	/** Mesh object — created once from descriptor, owned by this fragment.
	 *  Preserved across proxy recreations. Deleted only when holder entity is destroyed. */
	FSkeletalMeshObject* MeshObject = nullptr;

	/** Built instance data proxy for the current frame. */
	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> InstanceDataSceneProxy;

	int32 NumCustomDataFloats = 0;

	/** Render state helper — owns scene proxy lifecycle. Stored here instead of
	 *  FArcRenderStateFragment to avoid the engine's render state processor pipeline. */
	TSharedPtr<FArcMassInstancedSkinnedMeshRenderStateHelper> RenderStateHelper;

	/** Set when instance data changes. Cleared after proxy recreation. */
	bool bRenderStateDirty = false;

#if WITH_EDITORONLY_DATA
	// Retyped from HHitProxy to HArcTypeElementHandleHitProxy for parity with FArcRenderISMFragment
	// and to allow the change-set builder to call ->Id.GetColor() without a cast. Dense array kept
	// parallel to PerInstanceData (swap-removed in lockstep by the callers).
	// Full type provided by the unconditional "Mesh/ArcRenderMeshFragments.h" include above.
	TArray<TRefCountPtr<HArcTypeElementHandleHitProxy>> PerInstanceHitProxy;
#endif
};

template<>
struct TMassFragmentTraits<FArcMassRenderISMSkinnedFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

