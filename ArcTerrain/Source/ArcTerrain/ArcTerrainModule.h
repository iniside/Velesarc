// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"

class FArcTerrainRuntimeWorkCoordinator;

ARCTERRAIN_API DECLARE_LOG_CATEGORY_EXTERN(LogArcTerrain, Log, All);

namespace ArcTerrainDiag
{
	/** arc.Terrain.Weights.VerboseComposite != 0 — weight-composite DIAGNOSTICS ONLY (default 0). When
	 *  on, BuildCompositeJob logs each weight layer it DROPS (channel not in ChannelSet, no bound weight-RT
	 *  slot + no carve, or GPU source unresolved) and each weight pass it EMITS. Pure logging — reading it
	 *  changes no behaviour. Enable it to diagnose a stamped weight layer that never reaches the material. */
	ARCTERRAIN_API bool IsWeightCompositeVerbose();

	/** arc.Terrain.Weights.ProbeAtlasReadback != 0 — weight-atlas readback DIAGNOSTICS ONLY (default 0).
	 *  When on, arc.Terrain.DiagAt also reads back the one weight texel the material samples at the probed
	 *  position and logs every channel plus their sum. The readback blocks the render thread and the GPU,
	 *  so it is gated to that explicit console probe and never runs on the fill path. */
	ARCTERRAIN_API bool IsAtlasReadbackProbeEnabled();

	/** arc.Terrain.Material.VerboseLayerBlend != 0— layer-blend DIAGNOSTICS ONLY (default 0). When on,
	 *  UMaterialExpressionArcTerrainLayerBlend::Compile() and UMaterialExpressionArcTerrainLayerWeight::
	 *  Compile() log each authored layer's resolved channel -> ChannelSet index -> atlas array/component
	 *  mapping. Pure logging — reading it changes no behaviour. */
	ARCTERRAIN_API bool IsLayerBlendVerbose();

	/** arc.Terrain.Atlas.VerboseFacade != 0 — atlas-facade DIAGNOSTICS ONLY (default 0). Changes no
	 *  behaviour. When on, every FArcTerrainAtlasTextureResource::SetAtlasRHI_RenderThread call logs. When
	 *  off (the default) each facade instance still logs the first time and again whenever the state it
	 *  reports CHANGES; an unchanged repeat stays silent. InitRHI and UArcTerrainAtlasTexture::
	 *  CreateResource always log — they run once per resource. */
	ARCTERRAIN_API bool IsAtlasFacadeVerbose();
}

class FArcTerrainModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FArcTerrainRuntimeWorkCoordinator* GetRuntimeCoordinator() const
	{
		return RuntimeCoordinator.Get();
	}

private:
	bool TickRuntimeCoordinator(float DeltaTime);

	TSharedPtr<FArcTerrainRuntimeWorkCoordinator, ESPMode::ThreadSafe> RuntimeCoordinator;
	FTSTicker::FDelegateHandle RuntimeTickerHandle;
};
