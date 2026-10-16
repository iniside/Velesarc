// Copyright Lukasz Baran. All Rights Reserved.

#include "Thumbnail/ArcMassEntityConfigThumbnailRenderer.h"

#include "Thumbnail/ArcMassEntityConfigThumbnailScene.h"
#include "MassEntityConfigAsset.h"

#include "RenderingThread.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "ShowFlags.h"
#include "ThumbnailRendering/ThumbnailRenderer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassEntityConfigThumbnailRenderer)

bool UArcMassEntityConfigThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	UMassEntityConfigAsset* Config = Cast<UMassEntityConfigAsset>(Object);
	return Config != nullptr && FArcMassEntityConfigThumbnailScene::HasRenderableVisualization(Config);
}

EThumbnailRenderFrequency UArcMassEntityConfigThumbnailRenderer::GetThumbnailRenderFrequency(UObject* Object) const
{
	return EThumbnailRenderFrequency::OnPropertyChange;
}

void UArcMassEntityConfigThumbnailRenderer::BeginDestroy()
{
	if (ThumbnailScene)
	{
		FlushRenderingCommands();
		ThumbnailScene.Reset();
	}

	Super::BeginDestroy();
}

void UArcMassEntityConfigThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
                                                 FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UMassEntityConfigAsset* Config = Cast<UMassEntityConfigAsset>(Object);
	if (Config == nullptr || !FArcMassEntityConfigThumbnailScene::HasRenderableVisualization(Config))
	{
		// No renderable visualization — leave the canvas as-is so UThumbnailManager falls
		// back to the default class icon. Do NOT call Super::Draw: UDefaultSizedThumbnailRenderer
		// doesn't override Draw and UThumbnailRenderer::Draw is PURE_VIRTUAL (LowLevelFatalError).
		return;
	}

	if (ThumbnailScene == nullptr || ensure(ThumbnailScene->GetWorld() != nullptr) == false)
	{
		if (ThumbnailScene)
		{
			FlushRenderingCommands();
			ThumbnailScene.Reset();
		}
		ThumbnailScene = MakeUnique<FArcMassEntityConfigThumbnailScene>();
	}

	ThumbnailScene->SetConfig(Config);
	ThumbnailScene->GetScene()->UpdateSpeedTreeWind(0.0);

	FSceneViewFamilyContext ViewFamily(
		FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
			.SetTime(GetTime())
			.SetAdditionalViewFamily(bAdditionalViewFamily));

	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.LOD = 0;

	RenderViewFamily(Canvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height));

	// Release the spawned contributor components after render (mirrors the item renderer's
	// SetStaticMesh(nullptr)) so the offscreen scene doesn't retain per-asset geometry.
	ThumbnailScene->Clear();
}
