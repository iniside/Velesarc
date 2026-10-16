// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGSettings.h"

#include "ArcTerrainScatterSettings.generated.h"

class UArcTerrainScatterKernel;

/**
 * GPU-grass plan Step 3 — the ArcTerrain GPU scatter node.
 *
 * A POINT GENERATOR that nonetheless carries ONE Surface input pin. The terrain samples themselves do
 * NOT travel through that pin — they arrive through UArcTerrainDataInterface, which the kernel
 * attaches as an ADDITIONAL data interface, because PCG's pin->DI selection is a closed if/else in a
 * non-exported free function and a Surface-typed pin would silently bind UPCGTextureDataInterface
 * instead (see UArcTerrainDataInterface). The node's own settings are likewise pushed onto that DI at
 * compute-graph compilation time (UArcTerrainScatterKernel::CreateAdditionalInputDataInterfaces).
 *
 * WHY THE PIN EXISTS ANYWAY — HIERARCHICAL GENERATION, stated precisely. A node's HiGen grid size
 * starts at the GRAPH DEFAULT and is only ever LOWERED by taking the minimum over the nodes upstream
 * of its input pins (UPCGGraph::CalculateNodeGridSizeRecursive_Unsafe, PCGGraph.cpp:2748 onward); the
 * execution side prescribes the same default to any task with no inputs (PCGGraphCompiler.cpp:653-659).
 * So a pin-less node is NOT unbounded — it sits on the graph default — but it is UNREACHABLE by an
 * upstream UPCGHiGenGridSizeSettings node, i.e. its grid can never be authored. Declaring the pin is
 * what makes it addressable. Note that connecting it to Get ArcTerrain Data alone changes nothing:
 * in its default configuration that node has no WIRED input either (UPCGDataFromActorSettings emits an
 * advanced BoundingShape pin, and a DefaultInputLabel pin only when ActorFilter == FromInput —
 * PCGDataFromActor.cpp:293-315), so it resolves to the same default. Only an interposed Grid Size node
 * moves the grid, and a graph that generates as one giant cell is a graph whose own
 * bUseHierarchicalGeneration / HiGenGridSize says so — this pin does not override that.
 *
 * The pin is NOT required (UPCGSettings' default emits a REQUIRED DefaultInputLabel pin of type Any,
 * PCGSettings.cpp:479-487 — permanently unsatisfiable for a generator), and CanCullTaskIfUnwired() is
 * overridden to false, WITHOUT WHICH declaring the pin would silently cull the node in every graph
 * that leaves it unconnected. See that override for the mechanism.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class UArcTerrainScatterSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class UArcTerrainScatterKernel;

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ArcTerrainScatter")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::GPU; }

	/** Without this the "Execute on GPU" checkbox is hidden (its EditCondition is
	 *  DisplayExecuteOnGPUSettingCondition) and bExecuteOnGPU can never be set by an author. */
	virtual bool DisplayExecuteOnGPUSetting() const override { return true; }

	virtual void CreateKernels(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter,
		const UPCGNode* InNode, TArray<UPCGComputeKernel*>& OutKernels,
		TArray<FPCGKernelEdge>& OutEdges) const override;
#endif

	/**
	 * GPU-ONLY node: unlike every engine GPU node, this one has no CPU backend, so it does not
	 * consult bExecuteOnGPU. The single exception is PCG's CPU-only execution switch — honouring it
	 * is what keeps FArcTerrainScatterElement's check() unreachable instead of a crash.
	 */
	virtual bool ShouldExecuteOnGPU() const override;

	/** Enables UPCGSettings::Seed. The base class already owns that UPROPERTY — declaring another
	 *  'Seed' here would shadow it. It is the jitter seed the kernel pushes to the GPU. */
	virtual bool UseSeed() const override { return true; }

	/** false — the In pin is a grid-propagation edge, not a data dependency, so an unconnected one must
	 *  NOT statically cull this node. The base default (true) would. See the .cpp for the mechanism. */
	virtual bool CanCullTaskIfUnwired() const override;

protected:
	//~ Begin UPCGSettings interface
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~ End UPCGSettings interface

public:
	/** Points per square metre of the execution bounds' XY footprint, BEFORE the residency, slope and
	 *  layer filters reject anything. The realised density is always lower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings,
		meta = (PCG_Overridable, ClampMin = "0.000001", UIMin = "0.0", UIMax = "20.0"))
	float Density = 1.0f;

	/** Flattest accepted slope, in degrees from horizontal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings,
		meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "90.0"))
	float MinSlopeDegrees = 0.0f;

	/** Steepest accepted slope, in degrees from horizontal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings,
		meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "90.0"))
	float MaxSlopeDegrees = 90.0f;

	/** Terrain weight channel gating the scatter. NAME_None DISABLES the filter (every point passes);
	 *  a name that does not resolve also disables it AND raises a warning on the node — a silent
	 *  reject-everything is indistinguishable from "no grass was authored". */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName LayerChannel;

	/** Minimum weight of LayerChannel a point must sit on. Only meaningful once LayerChannel resolves. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings,
		meta = (PCG_Overridable, ClampMin = "0.0", ClampMax = "1.0"))
	float MinLayerWeight = 0.0f;

	/** Orient each point's +Z along the terrain normal instead of leaving the identity rotation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bAlignToNormal = false;
};

/**
 * CPU element. Reachable ONLY under PCG's CPU-only execution switch: this node has no CPU
 * implementation, and saying so loudly beats emitting a plausible-but-empty point data that would
 * read as "the terrain has no grass here".
 */
class FArcTerrainScatterElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
