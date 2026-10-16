/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"

#include "ArcSvgAssetThumbnailRenderer.generated.h"

class FCanvas;
class FRenderTarget;

/**
 * Thumbnail renderer for UArcSvgAsset.
 *
 * Rasterizes the stored SVG bytes via nanosvg directly in the Draw() call and uploads the
 * result as a transient BGRA texture, then composites it onto the thumbnail canvas.
 * Frequency is OnAssetSave — SVG art is static, no need to re-render every frame.
 */
UCLASS()
class UArcSvgAssetThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual bool CanVisualizeAsset(UObject* Object) override;
	virtual EThumbnailRenderFrequency GetThumbnailRenderFrequency(UObject* Object) const override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
	                  FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;
};
