#include "ArcTerrainModule.h"
#include "ArcTerrainTileAccess.h"
#include "ArcTerrainTileUpload.h"
#include "Render/ArcTerrainCBTRenderer.h"
#include "Runtime/ArcTerrainRuntimeWorkCoordinator.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "RenderingThread.h"
#include "ShaderCore.h"
#include "Misc/App.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogArcTerrain);

namespace ArcTerrainDiag
{
	static TAutoConsoleVariable<int32> CVarWeightCompositeVerbose(
		TEXT("arc.Terrain.Weights.VerboseComposite"), 0,
		TEXT("Weight-composite DIAGNOSTICS: 1 = per-cell, per-weight-layer trace of BuildCompositeJob's ")
		TEXT("weight loop — logs when a layer is DROPPED (channel not in the terrain's ChannelSet, no ")
		TEXT("bound weight-RT slot + no carve, or GPU source failed to resolve) and when a weight pass ")
		TEXT("IS emitted (resolved ChannelIndex/RT slot). Use it to see why a stamped weight layer never ")
		TEXT("reaches the material. Logging only — no behaviour changes. Default 0 (zero cost when off)."));

	bool IsWeightCompositeVerbose()
	{
		return CVarWeightCompositeVerbose.GetValueOnGameThread() != 0;
	}

	static TAutoConsoleVariable<int32> CVarProbeAtlasReadback(
		TEXT("arc.Terrain.Weights.ProbeAtlasReadback"), 0,
		TEXT("Weight-atlas readback DIAGNOSTICS: 1 = every arc.Terrain.DiagAt also reads back the single ")
		TEXT("weight texel the material samples for that position, out of the resident weight-atlas slice ")
		TEXT("(or the FILL slice, which it names), and logs every channel as raw uint8 + 0..1 float plus ")
		TEXT("their sum. Blocks the render thread and the GPU for the duration of the probe, so it runs ")
		TEXT("ONLY on an explicit DiagAt and never on the per-frame fill path. Logging only — no behaviour ")
		TEXT("changes. Default 0 (zero cost when off)."));

	bool IsAtlasReadbackProbeEnabled()
	{
		return CVarProbeAtlasReadback.GetValueOnGameThread() != 0;
	}

	static TAutoConsoleVariable<int32> CVarLayerBlendVerbose(
		TEXT("arc.Terrain.Material.VerboseLayerBlend"), 0,
		TEXT("Layer-blend DIAGNOSTICS: 1 = logs, for every UMaterialExpressionArcTerrainLayerBlend::Compile() ")
		TEXT("and UMaterialExpressionArcTerrainLayerWeight::Compile() call, each authored layer's resolved ")
		TEXT("channel -> ChannelSet index -> ArcTerrainWeights array/component mapping, its BlendType, and ")
		TEXT("whether its LayerInput/HeightInput pins are connected. Use it to see whether a channel maps to ")
		TEXT("the atlas slot/lane you expect. Logging only — no behaviour changes. Default 0 (zero cost when off)."));

	bool IsLayerBlendVerbose()
	{
		return CVarLayerBlendVerbose.GetValueOnAnyThread() != 0;
	}

	static TAutoConsoleVariable<int32> CVarAtlasFacadeVerbose(
		TEXT("arc.Terrain.Atlas.VerboseFacade"), 0,
		TEXT("Atlas-facade DIAGNOSTICS: 1 = every FArcTerrainAtlasTextureResource::SetAtlasRHI_RenderThread ")
		TEXT("call logs, once per call. When OFF (the default) each facade instance logs it only the first ")
		TEXT("time and again whenever the state it reports CHANGES; an unchanged repeat stays silent. ")
		TEXT("InitRHI and UArcTerrainAtlasTexture::CreateResource always log — they run once per resource. ")
		TEXT("Logging only — no behaviour changes. Default 0 (zero cost when off)."));

	bool IsAtlasFacadeVerbose()
	{
		return CVarAtlasFacadeVerbose.GetValueOnAnyThread() != 0;
	}
}

void FArcTerrainModule::StartupModule()
{
	const FString VirtualShaderDir = TEXT("/Plugin/ArcTerrain");
	if (!AllShaderSourceDirectoryMappings().Contains(VirtualShaderDir))
	{
		const FString ShaderDir = FPaths::Combine(
			IPluginManager::Get().FindPlugin(TEXT("ArcTerrain"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(VirtualShaderDir, ShaderDir);
	}

	RuntimeCoordinator = MakeShared<FArcTerrainRuntimeWorkCoordinator, ESPMode::ThreadSafe>();
	RuntimeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FArcTerrainModule::TickRuntimeCoordinator), 0.0f);
}

bool FArcTerrainModule::TickRuntimeCoordinator(float DeltaTime)
{
	if (RuntimeCoordinator.IsValid())
	{
		RuntimeCoordinator->BeginFrameAndPump(GFrameCounter);
	}
	return true;
}

void FArcTerrainModule::ShutdownModule()
{
	FTSTicker::RemoveTicker(RuntimeTickerHandle);
	RuntimeTickerHandle.Reset();
	if (RuntimeCoordinator.IsValid())
	{
		RuntimeCoordinator->ShutdownAndWait();
		RuntimeCoordinator.Reset();
	}

	ArcTerrainTileAccess::ReleaseAllTiles();

	ENQUEUE_RENDER_COMMAND(ArcTerrainUnregisterCBTRendererExtension)(
		[](FRHICommandListImmediate&)
		{
			GArcTerrainCBTRendererExtension.UnregisterExtension();
		});

	// The staging pool holds FTextureRHIRefs in a namespace-scope map, which static destruction would run
	// after RHIExit.
	ENQUEUE_RENDER_COMMAND(ArcTerrainReleaseStagingPool)(
		[](FRHICommandListImmediate&)
		{
			ArcTerrainTileUpload::ReleaseStagingPool();
		});
	FlushRenderingCommands();
}

IMPLEMENT_MODULE(FArcTerrainModule, ArcTerrain)
