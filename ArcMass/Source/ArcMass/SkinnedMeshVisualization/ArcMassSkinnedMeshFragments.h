// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "InstancedSkinnedMeshSceneProxyDesc.h"
#include "ArcMassSkinnedMeshFragments.generated.h"

class USkinnedAsset;
class UTransformProviderData;
class FSkeletalMeshObject;
class FInstanceDataSceneProxy;
class HHitProxy;
struct FArcMassInstancedSkinnedMeshRenderStateHelper;

/** Per-instance transform and animation index, stored in sparse array on the ISM holder. */
struct FArcSkinnedMeshInstanceData
{
	FTransform3f Transform;
	uint32 AnimationIndex = 0;
};

/** Const shared fragment — one per skinned mesh asset. Analogous to FMassStaticMeshFragment. */
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
 *  skinned mesh instances sharing the same asset. Analogous to FMassRenderISMFragment. */
USTRUCT()
struct ARCMASS_API FArcMassRenderISMSkinnedFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Per-instance transform and animation index. Sparse array for stable indices on removal. */
	TSparseArray<FArcSkinnedMeshInstanceData> PerInstanceData;

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
	 *  FMassRenderStateFragment to avoid the engine's render state processor pipeline. */
	TSharedPtr<FArcMassInstancedSkinnedMeshRenderStateHelper> RenderStateHelper;

	/** Set when instance data changes. Cleared after proxy recreation. */
	bool bRenderStateDirty = false;

#if WITH_EDITORONLY_DATA
	TSparseArray<TRefCountPtr<HHitProxy>> PerInstanceHitProxy;
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

