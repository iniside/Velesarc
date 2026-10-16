// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainGPU/ArcTerrainScatterKernel.h"

#include "TerrainGPU/ArcTerrainDataInterface.h"
#include "TerrainGPU/ArcTerrainScatterSettings.h"

#include "PCGCommon.h"
#include "PCGGraphExecutionStateInterface.h"
#include "PCGPin.h"
#include "PCGPointPropertiesTraits.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/PCGDataDescription.h"
#include "Compute/PCGPinPropertiesGPU.h"

#if WITH_EDITOR
#include "Graph/PCGGPUGraphCompilationContext.h"
#endif

#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTerrainScatterKernel)

#define LOCTEXT_NAMESPACE "ArcTerrainScatterKernel"

/** File-scoped, uniquely named (CLAUDE.md C++ Code Style). */
namespace ArcPCGExtensionTerrainScatterKernelPrivate
{
	TAutoConsoleVariable<int32> CVarMaxPoints(
		TEXT("arc.PCG.TerrainGPU.MaxPoints"),
		500000,
		TEXT("Hard cap on the points one ArcTerrain GPU scatter execution allocates. The cap is part ")
		TEXT("of the buffer sizing, so raising it costs GPU memory on every execution."),
		ECVF_Default);
}

int32 GetArcTerrainScatterMaxPoints()
{
	return FMath::Max(ArcPCGExtensionTerrainScatterKernelPrivate::CVarMaxPoints.GetValueOnAnyThread(), 0);
}

int32 ComputePointCount(float DensityPerSqM, const FBox& Bounds, int32 MaxPoints)
{
	const int32 ClampedMax = FMath::Max(MaxPoints, 0);
	if (Bounds.IsValid == 0 || !(DensityPerSqM > 0.0f) || ClampedMax <= 0)
	{
		return 0;
	}

	// The scatter is a 2D grid over the bounds' XY footprint; Z only ever supplies the probe height.
	const FVector Size = Bounds.GetSize();
	const double AreaSqM = (Size.X / 100.0) * (Size.Y / 100.0);
	if (!(AreaSqM > 0.0))
	{
		return 0;
	}

	// Double throughout: Density * Area overflows int32 long before it overflows a double, and a
	// silently wrapped count would size the point buffer negative.
	const double Count = FMath::RoundToDouble(static_cast<double>(DensityPerSqM) * AreaSqM);

	return static_cast<int32>(FMath::Clamp(Count, 0.0, static_cast<double>(ClampedMax)));
}

FIntPoint ComputeScatterGrid(int32 PointCount)
{
	if (PointCount <= 0)
	{
		return FIntPoint::ZeroValue;
	}

	int32 Nx = FMath::Max(FMath::CeilToInt(FMath::Sqrt(static_cast<double>(PointCount))), 1);

	// EXACT integer fix-up, mirrored verbatim in ArcTerrainScatter.usf. ceil(sqrt()) evaluated in
	// float on the GPU and in double here disagree by one at (and next to) exact squares, and one
	// cell of disagreement in Nx moves EVERY point of the grid.
	while (static_cast<int64>(Nx) * Nx < PointCount)
	{
		++Nx;
	}

	while (Nx > 1 && static_cast<int64>(Nx - 1) * (Nx - 1) >= PointCount)
	{
		--Nx;
	}

	return FIntPoint(Nx, FMath::DivideAndRoundUp(PointCount, Nx));
}

TSharedPtr<const FPCGDataCollectionDesc> UArcTerrainScatterKernel::ComputeOutputBindingDataDesc(
	FName InOutputPinLabel, UPCGDataBinding* InBinding) const
{
	check(InBinding);

	// Single output pin by construction (GetOutputPins).
	if (!ensure(InOutputPinLabel == PCGPinConstants::DefaultOutputLabel))
	{
		return nullptr;
	}

	const UArcTerrainScatterSettings* Settings = Cast<UArcTerrainScatterSettings>(GetSettings());
	if (!ensure(Settings))
	{
		return nullptr;
	}

	// THE SAME bounds authority UArcTerrainDataProvider::Initialize reads (the execution source, not
	// a pin — an additional data interface gets no downstream input pin). Any other source here and
	// the buffer this sizes would disagree with the grid the shader derives from
	// GetArcScatterPointCount().
	FBox Bounds(ForceInit);
	if (const IPCGGraphExecutionSource* ExecutionSource = InBinding->GetExecutionSource())
	{
		Bounds = ExecutionSource->GetExecutionState().GetBounds();
	}

	const int32 PointCount = ComputePointCount(Settings->Density, Bounds, GetArcTerrainScatterMaxPoints());

	TSharedPtr<FPCGDataCollectionDesc> OutDataDesc = FPCGDataCollectionDesc::MakeShared();
	FPCGDataDesc& DataDesc = OutDataDesc->GetDataDescriptionsMutable().Emplace_GetRef(
		EPCGDataType::Point, PointCount);

	// MANDATORY. A non-allocated native property is a Constant Value Range: ONE slot backs all N
	// elements, so without this every Out_SetPosition/Seed/Density would collapse onto element 0 and
	// the whole scatter would be a single point. Bounds/Color/Steepness stay CVR on purpose - they
	// are uniform, and the shader initialises them once at element 0.
	DataDesc.AllocateProperties(EPCGPointNativeProperties::Transform | EPCGPointNativeProperties::Seed
		| EPCGPointNativeProperties::Density);

	return OutDataDesc;
}

int UArcTerrainScatterKernel::ComputeThreadCount(const UPCGDataBinding* InBinding) const
{
	check(InBinding);

	const TSharedPtr<const FPCGDataCollectionDesc> OutputPinDesc = InBinding->GetCachedKernelPinDataDesc(
		this, PCGPinConstants::DefaultOutputLabel, /*bIsInput=*/false);

	return ensure(OutputPinDesc) ? OutputPinDesc->ComputeTotalElementCount() : 0;
}

#if WITH_EDITOR
void UArcTerrainScatterKernel::CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext,
	UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const
{
	Super::CreateAdditionalInputDataInterfaces(InOutContext, InObjectOuter, OutDataInterfaces);

	// PUSH, not pull: the data interface does not cast back to these settings (it would have to know
	// this class), so this is the ONLY route the node's settings have into HLSL - the same direction
	// PCGTransformPointsKernel.cpp:59-67 pushes settings into its DI.
	FArcTerrainScatterGpuParams ScatterParams;
	if (const UArcTerrainScatterSettings* Settings = Cast<UArcTerrainScatterSettings>(GetSettings()))
	{
		// Density, NOT a resolved point count: this runs at compute-graph COMPILATION time, once per
		// graph, while the bounds are per-execution. The provider turns density + its own bounds into
		// the count through ComputePointCount, which is exactly what ComputeOutputBindingDataDesc
		// calls - one rule, one result.
		ScatterParams.Density = Settings->Density;
		ScatterParams.Seed = Settings->Seed;
		ScatterParams.MinSlopeDegrees = Settings->MinSlopeDegrees;
		ScatterParams.MaxSlopeDegrees = Settings->MaxSlopeDegrees;
		ScatterParams.LayerChannel = Settings->LayerChannel;
		ScatterParams.MinLayerWeight = Settings->MinLayerWeight;
		ScatterParams.bAlignToNormal = Settings->bAlignToNormal;
	}

	TObjectPtr<UArcTerrainDataInterface> TerrainDI =
		InOutContext.NewObject_AnyThread<UArcTerrainDataInterface>(InObjectOuter);

	// LOAD-BEARING FOR NODE-VISIBLE DIAGNOSTICS, not just bookkeeping. PCG never calls
	// SetProducerSettings on an ADDITIONAL data interface (PCGGraphCompilerGPU.cpp:1750-1779 only calls
	// AddAdditionalDataInterface; 1272 is the pin-bound path), and that setter is not even callable
	// from outside the PCG module (PCGComputeDataInterface.h:31 - no PCG_API on a MinimalAPI class). The
	// producer KERNEL is therefore the only route: PCG copies it into the provider, and
	// UArcTerrainDataProvider::ResolveNodeSettings() turns it into the UPCGSettings that
	// PCGComputeCommon.cpp:198-218 requires before it will route a warning to the node's visual logs.
	// Drop this line and every ArcTerrain sampler warning silently becomes log-only.
	TerrainDI->SetProducerKernel(this);
	TerrainDI->SetScatterParams(ScatterParams);

	OutDataInterfaces.Add(TerrainDI);
}
#endif // WITH_EDITOR

void UArcTerrainScatterKernel::GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
}

#undef LOCTEXT_NAMESPACE
