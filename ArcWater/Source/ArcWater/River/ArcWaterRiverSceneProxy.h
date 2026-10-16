// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "LocalVertexFactory.h"
#include "MaterialShared.h"
#include "PrimitiveSceneProxy.h"
#include "StaticMeshResources.h"

class UArcWaterRiverComponent;

class FArcWaterRiverSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FArcWaterRiverSceneProxy(const UArcWaterRiverComponent* InComponent,
		TArray<FDynamicMeshVertex>&& InVertices, TArray<uint32>&& InIndices);

	//~ Begin FPrimitiveSceneProxy interface
	virtual SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual void DestroyRenderThreadResources() override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	//~ End FPrimitiveSceneProxy interface

private:
	FMaterialRenderProxy* Material = nullptr;
	FMaterialRelevance MaterialRelevance;

	// The ribbon is baked in component space and never displaced per frame, so it needs no counterpart to the ocean's wave vertex factory.
	FLocalVertexFactory VertexFactory;
	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;

	TArray<FDynamicMeshVertex> Vertices;
};
