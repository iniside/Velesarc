// Copyright Epic Games, Inc. All Rights Reserved.
#include "T_ThermalErosion.h"
#include "TextureGraphEngine.h"
#include "Job/JobArgs.h"
#include "Job/JobBatch.h"
#include "FxMat/MaterialManager.h"
#include "FxMat/RenderMaterial_FX.h"
#include "Model/Mix/MixInterface.h"
#include "Data/TiledBlob.h"

IMPLEMENT_GLOBAL_SHADER(CSH_Thermal, "/Plugin/ArcEditorTools/ArcThermalErosion.usf", "CSH_Thermal", SF_Compute);

T_ThermalErosion::T_ThermalErosion()
{
}

T_ThermalErosion::~T_ThermalErosion()
{
}

//////////////////////////////////////////////////////////////////////////
/// File-scoped helpers — unique namespace; do NOT reuse ArcFlowErosion_Internal
//////////////////////////////////////////////////////////////////////////
namespace ArcThermalErosion_Internal
{
	/// Subclass to flip the protected bIsNoCache flag, keeping per-iteration blobs out of the Blobber cache.
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

	/// Builds a single-channel (ItemsPerPoint=1) whole-domain buffer descriptor.
	/// Uses AddMetadata("FX:UAV") instead of AllowUAV() — the latter references
	/// a non-exported static (RawBufferMetadataDefs::G_FX_UAV) and fails to link externally.
	static BufferDescriptor MakeStateDesc(int32 InWidth, int32 InHeight, int32 InItemsPerPoint, const FString& InName)
	{
		BufferDescriptor Desc;
		Desc.Width          = (uint32)InWidth;
		Desc.Height         = (uint32)InHeight;
		Desc.Format         = BufferFormat::Float;
		Desc.ItemsPerPoint  = (uint32)InItemsPerPoint;
		Desc.Name           = InName;
		/// Equivalent to Desc.AllowUAV() without referencing the non-exported static symbol.
		Desc.AddMetadata(FString(TEXT("FX:UAV")));
		return Desc;
	}

	/// One thermal erosion pass: InputHeight SRV -> R32F height Result UAV.
	static TiledBlobPtr RunThermal(MixUpdateCyclePtr Cycle, TiledBlobPtr InputHeight,
		int32 TargetId, int32 InWidth, int32 InHeight, float Strength, int32 Iter)
	{
		FString Name = FString::Printf(TEXT("ThermalErosion.%d"), Iter);

		RenderMaterial_FXPtr Transform = TextureGraphEngine::GetMaterialManager()->CreateMaterial_CmpFX<CSH_Thermal>(
			Name, TEXT("Result"), InWidth, InHeight, 1);

		JobUPtr JobObj = MakeJob(Cycle, TargetId, Transform);
		JobObj
			->AddArg(std::make_shared<JobArg_Blob>(JobArg_Blob(InputHeight, "InputHeight").WithNotHandleTiles()))
			->AddArg(ARG_INT(InWidth,   "Width"))
			->AddArg(ARG_INT(InHeight,  "Height"))
			->AddArg(ARG_FLOAT(Strength, "Strength"));
		JobObj->SetTiled(false);

		BufferDescriptor Desc = MakeStateDesc(InWidth, InHeight, 1, Name);
		TiledBlobPtr Result = JobObj->InitResult(Name, &Desc);
		Result->MakeSingleBlob();

		Cycle->AddJob(TargetId, std::move(JobObj));
		return Result;
	}
}

TiledBlobPtr T_ThermalErosion::Create(MixUpdateCyclePtr Cycle, TiledBlobPtr InputHeight,
	int32 Iterations, float Strength, int32 TargetId)
{
	using namespace ArcThermalErosion_Internal;

	if (!InputHeight)
		return TiledBlobPtr();

	const int32 W = (int32)InputHeight->GetWidth();
	const int32 H = (int32)InputHeight->GetHeight();

	if (W <= 0 || H <= 0)
		return InputHeight;

	const int32 NumIters = FMath::Clamp(Iterations, 1, 1000);

	/// Ping-pong: each pass feeds the next.
	TiledBlobPtr Current = InputHeight;
	for (int32 i = 0; i < NumIters; ++i)
	{
		Current = RunThermal(Cycle, Current, TargetId, W, H, Strength, i);
	}

	return Current;
}
