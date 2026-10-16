// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "PrimitiveSceneProxy.h"
#include "Render/ArcTerrainRenderComponent.h"
#include "Render/ArcTerrainVertexFactory.h"

#include <atomic>

/**
 * Scene proxy for the whole terrain: owns the terrain vertex factories and per-face parameter buffers,
 * and emits one dynamic FMeshBatch per view.
 *
 * The TRIANGULATION is view-independent — the bisector tree is driven by the main view alone — but the
 * DRAWN SET is not: the view the frame's compaction culled for binds the compacted args and list, and
 * every other view binds the args over the whole live set. Sharing one compacted block would cull a
 * shadow caster standing behind the camera and take its shadow off the ground that is on screen.
 */
class FArcTerrainSceneProxy final : public FPrimitiveSceneProxy
{
public:
	explicit FArcTerrainSceneProxy(UArcTerrainRenderComponent* InComponent);

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

	/** Render-thread side of UArcTerrainRenderComponent::UpdateBindings. */
	void UpdateBindings_RenderThread(FRHICommandListBase& RHICmdList, FArcTerrainRenderBindings&& InBindings);

private:
	struct FDrawSlot
	{
		explicit FDrawSlot(ERHIFeatureLevel::Type FeatureLevel, uint8 InSlotIndex)
			: VertexFactory(FeatureLevel)
			, SlotIndex(InSlotIndex)
		{
		}

		FArcTerrainVertexFactory VertexFactory;
		FArcTerrainRenderBindings Bindings;
		FBufferRHIRef FaceParamsBuffer;
		FShaderResourceViewRHIRef FaceParamsSRV;
		FBufferRHIRef HeightRingBuffer;
		FShaderResourceViewRHIRef HeightRingSRV;
		FBufferRHIRef WeightRingBuffer;
		FShaderResourceViewRHIRef WeightRingSRV;
		uint8 SlotIndex = MAX_uint8;
		bool bValid = false;
		bool bBindingSourceRegistered = false;
	};

	void ApplyBindings_RenderThread(FRHICommandListBase& RHICmdList, FArcTerrainRenderBindings&& InBindings);
	bool BuildSlot_RenderThread(FRHICommandListBase& RHICmdList, FDrawSlot& Slot,
		FArcTerrainRenderBindings&& InBindings);
	void InvalidateSlot_RenderThread(FDrawSlot& Slot);
	void InvalidateAllSlots_RenderThread();
	FDrawSlot* FindSlot_RenderThread(const FArcTerrainRenderIdentity& Identity);
	const FDrawSlot* FindSlot_RenderThread(const FArcTerrainRenderIdentity& Identity) const;
	FArcTerrainCBTBindingSourceToken GetSourceToken(const FDrawSlot& Slot) const;

	FMaterialRenderProxy* Material = nullptr;
	FMaterialRelevance MaterialRelevance;

	const uint64 ProxySerial;
	FDrawSlot DrawSlot0;
	FDrawSlot DrawSlot1;
	FArcTerrainRenderBindings PendingBindings;
	bool bRenderResourcesCreated = false;

	/** One-shot "the terrain actually drew" diagnostic. */
	mutable std::atomic<uint64> FirstDrawOwnerEpoch { 0 };

	/** False until the first ApplyBindings_RenderThread logged its uniforms, which it does whether or not
	 *  the verbose cvar is on. */
	bool bLoggedWeightUniformsOnce = false;

	/** Last weight-addressing uniform set logged under arc.Terrain.Weights.VerboseComposite. Sentinels
	 *  no live value can take, so the first apply always logs. */
	struct FLoggedWeightUniforms
	{
		FIntPoint RingCenter = FIntPoint(MIN_int32, MIN_int32);
		int32 RingRadius = MIN_int32;
		int32 RingSliceCount = MIN_int32;
		uint32 NumTiers = MAX_uint32;
		uint32 FillSlice = MAX_uint32;
		int32 LayoutFillSlice = MIN_int32;
		int32 WeightQuadsShift = MIN_int32;
		uint32 SectorQuads = MAX_uint32;
		int32 SectorQuadsShift = MIN_int32;
		uint32 bResourcesValid = MAX_uint32;

		bool operator==(const FLoggedWeightUniforms& Other) const
		{
			return RingCenter == Other.RingCenter && RingRadius == Other.RingRadius
				&& RingSliceCount == Other.RingSliceCount && NumTiers == Other.NumTiers
				&& FillSlice == Other.FillSlice
				&& LayoutFillSlice == Other.LayoutFillSlice && WeightQuadsShift == Other.WeightQuadsShift
				&& SectorQuads == Other.SectorQuads && SectorQuadsShift == Other.SectorQuadsShift
				&& bResourcesValid == Other.bResourcesValid;
		}
	};
	FLoggedWeightUniforms LastLoggedWeightUniforms;
};
