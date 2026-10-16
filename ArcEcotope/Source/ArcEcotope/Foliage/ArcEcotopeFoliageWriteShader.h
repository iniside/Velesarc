// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Foliage/ArcEcotopeFoliageAtlas.h"
#include "Foliage/ArcEcotopeFoliageFields.h"
#include "GlobalShader.h"
#include "GPUSceneWriter.h"
#include "ShaderParameterStruct.h"

/**
 * One thread per allocated GPUScene instance slot. The thread's world lattice cell keys every random
 * draw, so an instance keeps its jitter, yaw and scale when the region that owns it moves.
 *
 * Every slot is written: an accepted one gets a transform, a rejected one the HIDDEN flag.
 */
class FArcEcotopeFoliageScatterCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FArcEcotopeFoliageScatterCS);
	SHADER_USE_PARAMETER_STRUCT(FArcEcotopeFoliageScatterCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, InPrimitiveId)
		SHADER_PARAMETER(uint32, InNumInstances)
		SHADER_PARAMETER(uint32, InNumInstancesAllocated)
		SHADER_PARAMETER(uint32, InGridX)
		SHADER_PARAMETER(uint32, InPayloadDataFlags)
		SHADER_PARAMETER(uint32, InCustomDataCount)
		SHADER_PARAMETER(uint32, InAlignToNormal)
		SHADER_PARAMETER(uint32, InMeshIndex)
		SHADER_PARAMETER(uint32, InMeshCount)
		SHADER_PARAMETER(FIntPoint, InRegionOriginCell)
		SHADER_PARAMETER(float, InGridStepCm)
		SHADER_PARAMETER(float, InProbeHeightWS)
		SHADER_PARAMETER(float, InMinScale)
		SHADER_PARAMETER(float, InMaxScale)
		SHADER_PARAMETER(float, InMaxYawRad)
		SHADER_PARAMETER(float, InCosMaxSlope)
		SHADER_PARAMETER(float, InMinLayerWeight)
		SHADER_PARAMETER(float, InUnderstoryShade)
		SHADER_PARAMETER(float, InShadePreference)
		SHADER_PARAMETER(float, InShadeStrength)
		SHADER_PARAMETER(float, InMinMoisture)
		SHADER_PARAMETER(float, InMinFertility)
		SHADER_PARAMETER_STRUCT_INCLUDE(FArcEcotopeFoliageTerrainParameters, TerrainParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FArcEcotopeFoliageFieldParameters, FieldParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FGPUSceneWriterParameters, GPUSceneWriterParameters)
	END_SHADER_PARAMETER_STRUCT()

	static constexpr uint32 NumThreadsPerGroup = 64;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};
