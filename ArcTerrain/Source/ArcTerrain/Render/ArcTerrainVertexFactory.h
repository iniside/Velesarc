// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "VertexFactory.h"
#include "SceneManagement.h"

class FShaderParameterMap;
struct FShaderCompilerEnvironment;

/**
 * Terrain-wide uniform buffer for the ArcTerrain vertex factory ("ArcTerrainVF" in HLSL).
 * Multi-frame; rebuilt whenever atlas residency or layout changes.
 *
 * Slice convention: a height slice is one SECTOR, (SectorQuads + 3)^2 texels — the (SectorQuads + 1)
 * inner texels plus a 1-texel apron on every side; a weight slice is one weight group.
 *
 * THE FACE AND THE SLICE ARE SEPARATE SCALES. FaceQuads sizes the [0,1]^2 domain a leaf's corners
 * decode into and is pure topology; SectorQuads and the weight shift size the two residency keys a
 * leaf derives from its global texel index. The ring windows below turn either key into a slice, so a
 * face may span any number of resident sectors, and unresident ones read the fill slice.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FArcTerrainVFParameters, ARCTERRAIN_API)
	SHADER_PARAMETER_TEXTURE(Texture2DArray<float>, HeightAtlas)
	SHADER_PARAMETER_SAMPLER(SamplerState, HeightSamplerBilinear)
	SHADER_PARAMETER(uint32, SectorQuads)              // height texels per sector side
	SHADER_PARAMETER(uint32, FaceQuads)                // texels per face side (the corner domain)
	// SIGNED on purpose: HLSL promotes int >> uint to an UNSIGNED shift, which turns the arithmetic
	// shift these rely on (floor division across the origin) into garbage for negative keys.
	SHADER_PARAMETER(int32, SectorQuadsShift)          // log2(SectorQuads): SectorKey = GlobalTexel >> this
	SHADER_PARAMETER(int32, WeightQuadsShift)          // log2(height texels per weight slice side)
	SHADER_PARAMETER(float, TexelToLocal)              // CellSize / QuadsPerCell (cm per texel)
	SHADER_PARAMETER(float, InvTileResolution)         // 1 / (SectorQuads + 3)
	// Per height tier, packed as scalar quads (ArcTerrain::MaxTiers == 4 fits one vector). RingOffset
	// indexes the CONCATENATED ring buffer; a negative radius is an empty window, every lookup fills.
	SHADER_PARAMETER(FUintVector4, HeightRingOffset)
	SHADER_PARAMETER(FIntVector4, HeightRingRadius)
	SHADER_PARAMETER(FIntVector4, HeightRingCenterX)
	SHADER_PARAMETER(FIntVector4, HeightRingCenterY)
	SHADER_PARAMETER(FIntVector4, HeightCumulativeShift)
	SHADER_PARAMETER(uint32, NumHeightTiers)
	SHADER_PARAMETER(uint32, HeightFillSlice)
	SHADER_PARAMETER(FUintVector4, WeightRingOffset)
	SHADER_PARAMETER(FIntVector4, WeightRingRadius)
	SHADER_PARAMETER(FIntVector4, WeightRingCenterX)
	SHADER_PARAMETER(FIntVector4, WeightRingCenterY)
	SHADER_PARAMETER(FIntVector4, WeightCumulativeShift)
	SHADER_PARAMETER(uint32, NumWeightTiers)
	SHADER_PARAMETER(uint32, WeightFillSlice)
	/** The ONE sanctioned emptiness: 0 while the atlas arrays are released (layout change / teardown).
	 *  Terrain-wide, because with a static face grid and fill slices there is no per-face emptiness left
	 *  to express — an unresident sector reads the fill slice and still draws. */
	SHADER_PARAMETER(uint32, bResourcesValid)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FArcTerrainVFParameters> FArcTerrainVFBufferRef;

/**
 * Per-draw loose parameters (FMeshBatchElement::UserData), filled per batch by
 * FArcTerrainSceneProxy::GetDynamicMeshElements.
 *
 * One instance = one bisector. How SV_InstanceID becomes a pool slot is chosen PER VIEW: with
 * bUseVisibleList the batch draws the frame's compacted list and the ordinal indexes it directly;
 * without it the ordinal is decoded against the FROZEN occupancy heap and the batch covers the whole
 * live set. The bisector record at that slot carries the LEB node id and the FACE id, which names the
 * face's global texel ORIGIN — never an atlas slice.
 */
struct FArcTerrainVFUserData : public FOneFrameResource
{
	/** Read-only ByteAddressBuffer over the pool occupancy heap. */
	FRHIShaderResourceView* CBTHeapSRV = nullptr;
	/** Read-only ByteAddressBuffer over the 32-byte bisector records. */
	FRHIShaderResourceView* CBTBisectorSRV = nullptr;
	/** Read-only ByteAddressBuffer of compacted pool slots; read only when bUseVisibleList. */
	FRHIShaderResourceView* VisibleBisectorSRV = nullptr;
	/** Buffer<uint4> per face: (asuint(TexelOrigin.x), asuint(TexelOrigin.y), 0, 0). The last two
	 *  components are padding: PF_R32G32B32A32_UINT is the widest typed format the buffer can carry. */
	FRHIShaderResourceView* FaceParamsSRV = nullptr;
	/** Buffer<uint> residency windows: sector key -> height slice, weight-group key -> weight slice. */
	FRHIShaderResourceView* HeightRingSRV = nullptr;
	FRHIShaderResourceView* WeightRingSRV = nullptr;
	uint32 PoolDepth = 0;
	/** Face-id space size; a decoded face outside it draws nothing. */
	uint32 NumFaces = 0;
	/** Set only on the view the frame's compaction culled for. Every other view — shadow depth, scene
	 *  capture — leaves it clear and draws the whole live set, so no view is culled by another's frustum. */
	uint32 bUseVisibleList = 0;
};

/**
 * Reference patch index buffer for the indirect draw: the trivial {0, 1, 2} triangle. One instance =
 * one bisector, 3 indices per instance, so SV_VertexID lands in {0, 1, 2} and selects a corner. The
 * corner ORDER is chosen per instance in the shader — leb_DecodeTransformationMatrix_Square swaps
 * corners 0 and 2 on alternate depths, so a fixed index order would back-face half the tree.
 */
class ARCTERRAIN_API FArcTerrainCBTRefIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
};

/**
 * Vertexless heightfield vertex factory: no vertex buffers (a single null ManualFetch stream and an
 * empty declaration) — the vertex is synthesized from SV_InstanceID + SV_VertexID.
 *
 * THE CANONICAL NUMBER IS THE INTEGER GLOBAL TEXEL INDEX. Everything — position, both residency keys,
 * both slice UVs — is derived from it by integer arithmetic; no world coordinate is ever divided in
 * float, because a float32 centimetre at the terrain's extent has a 128 cm ULP and two vertices sharing
 * an edge would resolve different sectors.
 *   Corner      = leb_DecodeNodeCorners(leaf node id)[k], in the face's [0,1]^2 domain
 *   CornerTexel = Corner * FaceQuads  (exact while FaceQuads is 2^k: the product is the integer itself)
 *   GlobalTexel = FaceTexelOrigin + floor(CornerTexel)
 *   LocalXY(DF) = DF(GlobalTexel) + frac -> * TexelToLocal (canonical int->DF frame: adjacent sectors
 *                                                           weld bit-identically at any distance)
 *   SectorKey   = GlobalTexel >> SectorQuadsShift, then the per-tier rings give the slice
 * The POSITION slice is resolved per vertex — a vertex in the next sector must read the ground under
 * itself, and the apron makes the two agree on the shared edge. The SHADING slice and its UV come from
 * the leaf CENTROID instead, so all three vertices agree and the interpolation is an identity: the
 * index rides Result.TexCoords[2], and an interpolation modifier applies to the whole semantic
 * variable, so nointerpolation would flatten every material UV with it.
 * Normals are derived ONCE, per pixel, from a 4-tap gradient on the shading slice.
 */
class ARCTERRAIN_API FArcTerrainVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FArcTerrainVertexFactory);

public:
	explicit FArcTerrainVertexFactory(ERHIFeatureLevel::Type InFeatureLevel);

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	/**
	 * Render-thread rail: rebuilds the terrain-wide uniform buffer. The sampler field of InParams is
	 * overwritten with the factory's clamp sampler and a null HeightAtlas falls back to
	 * GBlackArrayTexture, so callers only supply the atlas texture and the numeric params.
	 */
	bool SetTerrainParams_RenderThread(FRHICommandListBase& RHICmdList, const FArcTerrainVFParameters& InParams);

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
#if WITH_EDITOR
	static void ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors);
#endif
	static void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);

	/** {0,1,2} reference patch IB bound on the mesh batch for the indirect draw. */
	const FIndexBuffer* GetCBTRefIndexBuffer() const { return &CBTRefIndexBuffer; }

	/** Always-valid ByteAddressBuffer SRV for the loose CBT parameters before the pool exists — the
	 *  shader references them unconditionally, so the binding must always resolve to a real resource. */
	FRHIShaderResourceView* GetRawFallbackSRV() const { return RawFallbackSRV.GetReference(); }

	/** Always-valid Buffer<uint4> SRV for the loose face-params parameter. */
	FRHIShaderResourceView* GetFaceParamsFallbackSRV() const { return FaceParamsFallbackSRV.GetReference(); }

	/** Always-valid Buffer<uint> SRV for the loose residency-ring parameters. Its CONTENTS are never
	 *  reached: a null ring publishes a negative radius, which the range test answers with the fill slice. */
	FRHIShaderResourceView* GetRingFallbackSRV() const { return RingFallbackSRV.GetReference(); }

private:
	FArcTerrainVFParameters Params;
	FArcTerrainVFBufferRef UniformBuffer;

	FArcTerrainCBTRefIndexBuffer CBTRefIndexBuffer;

	FBufferRHIRef RawFallbackBuffer;
	FShaderResourceViewRHIRef RawFallbackSRV;
	FBufferRHIRef FaceParamsFallbackBuffer;
	FShaderResourceViewRHIRef FaceParamsFallbackSRV;
	FBufferRHIRef RingFallbackBuffer;
	FShaderResourceViewRHIRef RingFallbackSRV;

	friend class FArcTerrainVFShaderParameters;
};
