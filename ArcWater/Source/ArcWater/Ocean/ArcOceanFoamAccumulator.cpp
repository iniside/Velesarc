// Copyright Lukasz Baran. All Rights Reserved.

#include "Ocean/ArcOceanFoamAccumulator.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "Math/DoubleFloat.h"
#include "Misc/CoreDelegates.h"
#include "PixelFormat.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"
#include "ShowFlags.h"

#include "Ocean/ArcOceanSceneProxy.h"

class FArcOceanFoamAdvectCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FArcOceanFoamAdvectCS);
	SHADER_USE_PARAMETER_STRUCT(FArcOceanFoamAdvectCS, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FArcOceanVFParameters, ArcOceanVF)
		SHADER_PARAMETER(FVector3f, FoamWindowOriginHigh)
		SHADER_PARAMETER(FVector3f, FoamWindowOriginLow)
		SHADER_PARAMETER(FVector2f, FoamWindowSurfaceOriginCm)
		SHADER_PARAMETER(FVector2f, FoamWindowDeltaCm)
		SHADER_PARAMETER(float, FoamTexelSizeCm)
		SHADER_PARAMETER(float, FoamWindowSizeCm)
		SHADER_PARAMETER(float, FoamRingBaseCellSizeCm)
		SHADER_PARAMETER(float, FoamRingMorphSpanCells)
		SHADER_PARAMETER(float, FoamTimeSeconds)
		SHADER_PARAMETER(float, FoamDeltaTimeSeconds)
		SHADER_PARAMETER(uint32, FoamHasHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, FoamPrev)
		SHADER_PARAMETER_SAMPLER(SamplerState, FoamPrevSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, FoamOut)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ARC_OCEAN_MAX_WAVES"), ArcOcean::MaxWaves);
		OutEnvironment.SetDefine(TEXT("ARC_TERRAIN_ATLAS_MAX_SLOTS"), ArcTerrainPublishedMaxSlots);
		OutEnvironment.SetDefine(TEXT("ARC_OCEAN_FOAM_THREADS"), ArcOcean::FoamThreadsPerGroup);
	}
};

IMPLEMENT_GLOBAL_SHADER(FArcOceanFoamAdvectCS, "/Plugin/ArcWater/ArcOceanFoamAdvect.usf", "MainCS", SF_Compute);

namespace ArcOceanFoamAccumulatorLocal_3e08
{
	TSharedPtr<FArcOceanFoamViewExtension, ESPMode::ThreadSafe> GViewExtension;
	FDelegateHandle GPostEngineInitHandle;

	const FSceneView* FindMainView(const FSceneViewFamily& ViewFamily)
	{
		for (const FSceneView* View : ViewFamily.Views)
		{
			if (View != nullptr && !View->bIsSceneCapture && !View->bIsSceneCaptureCube
				&& !View->bIsReflectionCapture && !View->bIsPlanarReflection && !View->bIsVirtualTexture
				&& View->ViewMatrices.IsPerspectiveProjection())
			{
				return View;
			}
		}
		return nullptr;
	}

	FRDGTextureRef CreateOrRegisterTarget(FRDGBuilder& GraphBuilder,
		TRefCountPtr<IPooledRenderTarget>& Pooled, const TCHAR* Name)
	{
		if (Pooled.IsValid())
		{
			return GraphBuilder.RegisterExternalTexture(Pooled);
		}

		FRDGTextureRef Texture = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(
				FIntPoint(ArcOcean::FoamResolution, ArcOcean::FoamResolution),
				PF_R16F,
				FClearValueBinding::Black,
				ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV),
			Name, ERDGTextureFlags::MultiFrame);

		Pooled = GraphBuilder.ConvertToExternalTexture(Texture);
		return Texture;
	}
}

FArcOceanFoamAccumulator& FArcOceanFoamAccumulator::Get()
{
	static FArcOceanFoamAccumulator Instance;
	return Instance;
}

void FArcOceanFoamAccumulator::RegisterProxy(FArcOceanSceneProxy* InProxy)
{
	check(IsInRenderingThread());
	Proxies.AddUnique(InProxy);
}

void FArcOceanFoamAccumulator::UnregisterProxy(FArcOceanSceneProxy* InProxy)
{
	check(IsInRenderingThread());

	const FSceneInterface* const Scene = &InProxy->GetScene();
	Proxies.Remove(InProxy);

	for (const FArcOceanSceneProxy* Proxy : Proxies)
	{
		if (&Proxy->GetScene() == Scene)
		{
			return;
		}
	}

	States.Remove(Scene);
}

void FArcOceanFoamAccumulator::Update(FRDGBuilder& GraphBuilder, const FSceneView& InView)
{
	if (Proxies.IsEmpty() || InView.Family == nullptr || InView.Family->Scene == nullptr
		|| InView.GetFeatureLevel() < ERHIFeatureLevel::SM5)
	{
		return;
	}

	const FSceneInterface* const Scene = InView.Family->Scene;

	TArray<FArcOceanSceneProxy*, TInlineAllocator<4>> SceneProxies;
	for (FArcOceanSceneProxy* Proxy : Proxies)
	{
		if (&Proxy->GetScene() == Scene)
		{
			SceneProxies.Add(Proxy);
		}
	}

	if (SceneProxies.IsEmpty())
	{
		return;
	}

	FSceneState& State = States.FindOrAdd(Scene);

	if (State.LastUpdateFrame == GFrameNumberRenderThread)
	{
		return;
	}
	State.LastUpdateFrame = GFrameNumberRenderThread;

	const FArcOceanVFParameters OceanParams = SceneProxies[0]->GetShaderParams_RenderThread();
	const FVector SurfaceOriginCm = SceneProxies[0]->GetLocalToWorld().GetOrigin();

	const double TexelSizeCm = static_cast<double>(ArcOcean::FoamTexelSizeCm);
	const double WindowSizeCm = static_cast<double>(ArcOcean::FoamResolution) * TexelSizeCm;
	const FVector ViewOriginCm = InView.ViewMatrices.GetViewOrigin();
	const FVector NewOriginCm(
		FMath::FloorToDouble(ViewOriginCm.X / TexelSizeCm) * TexelSizeCm - 0.5 * WindowSizeCm,
		FMath::FloorToDouble(ViewOriginCm.Y / TexelSizeCm) * TexelSizeCm - 0.5 * WindowSizeCm,
		SurfaceOriginCm.Z);

	const FVector PrevOriginCm = State.bWindowValid ? State.WindowOriginCm : NewOriginCm;
	State.WindowOriginCm = NewOriginCm;
	State.bWindowValid = true;

	State.WriteIndex ^= 1;
	const int32 ReadIndex = State.WriteIndex ^ 1;

	FRDGTextureRef WriteTexture = ArcOceanFoamAccumulatorLocal_3e08::CreateOrRegisterTarget(
		GraphBuilder, State.Targets[State.WriteIndex], TEXT("ArcOcean.FoamAccumulator"));
	FRDGTextureRef ReadTexture = ArcOceanFoamAccumulatorLocal_3e08::CreateOrRegisterTarget(
		GraphBuilder, State.Targets[ReadIndex], TEXT("ArcOcean.FoamAccumulator"));

	const FDFVector3 WindowOriginDF(State.WindowOriginCm);

	FArcOceanFoamFields FoamFields;
	FoamFields.Texture = State.Targets[State.WriteIndex]->GetRHI();
	FoamFields.WindowOriginHigh = WindowOriginDF.High;
	FoamFields.WindowOriginLow = WindowOriginDF.Low;
	FoamFields.WindowSizeCm = static_cast<float>(WindowSizeCm);

	for (FArcOceanSceneProxy* Proxy : SceneProxies)
	{
		Proxy->SetFoamFields_RenderThread(GraphBuilder.RHICmdList, FoamFields);
	}

	FArcOceanVFParameters PassOceanParams = OceanParams;
	ArcOcean::SeedFoamParams(PassOceanParams);

	FArcOceanFoamAdvectCS::FParameters* PassParameters =
		GraphBuilder.AllocParameters<FArcOceanFoamAdvectCS::FParameters>();
	PassParameters->ArcOceanVF =
		FArcOceanVFBufferRef::CreateUniformBufferImmediate(PassOceanParams, UniformBuffer_SingleFrame);
	PassParameters->FoamWindowOriginHigh = WindowOriginDF.High;
	PassParameters->FoamWindowOriginLow = WindowOriginDF.Low;
	PassParameters->FoamWindowSurfaceOriginCm = FVector2f(
		static_cast<float>(State.WindowOriginCm.X - SurfaceOriginCm.X),
		static_cast<float>(State.WindowOriginCm.Y - SurfaceOriginCm.Y));
	PassParameters->FoamWindowDeltaCm = FVector2f(
		static_cast<float>(State.WindowOriginCm.X - PrevOriginCm.X),
		static_cast<float>(State.WindowOriginCm.Y - PrevOriginCm.Y));
	PassParameters->FoamTexelSizeCm = ArcOcean::FoamTexelSizeCm;
	PassParameters->FoamWindowSizeCm = static_cast<float>(WindowSizeCm);
	PassParameters->FoamRingBaseCellSizeCm = SceneProxies[0]->GetBaseCellSizeCm();
	PassParameters->FoamRingMorphSpanCells = SceneProxies[0]->GetRingMorphSpanCells();
	PassParameters->FoamTimeSeconds = InView.Family->Time.GetWorldTimeSeconds();
	PassParameters->FoamDeltaTimeSeconds =
		FMath::Clamp(InView.Family->Time.GetDeltaWorldTimeSeconds(), 0.0f, 0.1f);
	PassParameters->FoamHasHistory = (State.WrittenMask & (1u << ReadIndex)) != 0u ? 1u : 0u;
	PassParameters->FoamPrev = ReadTexture;
	PassParameters->FoamPrevSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->FoamOut = GraphBuilder.CreateUAV(WriteTexture);

	TShaderMapRef<FArcOceanFoamAdvectCS> ComputeShader(GetGlobalShaderMap(InView.GetShaderPlatform()));
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ArcOceanFoamAdvect"),
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(
			FIntPoint(ArcOcean::FoamResolution, ArcOcean::FoamResolution), ArcOcean::FoamThreadsPerGroup));

	GraphBuilder.UseExternalAccessMode(WriteTexture, ERHIAccess::SRVMask, ERHIPipeline::Graphics);

	State.WrittenMask |= 1u << State.WriteIndex;
}

void FArcOceanFoamViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	// A hit-proxy pass re-renders the same camera under its own frame number, so the frame guard lets it through and it would decay the buffer a second time.
	if (InViewFamily.EngineShowFlags.HitProxies)
	{
		return;
	}

	const FSceneView* const View = ArcOceanFoamAccumulatorLocal_3e08::FindMainView(InViewFamily);
	if (View == nullptr)
	{
		return;
	}

	FArcOceanFoamAccumulator::Get().Update(GraphBuilder, *View);
}

void ArcOcean::RegisterFoamViewExtension()
{
	if (ArcOceanFoamAccumulatorLocal_3e08::GPostEngineInitHandle.IsValid())
	{
		return;
	}

	ArcOceanFoamAccumulatorLocal_3e08::GPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddLambda([]()
	{
		ArcOceanFoamAccumulatorLocal_3e08::GViewExtension =
			FSceneViewExtensions::NewExtension<FArcOceanFoamViewExtension>();
	});
}

void ArcOcean::UnregisterFoamViewExtension()
{
	if (ArcOceanFoamAccumulatorLocal_3e08::GPostEngineInitHandle.IsValid())
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(ArcOceanFoamAccumulatorLocal_3e08::GPostEngineInitHandle);
		ArcOceanFoamAccumulatorLocal_3e08::GPostEngineInitHandle.Reset();
	}

	ArcOceanFoamAccumulatorLocal_3e08::GViewExtension.Reset();
}
