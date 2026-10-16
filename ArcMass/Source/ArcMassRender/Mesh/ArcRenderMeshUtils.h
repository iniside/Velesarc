// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Misc/NotNull.h"
#include "UObject/ObjectPtr.h"

struct FPrimitiveSceneProxyDesc;
struct FInstancedStaticMeshSceneProxyDesc;
struct FArcEditorVisualizationMeshFragment;
struct FArcVisualizationMeshFragment;
struct FArcStaticMeshFragment;
struct FStaticMeshSceneProxyDesc;
class UActorComponent;
struct FTransformFragment;
struct FArcRenderPrimitiveFragment;
struct FArcRenderStaticMeshFragment;
struct FArcRenderISMFragment;
#define UE_API ARCMASSRENDER_API

namespace UE::ArcMassRender::Mesh
{
	/**
	 * Initializes the transform, primitive fragments from a UActorComponent
	 * @param Component to initialize the fragments from
	 * @param TransformFragment to set up
	 * @param RenderPrimitiveFragment to set up
	 */
	UE_API void InitializePrimitiveFragmentsFromComponent(TNotNull<const UActorComponent*> Component, FTransformFragment& TransformFragment, FArcRenderPrimitiveFragment& RenderPrimitiveFragment);

	/**
	 * Initializes the render transform, primitive, render static mesh fragments from a UActorComponent
	 * @param Component to initialize the fragments from
	 * @param TransformFragment the transform fragment to set up
	 * @param RenderPrimitiveFragment the render primitive fragment to set up
	 * @param RenderStaticMeshFragment the render static mesh fragment to set up
	 */
	UE_API void InitializeStaticMeshFragmentsFromComponent(TNotNull<const UActorComponent*> Component, FTransformFragment& TransformFragment, FArcRenderPrimitiveFragment& RenderPrimitiveFragment, FArcRenderStaticMeshFragment& RenderStaticMeshFragment);

	/**
	 * Initializes the render transform, primitive, render instanced static mesh fragments from a UActorComponent
	 * @param Component to initialize the fragments from
	 * @param TransformFragment the transform fragment to set up
	 * @param RenderPrimitiveFragment the render primitive fragment to set up
	 * @param RenderISMFragment the render instanced static mesh fragment to set up
	 */
	UE_API void InitializeISMFragmentsFromComponent(TNotNull<const UActorComponent*> Component, FTransformFragment& TransformFragment, FArcRenderPrimitiveFragment& RenderPrimitiveFragment, FArcRenderISMFragment& RenderISMFragment);

	enum class EBoundsType
	{
		LocalBounds,
		WorldBounds,
		NavigationBounds
	};

	/**
	 * Calculates the bounds of the instanced static mesh from the mass fragments
	 * @param TransformFragment Transform to use as a root of the ISM
	 * @param RenderISMFragment List of instances transform of the ISM
	 * @param BoundsType Bounds type to calculate
	 * @return
	 */
	UE_API FBoxSphereBounds CalculateInstancedStaticMeshBounds(const FTransformFragment& TransformFragment, const FArcRenderISMFragment& RenderISMFragment, const EBoundsType BoundsType);

#if WITH_EDITOR
	/**
	 * Initialize the scene proxy desc from the editor mesh fragments
	 * @param EditorMeshFragment fragment describing the editor mesh parameters
	 * @param PrimitiveSceneProxyDesc scene proxy desc to initialize
	 */
	void InitializePrimitiveSceneProxyDescFromEditorFragment(const FArcEditorVisualizationMeshFragment& EditorMeshFragment, FPrimitiveSceneProxyDesc& PrimitiveSceneProxyDesc);
#endif // WITH_EDITOR

	/**
	 * Initialize the static mesh scene proxy desc from mass fragments
	 * @param StaticMeshFragment fragment describing the static mesh
	 * @param MeshFragment fragment describing the mesh parameters
	 * @param TransformFragment fragment specifying the transform
	 * @param StaticMeshSceneProxyDesc scene proxy desc to initialize
	 */
	UE_API void InitializeStaticMeshSceneProxyDescFromFragment(const FArcStaticMeshFragment& StaticMeshFragment, const FArcVisualizationMeshFragment& MeshFragment, const FTransformFragment& TransformFragment, FStaticMeshSceneProxyDesc& StaticMeshSceneProxyDesc);

	/**
	 * Initialize the instanced static mesh scene proxy desc from mass fragments
	 * @param StaticMeshFragment fragment describing the static mesh
	 * @param MeshFragment fragment describing the mesh parameters
	 * @param TransformFragment fragment specifying the transform
	 * @param ISMSceneProxyDesc scene proxy desc to initialize
	 */
	UE_API void InitializeInstanceStaticMeshSceneProxyDescFromFragment(const FArcStaticMeshFragment& StaticMeshFragment, const FArcVisualizationMeshFragment& MeshFragment, const FTransformFragment& TransformFragment, FInstancedStaticMeshSceneProxyDesc& ISMSceneProxyDesc);

	/**
	 * Update only the transform-dependent fields of a static mesh scene proxy desc.
	 * Shadow/lighting/visibility flags come from the const-shared MeshFragment and
	 * never change on a transform-only signal — no need to rewrite them.
	 * In non-editor builds this is effectively a no-op; the per-instance world
	 * transform is carried through the primitive's UpdateTransform path, not the desc.
	 * @param TransformFragment fragment specifying the new transform
	 * @param OutDesc scene proxy desc to update
	 */
	UE_API void UpdateStaticMeshSceneProxyDescTransform(const FTransformFragment& TransformFragment, FStaticMeshSceneProxyDesc& OutDesc);

}

#undef UE_API
