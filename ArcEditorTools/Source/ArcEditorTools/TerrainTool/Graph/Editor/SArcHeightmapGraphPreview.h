// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Spatial/SampledScalarField2.h"
#include "TerrainTool/Graph/ArcHeightmapGraphEval.h"

class FDeferredCleanupSlateBrush;

template <typename OptionType>
class SComboBox;

/**
 * On-demand grayscale preview of a heightmap graph's output field.
 *
 * Renders an FSampledScalarField2f into a transient UTexture2D (cell values
 * min/max normalised to 0..255, written as BGRA grayscale) and wraps it in an
 * FDeferredCleanupSlateBrush, which GC-roots the texture and defers its release
 * through the Slate render pipeline. Until the first SetField() the widget shows
 * an empty-state prompt; afterwards it shows the rendered texture.
 *
 * SetResult() additionally populates a layer-selector combo box that lets the
 * user pick between the heightfield and any named weight-map layers.
 */
class SArcHeightmapGraphPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcHeightmapGraphPreview) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Render Field into the preview texture and refresh the displayed image. */
	void SetField(const UE::Geometry::FSampledScalarField2f& Field);

	/** Store the full multi-output result (heightfield + named layers), rebuild the selector,
	 *  and render the currently-selected entry. */
	void SetResult(const FArcHeightmapEvalResult& Result);

	/** Toggle the "evaluating" overlay (indeterminate progress bar) shown while a worker eval runs. */
	void SetEvaluating(bool bInEvaluating);

private:
	/** Rebuild the texture/brush from whichever entry SelectedOption points at. */
	void RenderSelected();

	/** Brush wrapping the transient preview texture; null until the first evaluation. Owns the
	 *  texture's GC + render-pipeline lifetime so reassigning it is safe mid-frame. */
	TSharedPtr<FDeferredCleanupSlateBrush> PreviewBrush;

	/** True while a background graph evaluation is in flight; drives the progress overlay. */
	bool bEvaluating = false;

	// ---- Layer-selector state ----

	/** Option list backing the combo box.  NAME_None = heightfield ("Height"); any other name = a weight-map layer. */
	TArray<TSharedPtr<FName>> PreviewOptions;

	/** Copy of the latest FArcHeightmapEvalResult.Layers. */
	TMap<FName, UE::Geometry::FSampledScalarField2f> PreviewLayers;

	/** Copy of the latest FArcHeightmapEvalResult.Height. */
	UE::Geometry::FSampledScalarField2f PreviewHeight;

	/** Currently selected combo entry; null = nothing selected yet. */
	TSharedPtr<FName> SelectedOption;

	/** The combo box widget; assigned in Construct, populated/refreshed in SetResult. */
	TSharedPtr<SComboBox<TSharedPtr<FName>>> SelectorCombo;
};
