// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "PrimitiveSceneProxy.h"

#include "Ocean/ArcOceanComponent.h"
#include "Ocean/ArcOceanRingGeometry.h"
#include "Ocean/ArcOceanVertexFactory.h"

class FArcOceanSceneProxy final : public FPrimitiveSceneProxy
{
public:
	explicit FArcOceanSceneProxy(const UArcOceanComponent* InComponent);

	//~ Begin FPrimitiveSceneProxy interface
	virtual SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual void DestroyRenderThreadResources() override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual bool CanBeOccluded() const override;
	//~ End FPrimitiveSceneProxy interface

	/** The master copy of the uniform buffer lives here and fans out to three vertex factories, so a
	 *  field-wise setter on the factory would leave this copy stale and the next push would undo it. */
	void SetWaveFields_RenderThread(FRHICommandListBase& RHICmdList, const ArcOcean::FArcOceanWaveFields& InFields);
	void SetSeabedFields_RenderThread(FRHICommandListBase& RHICmdList, const FArcOceanSeabedFields& InFields);
	void SetFoamFields_RenderThread(FRHICommandListBase& RHICmdList, const FArcOceanFoamFields& InFields);

	const FArcOceanVFParameters& GetShaderParams_RenderThread() const { return WaveParams; }

	float GetBaseCellSizeCm() const { return BaseCellSize; }

	float GetRingMorphSpanCells() const
	{
		return static_cast<float>(FMath::Max(1, RingGeometry.GetHoleMin() + RingGeometry.GetHalfExtent()));
	}

private:
	void PushParamsToFactories(FRHICommandListBase& RHICmdList);

	void AddRingBatch(FMeshElementCollector& Collector, int32 ViewIndex, const FSceneViewFamily& ViewFamily,
		const FArcOceanRingMesh& RingMesh, const FArcOceanVertexFactory& RingVertexFactory,
		float CellSize, const FVector2f& OriginTranslatedWorld, uint32 TrimRotationSteps) const;

	FMaterialRenderProxy* Material = nullptr;
	FMaterialRelevance MaterialRelevance;

	FArcOceanRingGeometry RingGeometry;
	FArcOceanVertexFactory FullGridVertexFactory;
	FArcOceanVertexFactory DonutVertexFactory;
	FArcOceanVertexFactory TrimVertexFactory;

	FArcOceanVFParameters WaveParams;

	float BaseCellSize = 100.0f;
	int32 RingCount = 1;
};
