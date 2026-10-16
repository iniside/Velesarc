// Copyright Lukasz Baran. All Rights Reserved.

#include "Foliage/ArcEcotopeFoliageWriteShader.h"

void FArcEcotopeFoliageScatterCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	OutEnvironment.SetDefine(TEXT("USE_GPU_SCENE_DATA_RW"), 1);
	OutEnvironment.SetDefine(TEXT("NUM_THREADS_PER_GROUP"), NumThreadsPerGroup);
	// Sizes the per-slot origin arrays ArcEcotopeFoliageTerrain.ush declares; must match the C++ struct.
	OutEnvironment.SetDefine(TEXT("ARC_ECOTOPE_FOLIAGE_MAX_CELLS"), GArcEcotopeFoliageMaxCells);
}

IMPLEMENT_GLOBAL_SHADER(FArcEcotopeFoliageScatterCS, "/Plugin/ArcEcotope/Private/ArcEcotopeFoliageScatter.usf", "ArcEcotopeFoliageScatter_CS", SF_Compute);
