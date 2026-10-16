// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcDestroyRenderStateContext.h"
#include "ArcMassRenderLogs.h"
#include "SceneProxies/DeferredDecalProxy.h"
#include "PrimitiveSceneProxy.h"
#include "SceneInterface.h"

FArcDestroyRenderStateContext::FArcDestroyRenderStateContext(FSceneInterface* InScene)
	: Scene(InScene)
{
}

FArcDestroyRenderStateContext::~FArcDestroyRenderStateContext()
{
	Process();
}

void FArcDestroyRenderStateContext::Process()
{
	if (!PrimitiveSceneProxies.IsEmpty())
	{
		checkf(Scene, TEXT("Nothing should have been queued up if the scene was not set"));
		UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] DestroyContext::Process: %d proxies -> BatchRemovePrimitives",
			PrimitiveSceneProxies.Num());
		Scene->BatchRemovePrimitives(MoveTemp(PrimitiveSceneProxies));
		PrimitiveSceneProxies.Reset();
	}
	if (!DeferredDecalProxies.IsEmpty())
	{
		checkf(Scene, TEXT("Nothing should have been queued up if the scene was not set"));
		TArray<FDeferredDecalUpdateParams> BatchParams;
		BatchParams.Reserve(DeferredDecalProxies.Num());
		for (FDeferredDecalProxy* Proxy : DeferredDecalProxies)
		{
			BatchParams.Add(
				{
					.DecalProxy = Proxy,
					.OperationType = FDeferredDecalUpdateParams::EOperationType::RemoveFromSceneAndDelete
				});
		}
		Scene->BatchUpdateDecals(MoveTemp(BatchParams));
		DeferredDecalProxies.Reset();
	}
}

void FArcDestroyRenderStateContext::DestroyProxy(FArcDestroyRenderStateContext* InContext, FPrimitiveSceneProxy* InPrimitiveSceneProxy, FSceneInterface* InScene)
{
	check(InScene);
	check(InPrimitiveSceneProxy);

	if (InContext)
	{
		checkf(InScene == InContext->Scene, TEXT("Expecting the scene to match the context one"));
		InContext->PrimitiveSceneProxies.Add(InPrimitiveSceneProxy);
	}
	else
	{
		UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] DestroyProxy: unbatched immediate remove proxy=%p", InPrimitiveSceneProxy);
		InScene->BatchRemovePrimitives({ InPrimitiveSceneProxy });
	}
}

void FArcDestroyRenderStateContext::DestroyProxy(FArcDestroyRenderStateContext* InContext, FDeferredDecalProxy* InDeferredDecalProxy, FSceneInterface* InScene)
{
	check(InScene);
	check(InDeferredDecalProxy);

	if (InContext)
	{
		checkf(InScene == InContext->Scene, TEXT("Expecting the scene to match the context one"));
		InContext->DeferredDecalProxies.Add(InDeferredDecalProxy);
	}
	else
	{
		TArray<FDeferredDecalUpdateParams> BatchParams;
		BatchParams.Add(
		{
			.DecalProxy = InDeferredDecalProxy,
			.OperationType = FDeferredDecalUpdateParams::EOperationType::RemoveFromSceneAndDelete
		});
		InScene->BatchUpdateDecals(MoveTemp(BatchParams));
	}
}
