// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainGPU/ArcTerrainScatterSettings.h"

#include "TerrainGPU/ArcTerrainScatterKernel.h"

#include "PCGCommon.h"
#include "PCGContext.h"
#include "PCGModule.h"
#include "PCGPin.h"

#if WITH_EDITOR
#include "Compute/PCGKernelHelpers.h"
#include "Graph/PCGGPUGraphCompilationContext.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTerrainScatterSettings)

#define LOCTEXT_NAMESPACE "ArcTerrainScatterSettings"

#if WITH_EDITOR
FText UArcTerrainScatterSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "ArcTerrain Scatter (GPU)");
}

FText UArcTerrainScatterSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Scatters points on the ArcTerrain surface directly on the GPU, sampling the terrain's baked "
		"per-cell height and weight tiles (identical in editor, PIE and a cooked build). Generates its "
		"own points: its extent is the execution source's bounds (the PCG component / partition cell), "
		"and the Surface input pin carries no data — connect it only to place the node on a "
		"Hierarchical Generation grid.");
}

void UArcTerrainScatterSettings::CreateKernels(FPCGGPUCompilationContext& InOutContext,
	UObject* InObjectOuter, const UPCGNode* InNode, TArray<UPCGComputeKernel*>& OutKernels,
	TArray<FPCGKernelEdge>& OutEdges) const
{
	PCGKernelHelpers::FCreateKernelParams CreateParams(InObjectOuter, this, InNode);

	// The KERNEL declares no input pins (the node's In pin is a graph-topology edge only — nothing is
	// uploaded through it), and FCreateKernelParams defaults NodeInputPinsToWire to
	// { DefaultInputLabel }. Leaving that would wire an edge into a kernel pin that does not exist —
	// and this empty list, not UPCGComputeKernel::UploadInputPinDataToGPU, is what keeps the GPU
	// compiler from manufacturing a data interface for the pin (PCGGraphCompilerGPU.cpp:605, :1388).
	CreateParams.NodeInputPinsToWire.Empty();

	PCGKernelHelpers::CreateKernel<UArcTerrainScatterKernel>(InOutContext, CreateParams, OutKernels, OutEdges);
}
#endif // WITH_EDITOR

bool UArcTerrainScatterSettings::ShouldExecuteOnGPU() const
{
#if WITH_EDITOR
	// The one case where the CPU element may legally run: PCG's global CPU-only switch. Everywhere
	// else this node is GPU-only regardless of bExecuteOnGPU.
	if (FPCGModule::IsCPUOnlyExecution())
	{
		return false;
	}
#endif

	return true;
}

TArray<FPCGPinProperties> UArcTerrainScatterSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	// Deliberately NOT SetRequiredPin(): the kernel reads nothing from this pin, so requiring a
	// connection would make an otherwise-working node unsatisfiable. Note that non-required is NOT on
	// its own enough to keep the node alive unconnected — see CanCullTaskIfUnwired below.
	FPCGPinProperties& InPin = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel,
		EPCGDataType::Surface, /*bInAllowMultipleConnections=*/false, /*bAllowMultipleData=*/false);
#if WITH_EDITORONLY_DATA
	InPin.Tooltip = LOCTEXT("InPinTooltip", "Surface this scatter belongs to — connect Get ArcTerrain Data. The terrain samples do NOT travel through this pin (they reach the GPU through the node's own data interface); the edge exists so an upstream Grid Size node can place this node on a Hierarchical Generation grid. Leaving it unconnected is legal and the node still generates.");
#endif

	return PinProperties;
}

bool UArcTerrainScatterSettings::CanCullTaskIfUnwired() const
{
	// MUST be false. UPCGSettings defaults this to true, and FPCGGraphCompiler::
	// CalculateStaticallyActiveRecursive then culls any task that HAS a non-advanced input pin but no
	// active input on it (PCGGraphCompiler.cpp:744-746, 808-812; culling itself at :1394, enabled by
	// default via pcg.GraphExecution.TaskStaticCulling). Before the In pin existed the node had no
	// non-advanced pins at all and that clause could never fire. Adding the pin without this override
	// would silently drop the node from every graph where the pin is left unconnected — no points, no
	// error, not even this node's own PCGE_LOG, because the element never runs.
	return false;
}

TArray<FPCGPinProperties> UArcTerrainScatterSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

	return PinProperties;
}

FPCGElementPtr UArcTerrainScatterSettings::CreateElement() const
{
	return MakeShared<FArcTerrainScatterElement>();
}

bool FArcTerrainScatterElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FArcTerrainScatterElement::ExecuteInternal);

	const UArcTerrainScatterSettings* Settings = Context->GetInputSettings<UArcTerrainScatterSettings>();
	check(Settings && !Settings->ShouldExecuteOnGPU());

	PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoCPUImplementation",
		"ArcTerrain Scatter has no CPU implementation - it samples the terrain's GPU render targets. "
		"This node cannot run while PCG is in CPU-only execution mode."));

	return true;
}

#undef LOCTEXT_NAMESPACE
