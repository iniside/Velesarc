// Copyright Lukasz Baran. All Rights Reserved.

#include "Lighting/ArcTerrainFarOcclusionComponent.h"

#include "Lighting/ArcTerrainFarOcclusionProxy.h"

#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

UArcTerrainFarOcclusionComponent::UArcTerrainFarOcclusionComponent()
{
	Mobility = EComponentMobility::Movable;
	SetGenerateOverlapEvents(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bRayTracingFarField = true;
	// Stays true although the proxy's view relevance says otherwise: FRayTracingMeshProcessor::
	// TryAddMeshBatch gates on Proxy->ShouldRenderInMainPass(), which reads THIS field.
	bRenderInMainPass = true;
}

void UArcTerrainFarOcclusionComponent::SetMesh(ArcTerrainLightingMesh::FMesh&& InMesh)
{
	Mesh = MoveTemp(InMesh);
	UpdateBounds();
	MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UArcTerrainFarOcclusionComponent::CreateSceneProxy()
{
	if (Mesh.Indices.IsEmpty() || Mesh.Positions.IsEmpty() || !Mesh.LocalBounds.IsValid)
	{
		return nullptr;
	}
	return new FArcTerrainFarOcclusionProxy(this);
}

FBoxSphereBounds UArcTerrainFarOcclusionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (!Mesh.LocalBounds.IsValid)
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0);
	}
	return FBoxSphereBounds(FBox(Mesh.LocalBounds).TransformBy(LocalToWorld));
}

UMaterialInterface* UArcTerrainFarOcclusionComponent::GetMaterial(int32 ElementIndex) const
{
	return ElementIndex == 0 ? UMaterial::GetDefaultMaterial(MD_Surface) : nullptr;
}

void UArcTerrainFarOcclusionComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials,
	bool bGetDebugMaterials) const
{
	OutMaterials.Add(UMaterial::GetDefaultMaterial(MD_Surface));
}
