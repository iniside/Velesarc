// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Compute/PCGComputeKernel.h"
#include "Math/Box.h"

#include "ArcTerrainScatterKernel.generated.h"

class UPCGDataBinding;
struct FPCGDataCollectionDesc;
struct FPCGGPUCompilationContext;
struct FPCGPinProperties;
struct FPCGPinPropertiesGPU;

/**
 * GPU-grass plan Step 3 — the CPU/GPU SHARED ARITHMETIC.
 *
 * These are the authority for how many points one execution scatters and how they are laid out. The
 * shader recomputes the grid from GetArcScatterPointCount() / GetArcScatterBoundsMin/Extent(), so
 * both sides must derive it from one rule; ComputeScatterGrid's integer fix-up loop exists precisely
 * so a float-vs-double ceil(sqrt()) disagreement at exact squares cannot shift every point by a cell.
 * Exported (a later [test-author] step drives them directly).
 */

/** Points per m^2 x the bounds' XY footprint in m^2, rounded, clamped to [0, MaxPoints]. Returns 0
 *  for invalid bounds, a non-positive density or a non-positive cap. */
ARCPCGEXTENSION_API int32 ComputePointCount(float DensityPerSqM, const FBox& Bounds, int32 MaxPoints);

/** Nx = smallest integer with Nx*Nx >= PointCount, Ny = ceil(PointCount / Nx). (0,0) for PointCount <= 0. */
ARCPCGEXTENSION_API FIntPoint ComputeScatterGrid(int32 PointCount);

/** arc.PCG.TerrainGPU.MaxPoints (default 500000). The data-interface provider and the kernel MUST
 *  read the cap through this one accessor: they size the same buffer from the same bounds. */
ARCPCGEXTENSION_API int32 GetArcTerrainScatterMaxPoints();

/**
 * Point-generator compute kernel: ZERO input pins (legal - EPCGKernelType::PointGenerator; the
 * terrain, the bounds and the node settings all arrive through UArcTerrainDataInterface, attached in
 * CreateAdditionalInputDataInterfaces), one Point output pin.
 */
UCLASS()
class UArcTerrainScatterKernel : public UPCGComputeKernel
{
	GENERATED_BODY()

public:
	//~ Begin UPCGComputeKernel interface
	virtual TSharedPtr<const FPCGDataCollectionDesc> ComputeOutputBindingDataDesc(FName InOutputPinLabel,
		UPCGDataBinding* InBinding) const override;
	virtual int ComputeThreadCount(const UPCGDataBinding* InBinding) const override;

#if WITH_EDITOR
	virtual const TCHAR* GetSourceFilePath() const override
	{
		return TEXT("/Plugin/ArcPCGExtension/Private/ArcTerrainScatter.usf");
	}

	virtual FString GetEntryPoint() const override { return TEXT("ArcTerrainScatterCS"); }

	virtual void CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext,
		UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const override;
#endif

	/** Empty on purpose: this kernel generates its points, it does not transform any. */
	virtual void GetInputPins(TArray<FPCGPinProperties>& OutPins) const override {}
	virtual void GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const override;
	//~ End UPCGComputeKernel interface
};
