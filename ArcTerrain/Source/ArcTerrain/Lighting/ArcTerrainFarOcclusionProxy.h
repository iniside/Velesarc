// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "Lighting/ArcTerrainLightingMesh.h"
#include "LocalVertexFactory.h"
#include "MaterialShared.h"
#include "PrimitiveSceneProxy.h"
#include "StaticMeshResources.h"

#if RHI_RAYTRACING
#include "RayTracingGeometry.h"
#include "RayTracingInstance.h"
#endif

class UArcTerrainFarOcclusionComponent;

/**
 * Occlusion-only scene proxy for one far block: a static FLocalVertexFactory mesh whose whole purpose is
 * the BLAS in the ray tracing FarField layer. It has no view relevance at all — no raster pass, no
 * shadow pass, no card capture.
 */
class FArcTerrainFarOcclusionProxy final : public FPrimitiveSceneProxy
{
public:
	explicit FArcTerrainFarOcclusionProxy(UArcTerrainFarOcclusionComponent* InComponent);

	//~ Begin FPrimitiveSceneProxy interface
	virtual SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual void DestroyRenderThreadResources() override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override { return false; }

#if RHI_RAYTRACING
	virtual bool HasRayTracingRepresentation() const override { return !RayTracingGeometryGroupHandle.IsNull(); }
	virtual RayTracing::FGeometryGroupHandle GetRayTracingGeometryGroupHandle() const override { return RayTracingGeometryGroupHandle; }
	virtual ERayTracingPrimitiveFlags GetRayTracingPrimitiveFlags() override;
	virtual void GetStaticRayTracingInstances(FStaticRayTracingInstances& OutRayTracingInstances) override;
#endif
	//~ End FPrimitiveSceneProxy interface

private:
	const FMaterialRenderProxy* Material = nullptr;

	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;

	/** Latched on the game thread; released once CreateRenderThreadResources has uploaded it. */
	ArcTerrainLightingMesh::FMesh Mesh;

	uint32 NumVertices = 0;
	uint32 NumPrimitives = 0;

#if RHI_RAYTRACING
	FRayTracingGeometry RayTracingGeometry;
	RayTracing::FGeometryGroupHandle RayTracingGeometryGroupHandle;
	bool bReleasedRayTracingGeometryGroup = false;
#endif
};
