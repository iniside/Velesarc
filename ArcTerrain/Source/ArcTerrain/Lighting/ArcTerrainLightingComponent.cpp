// Copyright Lukasz Baran. All Rights Reserved.

#include "Lighting/ArcTerrainLightingComponent.h"

#include "ArcTerrainModule.h"
#include "Lighting/ArcTerrainLightingProxy.h"

#include "Materials/MaterialInterface.h"
#include "PrimitiveSceneProxy.h"

UArcTerrainLightingComponent::UArcTerrainLightingComponent()
{
	Mobility = EComponentMobility::Movable;
	SetGenerateOverlapEvents(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UArcTerrainLightingComponent::SetMesh(ArcTerrainLightingMesh::FMesh&& InMesh)
{
	Mesh = MoveTemp(InMesh);
	UpdateBounds();
	MarkRenderStateDirty();
	MarkRenderTransformDirty();
}

void UArcTerrainLightingComponent::OnRegister()
{
	Super::OnRegister();

	const int32 GroupId = GetRayTracingGroupId();
	UE_CLOGF(GroupId != FPrimitiveSceneProxy::InvalidRayTracingGroupId, LogArcTerrain, Warning,
		"[Lighting] '%ls' inherited RayTracingGroupId %d from its actor. Lumen takes grouped primitives "
		"down the merge branch, which sets IsHeightfield(false), so this cell's card never appears. "
		"Clear RayTracingGroupId on the owning actor.",
		*GetPathName(), GroupId);
}

FPrimitiveSceneProxy* UArcTerrainLightingComponent::CreateSceneProxy()
{
	if (Mesh.Indices.IsEmpty() || Mesh.Positions.IsEmpty() || !Mesh.LocalBounds.IsValid)
	{
		return nullptr;
	}
	return new FArcTerrainLightingProxy(this);
}

FBoxSphereBounds UArcTerrainLightingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (!Mesh.LocalBounds.IsValid)
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0);
	}
	return FBoxSphereBounds(FBox(Mesh.LocalBounds).TransformBy(LocalToWorld));
}

void UArcTerrainLightingComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial)
{
	if (ElementIndex == 0 && Material != InMaterial)
	{
		Material = InMaterial;
		MarkRenderStateDirty();
	}
}

UMaterialInterface* UArcTerrainLightingComponent::GetMaterial(int32 ElementIndex) const
{
	return ElementIndex == 0 ? Material : nullptr;
}

void UArcTerrainLightingComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}
