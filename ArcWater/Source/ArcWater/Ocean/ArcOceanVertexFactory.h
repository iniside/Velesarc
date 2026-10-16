// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SceneManagement.h"
#include "VertexFactory.h"

#include "Render/ArcTerrainAtlasPublish.h"

#include "Ocean/ArcOceanWaves.h"

class FShaderParameterMap;
struct FShaderCompilerEnvironment;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FArcOceanVFParameters, ARCWATER_API)
	SHADER_PARAMETER_ARRAY(FVector4f, Waves, [ArcOcean::MaxWaves * 2])
	SHADER_PARAMETER_ARRAY(FVector4f, SeabedSliceAndOriginPerSlot, [ArcTerrainPublishedMaxSlots])
	SHADER_PARAMETER(FMatrix44f, SeabedWorldToLocal)
	SHADER_PARAMETER(FMatrix44f, SeabedLocalToWorld)
	SHADER_PARAMETER(FVector3f, SeabedLWCTile)
	SHADER_PARAMETER(FIntPoint, SeabedOriginSector)
	SHADER_PARAMETER(int32, SeabedSectorSide)
	SHADER_PARAMETER(int32, SeabedSectorShift)
	SHADER_PARAMETER(float, SeabedCellSizeCm)
	SHADER_PARAMETER(float, SeabedHeightQuads)
	SHADER_PARAMETER(float, SeabedHeightSliceSide)
	SHADER_PARAMETER(uint32, SeabedValidSlotMask)
	SHADER_PARAMETER(float, DeepDepthCm)
	SHADER_PARAMETER(int32, WaveCount)
	SHADER_PARAMETER(FVector3f, FoamWindowOriginHigh)
	SHADER_PARAMETER(FVector3f, FoamWindowOriginLow)
	SHADER_PARAMETER(float, FoamWindowSizeCm)
	SHADER_PARAMETER_TEXTURE(Texture2DArray<float>, SeabedHeightAtlas)
	SHADER_PARAMETER_SAMPLER(SamplerState, SeabedHeightAtlasSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D<float>, FoamAccumulator)
	SHADER_PARAMETER_SAMPLER(SamplerState, FoamAccumulatorSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

struct FArcOceanSeabedFields
{
	FArcTerrainAtlasSnapshot Snapshot;
	FArcTerrainAtlasTextureResource* AtlasResource = nullptr;
	bool bAtlasFilterable = false;
	float DeepDepthCm = 0.0f;
};

struct FArcOceanFoamFields
{
	FRHITexture* Texture = nullptr;
	FVector3f WindowOriginHigh = FVector3f::ZeroVector;
	FVector3f WindowOriginLow = FVector3f::ZeroVector;
	float WindowSizeCm = 0.0f;
};

namespace ArcOcean
{
	/** Render thread. SHADER_PARAMETER does not initialise, so the whole seabed half is written before
	 *  the uniform buffer is created: a null texture in it is a hard RHI check at create and at update. */
	ARCWATER_API void SeedSeabedParams(FArcOceanVFParameters& OutParams);

	ARCWATER_API void ApplySeabedFields(FArcOceanVFParameters& OutParams, const FArcOceanSeabedFields& InFields);

	ARCWATER_API void SeedFoamParams(FArcOceanVFParameters& OutParams);

	ARCWATER_API void ApplyFoamFields(FArcOceanVFParameters& OutParams, const FArcOceanFoamFields& InFields);
}

typedef TUniformBufferRef<FArcOceanVFParameters> FArcOceanVFBufferRef;

struct FArcOceanVFUserData : public FOneFrameResource
{
	FVector2f SnappedOriginTranslatedWorld = FVector2f::ZeroVector;
	float CellSize = 0.0f;
	uint32 TrimRotationSteps = 0u;
};

class ARCWATER_API FArcOceanVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FArcOceanVertexFactory);

public:
	FArcOceanVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const FVertexBuffer* InGridVertexBuffer);

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
#if WITH_EDITOR
	static void ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors);
#endif
	static void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);

private:
	void UpdateParams_RenderThread(FRHICommandListBase& RHICmdList, const FArcOceanVFParameters& InParams);

	const FVertexBuffer* GridVertexBuffer = nullptr;

	FArcOceanVFParameters Params;
	FArcOceanVFBufferRef UniformBuffer;

	friend class FArcOceanVFShaderParameters;
	friend class FArcOceanSceneProxy;
};
