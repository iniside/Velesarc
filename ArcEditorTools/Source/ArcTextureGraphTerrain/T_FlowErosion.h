// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ShaderCompilerCore.h"
#include "GlobalShader.h"
#include "Job/Job.h"
#include "FxMat/FxMaterial.h"
#include "FxMat/TileInfo_FX.h"
#include "Model/Mix/MixUpdateCycle.h"
#include "Data/TiledBlob.h"
#include <DataDrivenShaderPlatformInfo.h>

#define UE_API ARCTEXTUREGRAPHTERRAIN_API

//////////////////////////////////////////////////////////////////////////
/// Compute shaders. Each pass writes exactly one UAV named Result.
//////////////////////////////////////////////////////////////////////////

class CSH_FlowErosion_InitHeight : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_FlowErosion_InitHeight, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_FlowErosion_InitHeight, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, InputHeight)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(FIntPoint, DstOffset)
		SHADER_PARAMETER(float, HeightScale)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

class CSH_FlowErosion_BuildRoutes0 : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_FlowErosion_BuildRoutes0, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_FlowErosion_BuildRoutes0, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, HeightState)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(float, HeightScale)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

class CSH_FlowErosion_BuildRoutes1 : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_FlowErosion_BuildRoutes1, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_FlowErosion_BuildRoutes1, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, HeightState)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(float, HeightScale)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

class CSH_FlowErosion_BuildD8_0 : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_FlowErosion_BuildD8_0, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_FlowErosion_BuildD8_0, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, HeightState)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(float, HeightScale)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

class CSH_FlowErosion_BuildD8_1 : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_FlowErosion_BuildD8_1, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_FlowErosion_BuildD8_1, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, HeightState)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(float, HeightScale)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

class CSH_FlowErosion_InitDrainage : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_FlowErosion_InitDrainage, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_FlowErosion_InitDrainage, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(float, Rain)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

class CSH_FlowErosion_AccumulateMFD : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_FlowErosion_AccumulateMFD, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_FlowErosion_AccumulateMFD, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, PrevFlow)
		SHADER_PARAMETER_TEXTURE(Texture2D, RouteMFD0)
		SHADER_PARAMETER_TEXTURE(Texture2D, RouteMFD1)
		SHADER_PARAMETER_TEXTURE(Texture2D, RouteD80)
		SHADER_PARAMETER_TEXTURE(Texture2D, RouteD81)
		SHADER_PARAMETER_TEXTURE(Texture2D, HeightState)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(float, Rain)
		SHADER_PARAMETER(float, HeightScale)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

class CSH_FlowErosion_ReduceMaxD8 : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_FlowErosion_ReduceMaxD8, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_FlowErosion_ReduceMaxD8, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceState)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(int32, SourceWidth)
		SHADER_PARAMETER(int32, SourceHeight)
		SHADER_PARAMETER(int32, SourceChannel)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

class CSH_FlowErosion_CarveDeposit : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_FlowErosion_CarveDeposit, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_FlowErosion_CarveDeposit, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, HeightState)
		SHADER_PARAMETER_TEXTURE(Texture2D, FlowState)
		SHADER_PARAMETER_TEXTURE(Texture2D, MaxD8State)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(float, Strength)
		SHADER_PARAMETER(float, Deposition)
		SHADER_PARAMETER(float, ChannelThreshold)
		SHADER_PARAMETER(float, FeatureScale)
		SHADER_PARAMETER(float, ChannelDepthFraction)
		SHADER_PARAMETER(float, DrainageExponent)
		SHADER_PARAMETER(float, MaxErodePerStep)
		SHADER_PARAMETER(float, SedimentCapacity)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

class CSH_FlowErosion_ThermalPolish : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_FlowErosion_ThermalPolish, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_FlowErosion_ThermalPolish, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, HeightState)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(float, ThermalRate)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

class FSH_ArcExtractHeight : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_ArcExtractHeight);
	SHADER_USE_PARAMETER_STRUCT(FSH_ArcExtractHeight, FSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, StateA)
		SHADER_PARAMETER_STRUCT(FTileInfo, TileInfo)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P)
	{ return IsPCPlatform(P.Platform) && EnumHasAllFlags(P.Flags, EShaderPermutationFlags::HasEditorOnlyData); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters&, FShaderCompilerEnvironment&) {}
};

class FSH_ArcFlowMap : public FSH_Base
{
public:
	DECLARE_GLOBAL_SHADER(FSH_ArcFlowMap);
	SHADER_USE_PARAMETER_STRUCT(FSH_ArcFlowMap, FSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FStandardSamplerStates, SamplerStates)
		SHADER_PARAMETER_TEXTURE(Texture2D, StateB)
		SHADER_PARAMETER(float, FlowMapGain)
		SHADER_PARAMETER_STRUCT(FTileInfo, TileInfo)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P)
	{ return IsPCPlatform(P.Platform) && EnumHasAllFlags(P.Flags, EShaderPermutationFlags::HasEditorOnlyData); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters&, FShaderCompilerEnvironment&) {}
};

class CSH_FlowErosion_Pack : public CmpSH_Base<8, 8, 1>
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(CSH_FlowErosion_Pack, UE_API);
	SHADER_USE_PARAMETER_STRUCT(CSH_FlowErosion_Pack, CmpSH_Base);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, StateA)
		SHADER_PARAMETER_TEXTURE(Texture2D, StateB)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(float, HeightScale)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, Result)
	END_SHADER_PARAMETER_STRUCT()

	TEXTURE_ENGINE_DEFAULT_PERMUTATION
};

//////////////////////////////////////////////////////////////////////////
/// Transform
//////////////////////////////////////////////////////////////////////////

struct FArcFlowErosionResult
{
	TiledBlobPtr	ErodedHeight;
	TiledBlobPtr	FlowMap;
};

class T_FlowErosion
{
public:
	UE_API T_FlowErosion();
	UE_API ~T_FlowErosion();

	static UE_API FArcFlowErosionResult Create(MixUpdateCyclePtr Cycle, TiledBlobPtr InputHeight,
		int32 Iterations, float Strength, float Rain, float Deposition, float HeightScale, float FlowMapGain,
		float ChannelThreshold, float FeatureScale, float ChannelDepthFraction, float DrainageExponent, float MaxErodePerStep,
		float SedimentCapacity, float ThermalRate, int32 TargetId);
};

#undef UE_API
