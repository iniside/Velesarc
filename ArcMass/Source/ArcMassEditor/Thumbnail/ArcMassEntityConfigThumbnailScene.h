// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailHelpers.h"

class AActor;
class IArcMassPreviewContributor;
class UMassEntityConfigAsset;

class FArcMassEntityConfigThumbnailScene : public FThumbnailPreviewScene
{
public:
	FArcMassEntityConfigThumbnailScene();

	/** Rebuilds the preview geometry for the given config (clears any previous). */
	void SetConfig(const UMassEntityConfigAsset* Config);

	/** Tears down contributor components and the owner actor. */
	void Clear();

	/**
	 * True iff any contributor can contribute to any trait on the config. Drives the
	 * thumbnail renderer's CanVisualizeAsset so trait-less configs fall back to the
	 * default class icon instead of rendering a blank frame.
	 */
	static bool HasRenderableVisualization(const UMassEntityConfigAsset* Config);

protected:
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:
	TArray<TUniquePtr<IArcMassPreviewContributor>> Contributors;
	TWeakObjectPtr<AActor> PreviewActor;
	FBoxSphereBounds CachedBounds = FBoxSphereBounds(ForceInit);
};
