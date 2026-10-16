// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Compute/DataInterfaces/PCGComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"
#include "RenderGraphFwd.h"
#include "RHIFwd.h"
#include "TerrainGPU/ArcTerrainAtlasLayout.h"

#include "ArcTerrainDataInterface.generated.h"

class AArcTerrain;
class FArcTerrainDataInterfaceParameters;
class UPCGSettings;

/** Hard cap on atlas slices, shared by the shader (the residency bitmask is a uint32 and the
 *  per-slot origin arrays are fixed-size) and by the arc.PCG.TerrainGPU.MaxCells clamp. */
inline constexpr int32 GArcTerrainDIMaxSlots = 16;

/**
 * GPU-grass plan Step 2/3 seam: the scatter node's settings, flattened to POD, carried BY THE DATA
 * INTERFACE.
 *
 * WHY NOT GetProducerKernel()->GetSettings() (which the plan and this step's brief both name, after
 * PCGCullPointsOutsideActorBoundsDataInterface.cpp:59-64): that pattern needs the concrete
 * UArcTerrainScatterSettings type at the cast site, and that class is a Step 3 artifact. Rather than
 * pull Step 3's whole settings UCLASS forward (unverifiable here — nothing would drive it), the
 * kernel PUSHES its resolved values onto the DI in CreateAdditionalInputDataInterfaces, which is the
 * same direction PCGTransformPointsKernel.cpp:59-67 pushes settings into its DI. Every behavioural
 * requirement of the brief is preserved: the values reach HLSL only through this DI, and the FName
 * layer channel is still resolved to an index on the GAME THREAD in the provider's Initialize.
 *
 * BOUNDS ARE NOT HERE. They come from the execution source in Initialize — the same source
 * ComputeOutputBindingDataDesc will size the point buffer from, so CPU count and GPU grid cannot
 * disagree.
 */
USTRUCT()
struct FArcTerrainScatterGpuParams
{
	GENERATED_BODY()

	/**
	 * Points per m^2 of the execution bounds' XY footprint - NOT a resolved point count.
	 *
	 * [Step 3] The kernel fills this struct in CreateAdditionalInputDataInterfaces, which runs at
	 * compute-graph COMPILATION time (once per graph, cached, reused by every partitioned component),
	 * while the bounds are a per-execution fact. A count baked here would be right for at most one
	 * component. The provider turns density + its own bounds into the count through ComputePointCount
	 * - the very function ComputeOutputBindingDataDesc sizes the point buffer with.
	 */
	UPROPERTY()
	float Density = 0.0f;

	UPROPERTY()
	int32 Seed = 0;

	/** Slope acceptance window in DEGREES from horizontal; converted to a cosine pair on the game
	 *  thread (cos is monotonically DECREASING, so the shader's usable range is
	 *  (cos(Max), cos(Min)) — see GetArcScatterSlopeRange in the .ush). */
	UPROPERTY()
	float MinSlopeDegrees = 0.0f;

	UPROPERTY()
	float MaxSlopeDegrees = 90.0f;

	/** NAME_None = layer filter OFF. See ResolveLayerFilterChannel for the tri-state contract. */
	UPROPERTY()
	FName LayerChannel;

	UPROPERTY()
	float MinLayerWeight = 0.0f;

	/** [Step 3] Orient the point's +Z along the terrain normal. Reaches HLSL through
	 *  GetArcScatterAlignToNormal(); the rotation itself is built in the kernel. */
	UPROPERTY()
	bool bAlignToNormal = false;
};

/**
 * Samples ArcTerrain's BAKED height and weight payloads from a PCG GPU kernel, and carries the
 * scatter node's settings into HLSL. The baked tiles — not the editor-only live composite — are the
 * persistent representation, and the textures this interface binds are built from their bytes, so they
 * need no keep-alive pin against the render-target pool.
 *
 * It is an ADDITIONAL data interface (UPCGComputeKernel::CreateAdditionalInputDataInterfaces), never
 * a pin binding: PCG's pin->DI selection is a closed if/else in a non-exported free function
 * (PCGComputeCommon.cpp:505-556), and a Surface-typed pin would silently bind
 * UPCGTextureDataInterface instead of erroring. Consequence: it gets no AddDownstreamInputPin, so
 * GetDownstreamInputPinLabelAliases() is EMPTY and the provider must take world + bounds from the
 * execution source (PCGLandscapeDataInterface.cpp:296-300).
 *
 * NODE-VISIBLE DIAGNOSTICS — THE OBLIGATION THIS CLASS CARRIES.
 * Every PCG_KERNEL_VALIDATION_WARN/_ERR the provider raises reaches the graph editor's node only if
 * PCGComputeCommon's LogKernelWarning/LogKernelError get a NON-NULL UPCGSettings
 * (PCGComputeCommon.cpp:198-218 takes the NodeVisualLogs branch only `if (Context && Settings)`);
 * with a null one it silently degrades to a log line nobody reads, which is precisely the
 * "silently produced no grass" failure these messages exist to prevent.
 *
 * PCG never populates that for an ADDITIONAL data interface: UPCGComputeDataInterface's
 * SetProducerSettings is only called by PCGGraphCompilerGPU for pin-bound DIs (line 1272) and for the
 * debug DI (1797); the additional-DI path (1750-1779) only calls AddAdditionalDataInterface, and it
 * is not callable from outside the PCG module anyway (declared without PCG_API on a MinimalAPI class
 * — PCGComputeDataInterface.h:31). The route that DOES work, and the one this DI relies on, is the
 * PRODUCER KERNEL: UPCGComputeKernel::GetSettings() is PCG_API, and UPCGComputeDataProvider::Initialize
 * copies the DI's producer kernel into the provider. So the producing kernel MUST call
 * SetProducerKernel(this) on this DI (UArcTerrainScatterKernel::CreateAdditionalInputDataInterfaces
 * does), and UArcTerrainDataProvider::ResolveNodeSettings() turns that into the settings pointer,
 * ensure()-ing loudly if it ever comes back null.
 */
UCLASS(ClassGroup = (Procedural))
class UArcTerrainDataInterface : public UPCGComputeDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("ArcTerrain"); }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder,
		FShaderParametersMetadataAllocations& InOutAllocations) const override;
	TCHAR const* GetShaderVirtualPath() const override;
	void GetShaderHash(FString& InOutKey) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider() const override;
	//~ End UComputeDataInterface Interface

	void SetScatterParams(const FArcTerrainScatterGpuParams& InParams) { ScatterParams = InParams; }
	const FArcTerrainScatterGpuParams& GetScatterParams() const { return ScatterParams; }

private:
	/** Set by the producing kernel (Step 3) during compute-graph compilation; serialized with the
	 *  compiled graph, hence UPROPERTY rather than a transient field. */
	UPROPERTY()
	FArcTerrainScatterGpuParams ScatterParams;

	static TCHAR const* TemplateFilePath;
};

/**
 * One cell's copy source, uploaded from ArcTerrain tile BYTES — ArcTerrain hands out no textures, so
 * there is no asset lifetime to outlive any more.
 *
 * The ref is written by the upload render command and read by AllocateResources, both on the render
 * thread and in that order, so FIFO command order is what makes it valid by the time it is registered.
 * The shared box carries it out of a command the game thread does not wait on, and the FTextureRHIRef
 * inside keeps the FRHITexture alive for exactly as long as the proxy holds the box.
 */
struct FArcTerrainDITileTexture
{
	FTextureRHIRef TextureRHI;
};

using FArcTerrainDITileTexturePtr = TSharedPtr<FArcTerrainDITileTexture, ESPMode::ThreadSafe>;

/**
 * Everything the render thread is allowed to see. Captured wholly on the GAME THREAD and passed BY
 * VALUE: no UObject, no UTexture2D/UTexture2DArray, and no FTextureResource* either — only counted
 * references to RHI textures this execution uploaded, plus POD.
 */
struct FArcTerrainDataProxyData
{
	struct FSlot
	{
		/** The cell's height sub-rect of the loaded tile (PF_R32_FLOAT, apron tile). Null = the cell's
		 *  tile is not loaded or could not be cut — a LEGAL hole: the slot's residency bit stays clear
		 *  and the shader answers IsArcTerrainResident() with false. */
		FArcTerrainDITileTexturePtr HeightTile;

		/** The cell's plane of the filter channel (PF_G8). Null when the layer filter is off, the cell's
		 *  weight tile is not loaded, or the tile carries no plane for the channel — the slot's weight
		 *  sample is then 0. */
		FArcTerrainDITileTexturePtr WeightTile;

		/** Slice of WeightTile holding the filter channel. One plane per upload, so this is 0 whenever
		 *  the weight source exists at all. */
		int32 WeightSourceSlice = INDEX_NONE;

		FIntPoint HeightSrcOrigin = FIntPoint::ZeroValue;
		FIntPoint WeightSrcOrigin = FIntPoint::ZeroValue;
		bool bResident = false;
	};

	/** Row-major, one entry per atlas slice; never longer than GArcTerrainDIMaxSlots. */
	TArray<FSlot> Slots;

	FIntPoint AtlasOriginCell = FIntPoint::ZeroValue;
	FIntPoint AtlasCellCount = FIntPoint::ZeroValue;
	FIntPoint HeightSubRect = FIntPoint(1, 1);
	FIntPoint WeightSubRect = FIntPoint(1, 1);

	/** LWC split of the terrain transform (PCGLandscapeDataInterface.cpp:134-141): the matrices are
	 *  TILE-RELATIVE, and LWCTile is that tile. A plain mul() of a world position by a matrix holding
	 *  a multi-kilometre translation is precisely the float32 cancellation this split prevents. */
	FMatrix44f WorldToTerrainLocal = FMatrix44f::Identity;
	FMatrix44f TerrainLocalToWorld = FMatrix44f::Identity;
	FVector3f TerrainInvNonUniformScale = FVector3f::OneVector;
	FVector3f LWCTile = FVector3f::ZeroVector;

	float CellSize = 1.0f;
	float HeightTileRes = 4.0f;
	float WeightTileRes = 4.0f;

	/** Terrain-local cm per source texel (CellSize / QuadsPerCell) — the DIVISOR of the normal's
	 *  central difference, never its tap offset. */
	float TexelToLocal = 1.0f;

	FVector3f ScatterBoundsMin = FVector3f::ZeroVector;
	FVector3f ScatterBoundsExtent = FVector3f::ZeroVector;
	uint32 ScatterPointCount = 0;
	uint32 ScatterSeed = 0;

	/** (cos(MaxSlopeDegrees), cos(MinSlopeDegrees)) — X is the LOWER bound on cos(slope), Y the upper. */
	FVector2f ScatterSlopeRange = FVector2f(-1.0f, 1.0f);

	/** -1 = filter disabled. Resolved on the game thread; the shader never compares names. */
	int32 ScatterLayerChannel = INDEX_NONE;
	float ScatterMinLayerWeight = 0.0f;

	/** 0/1; uint32 because a bool has no defined cbuffer packing. */
	uint32 ScatterAlignToNormal = 0;
};

/** Compute Framework data provider for the ArcTerrain sampler. */
UCLASS()
class UArcTerrainDataProvider : public UPCGComputeDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	virtual void Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding,
		uint64 InInputMask, uint64 InOutputMask) override;
	virtual FComputeDataProviderRenderProxy* GetRenderProxy() override;
	virtual void Reset() override;
	//~ End UComputeDataProvider Interface

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

private:
	/** The UPCGSettings that node-visible validation messages must be attributed to; see the
	 *  NODE-VISIBLE DIAGNOSTICS block on UArcTerrainDataInterface. Never returns silently-null:
	 *  a null result is ensure()d and logged, because it would demote every warning below to log-only. */
	const UPCGSettings* ResolveNodeSettings() const;

	FArcTerrainDataProxyData Data;
};

/** Render-thread half: builds the sub-rect atlas into RDG and stamps the shader parameters. */
class FArcTerrainDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	explicit FArcTerrainDataProviderProxy(FArcTerrainDataProxyData InData)
		: Data(MoveTemp(InData))
	{}

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData) override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

protected:
	using FParameters = FArcTerrainDataInterfaceParameters;

	FArcTerrainDataProxyData Data;

	/** Rebuilt EVERY AllocateResources — never cached across frames. A section rebuild/reload between
	 *  generations allocates a brand-new texture object, so a retained FRHITexture would be sampling a
	 *  freed or superseded resource. */
	FRDGTextureSRVRef HeightAtlasSRV = nullptr;
	FRDGTextureSRVRef WeightAtlasSRV = nullptr;

	/** Bit i set = slice i holds this frame's copied sub-rect for a live cell. */
	uint32 ResidencyMask = 0;
};
