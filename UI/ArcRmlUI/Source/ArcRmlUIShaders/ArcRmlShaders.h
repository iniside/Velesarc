// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

namespace ArcRmlShaders
{
	inline constexpr int32 MaxGradientStops = 16;
}

enum class EArcRmlTextureEncoding : uint32
{
	Premultiplied = 0,
	Straight = 1,
	StraightLinear = 2
};

BEGIN_SHADER_PARAMETER_STRUCT(FArcRmlSourceParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SourceTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
	SHADER_PARAMETER(FVector4f, SourceScaleBias)
	SHADER_PARAMETER(FVector2f, SourceInvSize)
	SHADER_PARAMETER(FVector4f, SourceRegion)
END_SHADER_PARAMETER_STRUCT()

class FArcRmlGeometryVS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FArcRmlGeometryVS, ARCRMLUISHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FArcRmlGeometryVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FMatrix44f, GeometryTransform)
		SHADER_PARAMETER(FVector2f, GeometryTranslation)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

class FArcRmlColorPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FArcRmlColorPS, ARCRMLUISHADERS_API);

	FArcRmlColorPS() = default;
	FArcRmlColorPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

class FArcRmlTexturePS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FArcRmlTexturePS, ARCRMLUISHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FArcRmlTexturePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, GeometryTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, GeometrySampler)
		SHADER_PARAMETER(uint32, GeometryTextureEncoding)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

class FArcRmlGradientPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FArcRmlGradientPS, ARCRMLUISHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FArcRmlGradientPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, GradientFunction)
		SHADER_PARAMETER(int32, GradientNumStops)
		SHADER_PARAMETER(FVector2f, GradientP)
		SHADER_PARAMETER(FVector2f, GradientV)
		SHADER_PARAMETER_ARRAY(FVector4f, GradientStopColors, [ArcRmlShaders::MaxGradientStops])
		SHADER_PARAMETER_ARRAY(FVector4f, GradientStopPositions, [ArcRmlShaders::MaxGradientStops / 4])
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ARC_RML_MAX_GRADIENT_STOPS"), ArcRmlShaders::MaxGradientStops);
	}
};

class FArcRmlCopyPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FArcRmlCopyPS, ARCRMLUISHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FArcRmlCopyPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FArcRmlSourceParameters, Source)
		SHADER_PARAMETER(float, CopyOpacity)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

class FArcRmlColorMatrixPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FArcRmlColorMatrixPS, ARCRMLUISHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FArcRmlColorMatrixPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FArcRmlSourceParameters, Source)
		SHADER_PARAMETER(FMatrix44f, ColorMatrix)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

class FArcRmlBlendMaskPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FArcRmlBlendMaskPS, ARCRMLUISHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FArcRmlBlendMaskPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FArcRmlSourceParameters, Source)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MaskTexture)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

class FArcRmlBlurPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FArcRmlBlurPS, ARCRMLUISHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FArcRmlBlurPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FArcRmlSourceParameters, Source)
		SHADER_PARAMETER(FVector2f, BlurTexelOffset)
		SHADER_PARAMETER(FVector4f, BlurWeights)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

class FArcRmlDropShadowPS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FArcRmlDropShadowPS, ARCRMLUISHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FArcRmlDropShadowPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FArcRmlSourceParameters, Source)
		SHADER_PARAMETER(FVector4f, ShadowColor)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};
