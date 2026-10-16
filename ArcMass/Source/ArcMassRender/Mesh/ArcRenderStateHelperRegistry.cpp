// Copyright Lukasz Baran. All Rights Reserved.

#include "Mesh/ArcRenderStateHelperRegistry.h"

#include "Components/ComponentInterfaces.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Materials/MaterialInterface.h"
#include "PrimitiveSceneProxy.h"
#include "Rendering/StaticLightingSystemInterface.h"
#include "RenderingThread.h"
#include "SceneInterface.h"
#endif

namespace ArcRenderStateHelperRegistry_Private
{
	/** Map from live componentless-primitive pointer to the world it was registered for. */
	static TMap<IPrimitiveComponent*, TWeakObjectPtr<UWorld>> GRegisteredHelpers;

#if WITH_EDITOR
	/** Render-thread-only: material render proxy -> scene proxies of our helpers rendering it. */
	static TMap<const FMaterialRenderProxy*, TSet<FPrimitiveSceneProxy*>> GMaterialToSceneProxies;
	/** Render-thread-only reverse map for cleanup. */
	static TMap<FPrimitiveSceneProxy*, TArray<const FMaterialRenderProxy*>> GSceneProxyToMaterials;

	static FDelegateHandle GMaterialInvalidatedHandle;

	static void RequestStaticMeshUpdates_RenderThread(const FMaterialRenderProxy* MaterialProxy)
	{
		const TSet<FPrimitiveSceneProxy*>* SceneProxies = GMaterialToSceneProxies.Find(MaterialProxy);
		if (SceneProxies == nullptr)
		{
			return;
		}

		for (FPrimitiveSceneProxy* SceneProxy : *SceneProxies)
		{
			// In-place heal: flags the primitive for a remove + re-cache of its mesh draw commands
			// and Nanite material bins in the next UpdateAllPrimitiveSceneInfos, before rendering.
			SceneProxy->GetScene().UpdateCachedRenderStates(SceneProxy);
		}
	}

	// Signature matches FStaticLightingSystemInterface::FMaterialInvalidationSignature (non-const param).
	static void OnMaterialInvalidated(FMaterialRenderProxy* MaterialProxy)
	{
		if (IsInRenderingThread())
		{
			RequestStaticMeshUpdates_RenderThread(MaterialProxy);
		}
		else
		{
			ENQUEUE_RENDER_COMMAND(ArcMaterialInvalidatedStaticMeshUpdate)(
				[MaterialProxy](FRHICommandListImmediate&)
				{
					RequestStaticMeshUpdates_RenderThread(MaterialProxy);
				});
		}
	}
#endif // WITH_EDITOR
}

void ArcRenderStateHelperRegistry::Register(IPrimitiveComponent* Primitive, UWorld* World)
{
	check(IsInGameThread());

	if (!Primitive || !World)
	{
		return;
	}

	ArcRenderStateHelperRegistry_Private::GRegisteredHelpers.Add(Primitive, TWeakObjectPtr<UWorld>(World));
}

void ArcRenderStateHelperRegistry::Unregister(IPrimitiveComponent* Primitive)
{
	check(IsInGameThread());

	ArcRenderStateHelperRegistry_Private::GRegisteredHelpers.Remove(Primitive);
}

void ArcRenderStateHelperRegistry::ResolveForWorld(const UWorld* World, TArray<IPrimitiveComponent*>& Out)
{
	check(IsInGameThread());

	for (TPair<IPrimitiveComponent*, TWeakObjectPtr<UWorld>>& Pair : ArcRenderStateHelperRegistry_Private::GRegisteredHelpers)
	{
		UWorld* ResolvedWorld = Pair.Value.Get();
		if (ResolvedWorld == nullptr || ResolvedWorld != World)
		{
			continue;
		}

		Out.Add(Pair.Key);
	}
}

#if WITH_EDITOR

void ArcRenderStateHelperRegistry::TrackProxyMaterials(IPrimitiveComponent* Primitive)
{
	check(IsInGameThread());

	if (Primitive == nullptr)
	{
		return;
	}

	FPrimitiveSceneProxy* SceneProxy = Primitive->GetSceneProxy();
	if (SceneProxy == nullptr)
	{
		return;
	}

	TArray<UMaterialInterface*> UsedMaterials;
	Primitive->GetUsedMaterials(UsedMaterials);

	TArray<const FMaterialRenderProxy*> MaterialProxies;
	MaterialProxies.Reserve(UsedMaterials.Num());
	for (UMaterialInterface* Material : UsedMaterials)
	{
		if (Material == nullptr)
		{
			continue;
		}
		const FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy();
		if (MaterialProxy != nullptr)
		{
			MaterialProxies.AddUnique(MaterialProxy);
		}
	}

	if (MaterialProxies.Num() == 0)
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(ArcTrackProxyMaterials)(
		[SceneProxy, MaterialProxies = MoveTemp(MaterialProxies)](FRHICommandListImmediate&)
		{
			using namespace ArcRenderStateHelperRegistry_Private;
			for (const FMaterialRenderProxy* MaterialProxy : MaterialProxies)
			{
				GMaterialToSceneProxies.FindOrAdd(MaterialProxy).Add(SceneProxy);
			}
			GSceneProxyToMaterials.Add(SceneProxy, MaterialProxies);
		});
}

void ArcRenderStateHelperRegistry::UntrackProxyMaterials(FPrimitiveSceneProxy* SceneProxy)
{
	check(IsInGameThread());

	if (SceneProxy == nullptr)
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(ArcUntrackProxyMaterials)(
		[SceneProxy](FRHICommandListImmediate&)
		{
			using namespace ArcRenderStateHelperRegistry_Private;
			TArray<const FMaterialRenderProxy*> MaterialProxies;
			if (GSceneProxyToMaterials.RemoveAndCopyValue(SceneProxy, MaterialProxies))
			{
				for (const FMaterialRenderProxy* MaterialProxy : MaterialProxies)
				{
					TSet<FPrimitiveSceneProxy*>* SceneProxies = GMaterialToSceneProxies.Find(MaterialProxy);
					if (SceneProxies != nullptr)
					{
						SceneProxies->Remove(SceneProxy);
						if (SceneProxies->Num() == 0)
						{
							GMaterialToSceneProxies.Remove(MaterialProxy);
						}
					}
				}
			}
		});
}

void ArcRenderStateHelperRegistry::StartMaterialInvalidationWatcher()
{
	using namespace ArcRenderStateHelperRegistry_Private;
	if (!GMaterialInvalidatedHandle.IsValid())
	{
		GMaterialInvalidatedHandle = FStaticLightingSystemInterface::OnMaterialInvalidated.AddStatic(&OnMaterialInvalidated);
	}
}

void ArcRenderStateHelperRegistry::StopMaterialInvalidationWatcher()
{
	using namespace ArcRenderStateHelperRegistry_Private;
	if (GMaterialInvalidatedHandle.IsValid())
	{
		FStaticLightingSystemInterface::OnMaterialInvalidated.Remove(GMaterialInvalidatedHandle);
		GMaterialInvalidatedHandle.Reset();
	}
}

#endif // WITH_EDITOR
