// Copyright Epic Games, Inc. All Rights Reserved.
#include "T_FlowErosion.h"
#include "TextureGraphEngine.h"
#include "Job/JobArgs.h"
#include "Job/JobBatch.h"
#include "FxMat/MaterialManager.h"
#include "FxMat/RenderMaterial_FX.h"
#include "Model/Mix/MixInterface.h"
#include "Data/TiledBlob.h"
#include "Transform/BlobTransform.h"
#include "Device/FX/Device_FX.h"
#include "Device/FX/DeviceBuffer_FX.h"
#include "2D/Tex.h"
#include <TextureResource.h>

IMPLEMENT_GLOBAL_SHADER(CSH_FlowErosion_InitHeight, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "CSH_InitHeight", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(CSH_FlowErosion_BuildRoutes0, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "CSH_BuildRoutes0", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(CSH_FlowErosion_BuildRoutes1, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "CSH_BuildRoutes1", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(CSH_FlowErosion_BuildD8_0, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "CSH_BuildD8_0", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(CSH_FlowErosion_BuildD8_1, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "CSH_BuildD8_1", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(CSH_FlowErosion_InitDrainage, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "CSH_InitDrainage", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(CSH_FlowErosion_AccumulateMFD, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "CSH_AccumulateMFD", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(CSH_FlowErosion_ReduceMaxD8, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "CSH_ReduceMaxD8", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(CSH_FlowErosion_CarveDeposit, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "CSH_CarveDeposit", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(CSH_FlowErosion_ThermalPolish, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "CSH_ThermalPolish", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSH_ArcExtractHeight, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "FSH_ArcExtractHeight", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSH_ArcFlowMap, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "FSH_ArcFlowMap", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(CSH_FlowErosion_Pack, "/Plugin/ArcEditorTools/ArcFlowErosion.usf", "CSH_Pack", SF_Compute);

T_FlowErosion::T_FlowErosion()
{
}

T_FlowErosion::~T_FlowErosion()
{
}

namespace ArcFlowErosion_Internal
{
	class FArcNoCacheJob : public Job
	{
	public:
		FArcNoCacheJob(UMixInterface* InMix, int32 InTargetId, BlobTransformPtr InTransform)
			: Job(InMix, InTargetId, InTransform)
		{
			bIsNoCache = true;
		}
	};

	static JobUPtr MakeJob(MixUpdateCyclePtr Cycle, int32 TargetId, RenderMaterial_FXPtr Transform)
	{
		return std::make_unique<FArcNoCacheJob>(Cycle->GetMix(), TargetId, std::static_pointer_cast<BlobTransform>(Transform));
	}

	static BufferDescriptor MakeStateDesc(int32 InWidth, int32 InHeight, int32 InItemsPerPoint, const FString& InName)
	{
		BufferDescriptor Desc;
		Desc.Width = (uint32)InWidth;
		Desc.Height = (uint32)InHeight;
		Desc.Format = BufferFormat::Float;
		Desc.ItemsPerPoint = (uint32)InItemsPerPoint;
		Desc.Name = InName;
		Desc.AddMetadata(FString(TEXT("FX:UAV")));
		return Desc;
	}

	static BufferDescriptor MakeOutputDesc(int32 InWidth, int32 InHeight, int32 InItemsPerPoint, const FString& InName)
	{
		BufferDescriptor Desc;
		Desc.Width = (uint32)InWidth;
		Desc.Height = (uint32)InHeight;
		Desc.Format = BufferFormat::Float;
		Desc.ItemsPerPoint = (uint32)InItemsPerPoint;
		Desc.Name = InName;
		return Desc;
	}

	static FRHITexture* GetRHITextureChecked(const TexPtr& Texture)
	{
		check(Texture.get());
		FRHITexture* RHITexture = Texture->GetRHITexture();
		check(RHITexture);
		return RHITexture;
	}

	static FUnorderedAccessViewRHIRef CreateTextureUAV(FRHICommandListImmediate& RHI, FRHITexture* Texture)
	{
		check(Texture);
		return RHI.CreateUnorderedAccessView(Texture, FRHIViewDesc::CreateTextureUAV().SetDimensionFromTexture(Texture));
	}

	static FIntVector MakeGroupCount(int32 InWidth, int32 InHeight)
	{
		return FIntVector(FMath::DivideAndRoundUp(InWidth, 8), FMath::DivideAndRoundUp(InHeight, 8), 1);
	}

	static TiledBlobPtr RunExtractHeight(MixUpdateCyclePtr Cycle, TiledBlobPtr StateA, int32 TargetId, int32 InWidth, int32 InHeight)
	{
		FString Name = TEXT("FlowErosion.ExtractHeight");

		RenderMaterial_FXPtr Transform = TextureGraphEngine::GetMaterialManager()
			->CreateMaterial_FX<VSH_Simple, FSH_ArcExtractHeight>(Name);

		JobUPtr JobObj = MakeJob(Cycle, TargetId, Transform);
		JobObj
			->AddArg(ARG_BLOB(StateA, "StateA"))
			->AddArg(ARG_TILEINFO(FTileInfo(), "TileInfo"));

		BufferDescriptor Desc = MakeOutputDesc(InWidth, InHeight, 1, Name);
		TiledBlobPtr Result = JobObj->InitResult(Name, &Desc);

		Cycle->AddJob(TargetId, std::move(JobObj));
		return Result;
	}

	static TiledBlobPtr RunFlowMap(MixUpdateCyclePtr Cycle, TiledBlobPtr StateB, int32 TargetId, int32 InWidth, int32 InHeight, float FlowMapGain)
	{
		FString Name = TEXT("FlowErosion.FlowMap");

		RenderMaterial_FXPtr Transform = TextureGraphEngine::GetMaterialManager()
			->CreateMaterial_FX<VSH_Simple, FSH_ArcFlowMap>(Name);

		JobUPtr JobObj = MakeJob(Cycle, TargetId, Transform);
		JobObj
			->AddArg(ARG_BLOB(StateB, "StateB"))
			->AddArg(ARG_FLOAT(FlowMapGain, "FlowMapGain"))
			->AddArg(ARG_TILEINFO(FTileInfo(), "TileInfo"));

		BufferDescriptor Desc = MakeOutputDesc(InWidth, InHeight, 1, Name);
		TiledBlobPtr Result = JobObj->InitResult(Name, &Desc);

		Cycle->AddJob(TargetId, std::move(JobObj));
		return Result;
	}

	class RenderMaterial_FX_FlowErosion : public RenderMaterial_FX
	{
	private:
		TiledBlobPtr	InputHeight;
		int32			Iterations = 1;
		float			Strength = 0.0f;
		float			Rain = 0.0f;
		float			Deposition = 0.0f;
		float			HeightScale = 1.0f;
		float			ChannelThreshold = 0.0f;
		float			FeatureScale = 1.0f;
		float			ChannelDepthFraction = 0.0f;
		float			DrainageExponent = 0.0f;
		float			MaxErodePerStep = 0.0f;
		float			SedimentCapacity = 0.0f;
		float			ThermalRate = 0.0f;
		int32			Width = 0;
		int32			Height = 0;

		TexPtr			HeightA;
		TexPtr			HeightB;
		TexPtr			FlowA;
		TexPtr			FlowB;
		TexPtr			RouteMFD0;
		TexPtr			RouteMFD1;
		TexPtr			RouteD80;
		TexPtr			RouteD81;
		TArray<TexPtr>	ReduceChain;
		TArray<FIntPoint> ReduceSizes;

	public:
		RenderMaterial_FX_FlowErosion(FString InName, FxMaterialPtr InBaseMaterial, TiledBlobPtr InInputHeight,
			int32 InIterations, float InStrength, float InRain, float InDeposition, float InHeightScale,
			float InChannelThreshold, float InFeatureScale, float InChannelDepthFraction, float InDrainageExponent, float InMaxErodePerStep,
			float InSedimentCapacity, float InThermalRate, int32 InWidth, int32 InHeight)
			: RenderMaterial_FX(InName, InBaseMaterial)
			, InputHeight(InInputHeight)
			, Iterations(FMath::Clamp(InIterations, 1, 1000))
			, Strength(InStrength)
			, Rain(InRain)
			, Deposition(InDeposition)
			, HeightScale(FMath::Max(InHeightScale, 1.0f))
			, ChannelThreshold(InChannelThreshold)
			, FeatureScale(FMath::Clamp(InFeatureScale, 0.25f, 8.0f))
			, ChannelDepthFraction(InChannelDepthFraction)
			, DrainageExponent(InDrainageExponent)
			, MaxErodePerStep(InMaxErodePerStep)
			, SedimentCapacity(InSedimentCapacity)
			, ThermalRate(InThermalRate)
			, Width(InWidth)
			, Height(InHeight)
		{
		}

		virtual ~RenderMaterial_FX_FlowErosion() override
		{
		}

		virtual AsyncPrepareResult PrepareResources(const TransformArgs& Args) override
		{
			HeightA = Device_FX::Get()->AllocateRenderTarget(MakeStateDesc(Width, Height, 4, TEXT("FlowErosion.Height.A")));
			HeightB = Device_FX::Get()->AllocateRenderTarget(MakeStateDesc(Width, Height, 4, TEXT("FlowErosion.Height.B")));
			FlowA = Device_FX::Get()->AllocateRenderTarget(MakeStateDesc(Width, Height, 4, TEXT("FlowErosion.Flow.A")));
			FlowB = Device_FX::Get()->AllocateRenderTarget(MakeStateDesc(Width, Height, 4, TEXT("FlowErosion.Flow.B")));
			RouteMFD0 = Device_FX::Get()->AllocateRenderTarget(MakeStateDesc(Width, Height, 4, TEXT("FlowErosion.Route.MFD0")));
			RouteMFD1 = Device_FX::Get()->AllocateRenderTarget(MakeStateDesc(Width, Height, 4, TEXT("FlowErosion.Route.MFD1")));
			RouteD80 = Device_FX::Get()->AllocateRenderTarget(MakeStateDesc(Width, Height, 4, TEXT("FlowErosion.Route.D80")));
			RouteD81 = Device_FX::Get()->AllocateRenderTarget(MakeStateDesc(Width, Height, 4, TEXT("FlowErosion.Route.D81")));

			ReduceChain.Reset();
			ReduceSizes.Reset();

			int32 ReduceWidth = Width;
			int32 ReduceHeight = Height;
			while (ReduceWidth > 1 || ReduceHeight > 1)
			{
				ReduceWidth = FMath::Max(1, FMath::DivideAndRoundUp(ReduceWidth, 2));
				ReduceHeight = FMath::Max(1, FMath::DivideAndRoundUp(ReduceHeight, 2));
				FString ReduceName = FString::Printf(TEXT("FlowErosion.Reduce.%dx%d"), ReduceWidth, ReduceHeight);
				ReduceSizes.Add(FIntPoint(ReduceWidth, ReduceHeight));
				ReduceChain.Add(Device_FX::Get()->AllocateRenderTarget(MakeStateDesc(ReduceWidth, ReduceHeight, 4, ReduceName)));
			}

			return cti::make_ready_continuable(0);
		}

		void DispatchRoutes(FRHICommandListImmediate& RHI, FRHITexture* HeightRHI,
			FRHITexture* RouteMFD0RHI, FRHITexture* RouteMFD1RHI, FRHITexture* RouteD80RHI, FRHITexture* RouteD81RHI)
		{
			const FIntVector GroupCount = MakeGroupCount(Width, Height);

			{
				FUnorderedAccessViewRHIRef ResultUAV = CreateTextureUAV(RHI, RouteMFD0RHI);
				RHI.Transition(FRHITransitionInfo(RouteMFD0RHI, ERHIAccess::Unknown, ERHIAccess::UAVMask));
				CSH_FlowErosion_BuildRoutes0::FParameters Params;
				Params.HeightState = HeightRHI;
				Params.Width = Width;
				Params.Height = Height;
				Params.HeightScale = HeightScale;
				Params.Result = ResultUAV;
				TShaderMapRef<CSH_FlowErosion_BuildRoutes0> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));
				FComputeShaderUtils::Dispatch(RHI, Shader, Params, GroupCount);
				RHI.Transition(FRHITransitionInfo(RouteMFD0RHI, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			}

			{
				FUnorderedAccessViewRHIRef ResultUAV = CreateTextureUAV(RHI, RouteMFD1RHI);
				RHI.Transition(FRHITransitionInfo(RouteMFD1RHI, ERHIAccess::Unknown, ERHIAccess::UAVMask));
				CSH_FlowErosion_BuildRoutes1::FParameters Params;
				Params.HeightState = HeightRHI;
				Params.Width = Width;
				Params.Height = Height;
				Params.HeightScale = HeightScale;
				Params.Result = ResultUAV;
				TShaderMapRef<CSH_FlowErosion_BuildRoutes1> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));
				FComputeShaderUtils::Dispatch(RHI, Shader, Params, GroupCount);
				RHI.Transition(FRHITransitionInfo(RouteMFD1RHI, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			}

			{
				FUnorderedAccessViewRHIRef ResultUAV = CreateTextureUAV(RHI, RouteD80RHI);
				RHI.Transition(FRHITransitionInfo(RouteD80RHI, ERHIAccess::Unknown, ERHIAccess::UAVMask));
				CSH_FlowErosion_BuildD8_0::FParameters Params;
				Params.HeightState = HeightRHI;
				Params.Width = Width;
				Params.Height = Height;
				Params.HeightScale = HeightScale;
				Params.Result = ResultUAV;
				TShaderMapRef<CSH_FlowErosion_BuildD8_0> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));
				FComputeShaderUtils::Dispatch(RHI, Shader, Params, GroupCount);
				RHI.Transition(FRHITransitionInfo(RouteD80RHI, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			}

			{
				FUnorderedAccessViewRHIRef ResultUAV = CreateTextureUAV(RHI, RouteD81RHI);
				RHI.Transition(FRHITransitionInfo(RouteD81RHI, ERHIAccess::Unknown, ERHIAccess::UAVMask));
				CSH_FlowErosion_BuildD8_1::FParameters Params;
				Params.HeightState = HeightRHI;
				Params.Width = Width;
				Params.Height = Height;
				Params.HeightScale = HeightScale;
				Params.Result = ResultUAV;
				TShaderMapRef<CSH_FlowErosion_BuildD8_1> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));
				FComputeShaderUtils::Dispatch(RHI, Shader, Params, GroupCount);
				RHI.Transition(FRHITransitionInfo(RouteD81RHI, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			}
		}

		FRHITexture* DispatchReduceMax(FRHICommandListImmediate& RHI, FRHITexture* SourceRHI)
		{
			FRHITexture* CurrentSourceRHI = SourceRHI;
			int32 SourceWidth = Width;
			int32 SourceHeight = Height;
			int32 SourceChannel = 3;

			for (int32 ChainIndex = 0; ChainIndex < ReduceChain.Num(); ++ChainIndex)
			{
				FRHITexture* TargetRHI = GetRHITextureChecked(ReduceChain[ChainIndex]);
				FUnorderedAccessViewRHIRef ResultUAV = CreateTextureUAV(RHI, TargetRHI);
				RHI.Transition(FRHITransitionInfo(TargetRHI, ERHIAccess::Unknown, ERHIAccess::UAVMask));

				const FIntPoint TargetSize = ReduceSizes[ChainIndex];
				CSH_FlowErosion_ReduceMaxD8::FParameters Params;
				Params.SourceState = CurrentSourceRHI;
				Params.Width = TargetSize.X;
				Params.Height = TargetSize.Y;
				Params.SourceWidth = SourceWidth;
				Params.SourceHeight = SourceHeight;
				Params.SourceChannel = SourceChannel;
				Params.Result = ResultUAV;

				TShaderMapRef<CSH_FlowErosion_ReduceMaxD8> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));
				FComputeShaderUtils::Dispatch(RHI, Shader, Params, MakeGroupCount(TargetSize.X, TargetSize.Y));

				RHI.Transition(FRHITransitionInfo(TargetRHI, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
				CurrentSourceRHI = TargetRHI;
				SourceWidth = TargetSize.X;
				SourceHeight = TargetSize.Y;
				SourceChannel = 0;
			}

			return CurrentSourceRHI;
		}

		virtual AsyncTransformResultPtr Exec(const TransformArgs& Args) override
		{
			check(IsInRenderingThread());

			FRHICommandListImmediate& RHI = Device_FX::Get()->RHI();

			BlobPtr TargetBlob = Args.Target.lock();
			check(TargetBlob);
			check(TargetBlob->GetBufferRef().GetDeviceType() == DeviceType::FX);
			DeviceBuffer_FX* TargetBuffer = static_cast<DeviceBuffer_FX*>(TargetBlob->GetBufferRef().get());
			TexPtr ResultTex = TargetBuffer->GetTexture();
			FRHITexture* ResultRHI = GetRHITextureChecked(ResultTex);

			const FIntVector GroupCount = MakeGroupCount(Width, Height);
			FRHITexture* HeightARHI = GetRHITextureChecked(HeightA);
			FRHITexture* HeightBRHI = GetRHITextureChecked(HeightB);
			FRHITexture* FlowARHI = GetRHITextureChecked(FlowA);
			FRHITexture* FlowBRHI = GetRHITextureChecked(FlowB);
			FRHITexture* RouteMFD0RHI = GetRHITextureChecked(RouteMFD0);
			FRHITexture* RouteMFD1RHI = GetRHITextureChecked(RouteMFD1);
			FRHITexture* RouteD80RHI = GetRHITextureChecked(RouteD80);
			FRHITexture* RouteD81RHI = GetRHITextureChecked(RouteD81);

			{
				check(InputHeight);

				FUnorderedAccessViewRHIRef HeightUAV = CreateTextureUAV(RHI, HeightARHI);
				RHI.Transition(FRHITransitionInfo(HeightARHI, ERHIAccess::Unknown, ERHIAccess::UAVMask));
				TShaderMapRef<CSH_FlowErosion_InitHeight> InitCS(GetGlobalShaderMap(GMaxRHIShaderPlatform));

				const int32 NumRows = (int32)InputHeight->Rows();
				const int32 NumCols = (int32)InputHeight->Cols();
				for (int32 Row = 0; Row < NumRows; ++Row)
				{
					for (int32 Col = 0; Col < NumCols; ++Col)
					{
						if (!InputHeight->IsValidTileIndex(Row, Col))
						{
							continue;
						}

						const BlobPtr Tile = InputHeight->GetTile(Row, Col);
						check(Tile);
						check(Tile->GetBufferRef().GetDeviceType() == DeviceType::FX);
						DeviceBuffer_FX* TileBuffer = static_cast<DeviceBuffer_FX*>(Tile->GetBufferRef().get());
						TexPtr TileTex = TileBuffer->GetTexture();
						FRHITexture* TileRHI = GetRHITextureChecked(TileTex);
						const FIntPoint TileSize = TileRHI->GetSizeXY();

						CSH_FlowErosion_InitHeight::FParameters Params;
						Params.InputHeight = TileRHI;
						Params.Width = TileSize.X;
						Params.Height = TileSize.Y;
						Params.DstOffset = FIntPoint(Col * TileSize.X, Row * TileSize.Y);
						Params.HeightScale = HeightScale;
						Params.Result = HeightUAV;

						FComputeShaderUtils::Dispatch(RHI, InitCS, Params, MakeGroupCount(TileSize.X, TileSize.Y));
					}
				}

				RHI.Transition(FRHITransitionInfo(HeightARHI, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			}

			DispatchRoutes(RHI, HeightARHI, RouteMFD0RHI, RouteMFD1RHI, RouteD80RHI, RouteD81RHI);

			{
				FUnorderedAccessViewRHIRef FlowUAV = CreateTextureUAV(RHI, FlowARHI);
				RHI.Transition(FRHITransitionInfo(FlowARHI, ERHIAccess::Unknown, ERHIAccess::UAVMask));

				CSH_FlowErosion_InitDrainage::FParameters Params;
				Params.Width = Width;
				Params.Height = Height;
				Params.Rain = Rain;
				Params.Result = FlowUAV;

				TShaderMapRef<CSH_FlowErosion_InitDrainage> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));
				FComputeShaderUtils::Dispatch(RHI, Shader, Params, GroupCount);
				RHI.Transition(FRHITransitionInfo(FlowARHI, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			}

			FRHITexture* HeightReadRHI = HeightARHI;
			FRHITexture* HeightWriteRHI = HeightBRHI;
			FRHITexture* FlowReadRHI = FlowARHI;
			FRHITexture* FlowWriteRHI = FlowBRHI;

			for (int32 IterationIndex = 0; IterationIndex < Iterations; ++IterationIndex)
			{
				{
					FUnorderedAccessViewRHIRef FlowUAV = CreateTextureUAV(RHI, FlowWriteRHI);
					RHI.Transition(FRHITransitionInfo(FlowWriteRHI, ERHIAccess::Unknown, ERHIAccess::UAVMask));

					CSH_FlowErosion_AccumulateMFD::FParameters Params;
					Params.PrevFlow = FlowReadRHI;
					Params.RouteMFD0 = RouteMFD0RHI;
					Params.RouteMFD1 = RouteMFD1RHI;
					Params.RouteD80 = RouteD80RHI;
					Params.RouteD81 = RouteD81RHI;
					Params.HeightState = HeightReadRHI;
					Params.Width = Width;
					Params.Height = Height;
					Params.Rain = Rain;
					Params.HeightScale = HeightScale;
					Params.Result = FlowUAV;

					TShaderMapRef<CSH_FlowErosion_AccumulateMFD> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));
					FComputeShaderUtils::Dispatch(RHI, Shader, Params, GroupCount);
					RHI.Transition(FRHITransitionInfo(FlowWriteRHI, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
				}

				FRHITexture* FlowTempRHI = FlowReadRHI;
				FlowReadRHI = FlowWriteRHI;
				FlowWriteRHI = FlowTempRHI;

				FRHITexture* MaxD8RHI = DispatchReduceMax(RHI, FlowReadRHI);

				{
					FUnorderedAccessViewRHIRef HeightUAV = CreateTextureUAV(RHI, HeightWriteRHI);
					RHI.Transition(FRHITransitionInfo(HeightWriteRHI, ERHIAccess::Unknown, ERHIAccess::UAVMask));

					CSH_FlowErosion_CarveDeposit::FParameters Params;
					Params.HeightState = HeightReadRHI;
					Params.FlowState = FlowReadRHI;
					Params.MaxD8State = MaxD8RHI;
					Params.Width = Width;
					Params.Height = Height;
					Params.Strength = Strength;
					Params.Deposition = Deposition;
					Params.ChannelThreshold = ChannelThreshold;
					Params.FeatureScale = FeatureScale;
					Params.ChannelDepthFraction = ChannelDepthFraction;
					Params.DrainageExponent = DrainageExponent;
					Params.MaxErodePerStep = MaxErodePerStep;
					Params.SedimentCapacity = SedimentCapacity;
					Params.Result = HeightUAV;

					TShaderMapRef<CSH_FlowErosion_CarveDeposit> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));
					FComputeShaderUtils::Dispatch(RHI, Shader, Params, GroupCount);
					RHI.Transition(FRHITransitionInfo(HeightWriteRHI, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
				}

				if (ThermalRate > 0.0f)
				{
					FUnorderedAccessViewRHIRef HeightUAV = CreateTextureUAV(RHI, HeightReadRHI);
					RHI.Transition(FRHITransitionInfo(HeightReadRHI, ERHIAccess::Unknown, ERHIAccess::UAVMask));

					CSH_FlowErosion_ThermalPolish::FParameters Params;
					Params.HeightState = HeightWriteRHI;
					Params.Width = Width;
					Params.Height = Height;
					Params.ThermalRate = ThermalRate;
					Params.Result = HeightUAV;

					TShaderMapRef<CSH_FlowErosion_ThermalPolish> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));
					FComputeShaderUtils::Dispatch(RHI, Shader, Params, GroupCount);
					RHI.Transition(FRHITransitionInfo(HeightReadRHI, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
				}
				else
				{
					FRHITexture* HeightTempRHI = HeightReadRHI;
					HeightReadRHI = HeightWriteRHI;
					HeightWriteRHI = HeightTempRHI;
				}

				DispatchRoutes(RHI, HeightReadRHI, RouteMFD0RHI, RouteMFD1RHI, RouteD80RHI, RouteD81RHI);
			}

			{
				FUnorderedAccessViewRHIRef ResultUAV = CreateTextureUAV(RHI, ResultRHI);
				RHI.Transition(FRHITransitionInfo(ResultRHI, ERHIAccess::Unknown, ERHIAccess::UAVMask));

				CSH_FlowErosion_Pack::FParameters Params;
				Params.StateA = HeightReadRHI;
				Params.StateB = FlowReadRHI;
				Params.Width = Width;
				Params.Height = Height;
				Params.HeightScale = HeightScale;
				Params.Result = ResultUAV;

				TShaderMapRef<CSH_FlowErosion_Pack> Shader(GetGlobalShaderMap(GMaxRHIShaderPlatform));
				FComputeShaderUtils::Dispatch(RHI, Shader, Params, GroupCount);
				RHI.Transition(FRHITransitionInfo(ResultRHI, ERHIAccess::UAVMask, ERHIAccess::SRVMask));
			}

			TransformResultPtr Result = std::make_shared<TransformResult>();
			Result->Target = Args.JobObj->GetResultRef();
			return cti::make_ready_continuable(Result);
		}
	};
}

FArcFlowErosionResult T_FlowErosion::Create(MixUpdateCyclePtr Cycle, TiledBlobPtr InputHeight,
	int32 Iterations, float Strength, float Rain, float Deposition, float HeightScale, float FlowMapGain,
	float ChannelThreshold, float FeatureScale, float ChannelDepthFraction, float DrainageExponent, float MaxErodePerStep,
	float SedimentCapacity, float ThermalRate, int32 TargetId)
{
	using namespace ArcFlowErosion_Internal;

	FArcFlowErosionResult Out;

	if (!InputHeight)
	{
		return Out;
	}

	const int32 W = (int32)InputHeight->GetWidth();
	const int32 H = (int32)InputHeight->GetHeight();

	if (W <= 0 || H <= 0)
	{
		Out.ErodedHeight = InputHeight;
		Out.FlowMap = InputHeight;
		return Out;
	}

	const int32 NumIters = FMath::Clamp(Iterations, 1, 1000);
	const float SafeHeightScale = FMath::Max(HeightScale, 1.0f);
	const float SafeFeatureScale = FMath::Clamp(FeatureScale, 0.25f, 8.0f);

	const FString SimName = FString::Printf(TEXT("[%s].FlowErosion.MFD.I%d_S%.5f_R%.5f_D%.5f_H%.2f_CT%.5f_FS%.5f_CD%.5f_DE%.5f_ME%.5f_SC%.5f_TR%.5f"),
		*InputHeight->Name(), NumIters, Strength, Rain, Deposition, SafeHeightScale,
		ChannelThreshold, SafeFeatureScale, ChannelDepthFraction, DrainageExponent, MaxErodePerStep, SedimentCapacity, ThermalRate);

	RenderMaterial_FXPtr BaseMat = TextureGraphEngine::GetMaterialManager()->CreateMaterial_CmpFX<CSH_FlowErosion_InitHeight>(
		SimName, TEXT("Result"), W, H, 1);

	std::shared_ptr<RenderMaterial_FX_FlowErosion> SimXf = std::make_shared<RenderMaterial_FX_FlowErosion>(
		SimName, BaseMat->FxMaterial(), InputHeight, NumIters, Strength, Rain, Deposition, SafeHeightScale,
		ChannelThreshold, SafeFeatureScale, ChannelDepthFraction, DrainageExponent, MaxErodePerStep, SedimentCapacity, ThermalRate, W, H);

	JobUPtr JobObj = MakeJob(Cycle, TargetId, std::static_pointer_cast<RenderMaterial_FX>(SimXf));
	JobObj
		->AddArg(std::make_shared<JobArg_Blob>(JobArg_Blob(InputHeight, "InputHeight").WithArrayOfTiles()));
	JobObj->SetTiled(false);

	BufferDescriptor Desc = MakeStateDesc(W, H, 4, SimName);
	TiledBlobPtr FinalState = JobObj->InitResult(SimName, &Desc);
	FinalState->MakeSingleBlob();

	Cycle->AddJob(TargetId, std::move(JobObj));

	Out.ErodedHeight = RunExtractHeight(Cycle, FinalState, TargetId, W, H);
	Out.FlowMap = RunFlowMap(Cycle, FinalState, TargetId, W, H, FlowMapGain);

	return Out;
}
