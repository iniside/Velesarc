// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"

#include "ArcMassEntityConfigThumbnailRenderer.generated.h"

class FArcMassEntityConfigThumbnailScene;
class FCanvas;
class FRenderTarget;

UCLASS()
class UArcMassEntityConfigThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()
public:
	virtual bool CanVisualizeAsset(UObject* Object) override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
	                  FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	virtual EThumbnailRenderFrequency GetThumbnailRenderFrequency(UObject* Object) const override;
	virtual void BeginDestroy() override;

private:
	TUniquePtr<FArcMassEntityConfigThumbnailScene> ThumbnailScene;
};
