// Copyright Lukasz Baran. All Rights Reserved.

#include "Render/ArcTerrainRenderComponent.h"

#include "Render/ArcTerrainSceneProxy.h"

#include "Materials/MaterialInterface.h"
#include "RenderingThread.h"

UArcTerrainRenderComponent::UArcTerrainRenderComponent()
{
	Mobility = EComponentMobility::Movable;
	CastShadow = true;
	bCastDynamicShadow = true;
}

void UArcTerrainRenderComponent::UpdateBindings(FArcTerrainRenderBindings&& InBindings)
{
	// Residency changes rarely; the rail runs every tick. Rebuilding the face-params RHI buffer on an
	// unchanged table would churn the GPU allocator once per frame per terrain.
	if (Bindings == InBindings)
	{
		return;
	}

	const bool bBoundsChanged = Bindings.LocalBounds != InBindings.LocalBounds;
	Bindings = MoveTemp(InBindings);

	if (SceneProxy != nullptr)
	{
		FArcTerrainSceneProxy* TypedProxy = static_cast<FArcTerrainSceneProxy*>(SceneProxy);
		FArcTerrainRenderBindings Captured = Bindings;
		ENQUEUE_RENDER_COMMAND(ArcTerrainUpdateRenderBindings)(
			[TypedProxy, Captured = MoveTemp(Captured)](FRHICommandListImmediate& RHICmdList) mutable
			{
				TypedProxy->UpdateBindings_RenderThread(RHICmdList, MoveTemp(Captured));
			});
	}

	if (bBoundsChanged)
	{
		UpdateBounds();
		MarkRenderTransformDirty();
	}
}

FPrimitiveSceneProxy* UArcTerrainRenderComponent::CreateSceneProxy()
{
	if (!Bindings.IsValid())
	{
		return nullptr;
	}
	return new FArcTerrainSceneProxy(this);
}

FBoxSphereBounds UArcTerrainRenderComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (!Bindings.LocalBounds.IsValid)
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector(1.0), 1.0);
	}
	return FBoxSphereBounds(Bindings.LocalBounds.TransformBy(LocalToWorld));
}

void UArcTerrainRenderComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial)
{
	if (ElementIndex == 0 && Material != InMaterial)
	{
		Material = InMaterial;
		MarkRenderStateDirty(); // a recreated proxy re-resolves the material (incl. the usage gate)
	}
}

UMaterialInterface* UArcTerrainRenderComponent::GetMaterial(int32 ElementIndex) const
{
	return ElementIndex == 0 ? Material : nullptr;
}

void UArcTerrainRenderComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}
