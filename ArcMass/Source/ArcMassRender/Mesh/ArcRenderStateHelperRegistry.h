// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Forward declarations
class IPrimitiveComponent;
class UWorld;
class FPrimitiveSceneProxy;

/**
 * Game-thread registry of live componentless IPrimitiveComponent implementers.
 *
 * Every componentless primitive with an active scene proxy registers itself here at
 * CreateRenderState time and unregisters at DestroyRenderState / destruction time — the ArcMass
 * FArcPrimitiveRenderStateHelper family, and ArcTerrain's Nanite cell primitives
 * which reuse this registry instead of forking the editor rails.
 * The module-startup CollectionResolver registration (ArcMassRenderModule.cpp) feeds these
 * pointers into the engine's IPrimitiveComponent collection resolver so that
 * FMaterialUpdateContext's UpdateStaticMeshesForMaterials
 * sweep can reach our helpers and re-cache Nanite material bins in place — matching the
 * same sweep path that keeps UPrimitiveComponent instances coherent.
 *
 * Thread safety: NOT thread-safe.  All functions check(IsInGameThread()).
 * CreateRenderState/DestroyRenderState call sites are game-thread-only (editor actors +
 * GT Mass processors), so no locking is needed.
 */
namespace ArcRenderStateHelperRegistry
{
	/** Register Primitive with its owning World.  No-op if Primitive or World is null. */
	ARCMASSRENDER_API void Register(IPrimitiveComponent* Primitive, UWorld* World);

	/** Remove Primitive from the registry.  Idempotent — safe to call on an unregistered pointer. */
	ARCMASSRENDER_API void Unregister(IPrimitiveComponent* Primitive);

	/**
	 * Append all registered helpers whose stored world resolves and equals World into Out.
	 * Helpers with an expired or mismatched world are skipped; null pointers are never appended
	 * (the engine iterator check(Interface)s every entry).
	 */
	ARCMASSRENDER_API void ResolveForWorld(const UWorld* World, TArray<IPrimitiveComponent*>& Out);

#if WITH_EDITOR
	/**
	 * Material-invalidation watcher: bounds Nanite shading-bin staleness to <=1 frame, matching
	 * the guarantee components get. FMaterialRenderProxy::InvalidateUniformExpressionCache can
	 * recreate a material's RHI uniform buffers at any time (usage-flag recompiles, async shader
	 * map swaps, parameter edits); cached Nanite shading pipelines hold the buffer pointer raw.
	 * Components stay healthy because engine notifications refresh their cached draw data within
	 * a frame and the RHI deferred-delete queue keeps the old buffer's memory valid meanwhile.
	 * The watcher gives our componentless proxies the same bound: it maps each used material's
	 * render proxy to the scene proxies rendering it and requests an in-place static-mesh update
	 * (remove + re-cache of mesh draw commands and Nanite material bins) the moment the material
	 * invalidates — via FStaticLightingSystemInterface::OnMaterialInvalidated, which despite its
	 * home fires for every editor-build invalidation regardless of static lighting.
	 */

	/** Record Primitive's scene proxy against the render proxies of all its used materials. Game thread; call after CreateRenderState succeeds. */
	ARCMASSRENDER_API void TrackProxyMaterials(IPrimitiveComponent* Primitive);

	/** Remove SceneProxy from the material tracking map. Game thread; call before the proxy's scene removal is enqueued. */
	ARCMASSRENDER_API void UntrackProxyMaterials(FPrimitiveSceneProxy* SceneProxy);

	/** Subscribe/unsubscribe the OnMaterialInvalidated handler. Module startup/shutdown. */
	ARCMASSRENDER_API void StartMaterialInvalidationWatcher();
	ARCMASSRENDER_API void StopMaterialInvalidationWatcher();
#endif // WITH_EDITOR
}
