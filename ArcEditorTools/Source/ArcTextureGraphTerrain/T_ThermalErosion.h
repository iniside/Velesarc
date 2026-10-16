// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ShaderCompilerCore.h"
#include "GlobalShader.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "Model/Mix/MixUpdateCycle.h"
#include "Data/TiledBlob.h"
#include <DataDrivenShaderPlatformInfo.h>

#define UE_API ARCTEXTUREGRAPHTERRAIN_API

//////////////////////////////////////////////////////////////////////////
/// Compute shader — single pass, whole-domain thermal (talus) erosion.
/// Reads InputHeight SRV, writes R32F height to Result UAV (.r channel).
/// THREADGROUPSIZE_{X,Y,Z} injected by CmpSH_Base<8,8,1>.
//////////////////////////////////////////////////////////////////////////

class CSH_Thermal : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_Thermal, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_Thermal, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, InputHeight)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(float, Strength)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

//////////////////////////////////////////////////////////////////////////
/// Transform
//////////////////////////////////////////////////////////////////////////

class T_ThermalErosion
{
public:
	UE_API T_ThermalErosion();
	UE_API ~T_ThermalErosion();

	/// Runs Iterations ping-pong thermal erosion passes on InputHeight and returns the eroded height blob.
	static UE_API TiledBlobPtr Create(MixUpdateCyclePtr Cycle, TiledBlobPtr InputHeight,
		int32 Iterations, float Strength, int32 TargetId);
};

#undef UE_API
