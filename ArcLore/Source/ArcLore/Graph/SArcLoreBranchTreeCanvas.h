// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/ArcLoreBranchTreeLayout.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FPaintArgs;
class FSlateWindowElementList;

/**
 * Free-positioned, pan/zoom branch-TREE canvas for the ArcLore Branch Explorer.
 *
 * One rounded "blob" per branch, connected to its parent by a curved (cubic bezier)
 * colored edge. Modelled on the engine's STreeMap (SLeafWidget + imperative OnPaint +
 * AbsoluteToLocal hit-test) but with no UObject coupling and no TreeMap dependency.
 *
 * Coordinate model (single source of truth — see the file-scoped helpers in the .cpp):
 *   canvas -> widget : WidgetPt = (CanvasPt + PanOffset) * ZoomFactor
 *   widget -> canvas : CanvasPt = LocalPt / ZoomFactor - PanOffset
 *
 * The widget is fed a layout by the host panel (SetLayout). Selection and the
 * context-menu request are surfaced as delegates; the panel builds the actual menu.
 */
class SArcLoreBranchTreeCanvas : public SLeafWidget
{
public:
	/** Fired on LMB-click of a branch blob (panel updates the detail strip + commit drill-down). */
	DECLARE_DELEGATE_OneParam(FOnBranchSelected, const FArcLoreBranchTreeNode&);
	/** Fired on RMB-click of a branch blob (panel builds + pushes the context menu). */
	DECLARE_DELEGATE_OneParam(FOnBranchContextMenu, const FArcLoreBranchTreeNode&);

	SLATE_BEGIN_ARGS(SArcLoreBranchTreeCanvas)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Stores the layout (moved), clamps the selection, rebuilds the id->index map, and repaints. */
	void SetLayout(FArcLoreBranchTreeLayout InLayout);

	/** Selects the node whose TipHash matches (panel uses this to sync selection from elsewhere). */
	void SetSelectedByTipHash(const FString& InTipHash);

	/** Selection / context-menu delegates (panel binds these). */
	FOnBranchSelected OnBranchSelected;
	FOnBranchContextMenu OnBranchContextMenu;

	//~ SWidget overrides
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ End SWidget overrides

private:
	/** Returns the node index under the given canvas-space point, or INDEX_NONE. */
	int32 HitTestNode(const FVector2f& CanvasPt) const;

	/** The tree being drawn. */
	FArcLoreBranchTreeLayout Layout;

	/** Maps a node's BranchId -> its index in Layout.Nodes (rebuilt in SetLayout). */
	TMap<FString, int32> IdToIndex;

	/** Pan offset in canvas space (added before the zoom scale). */
	mutable FVector2f PanOffset = FVector2f::ZeroVector;

	/** Zoom multiplier, clamped to [0.1, 8]. */
	mutable float ZoomFactor = 1.0f;

	/** Set by SetLayout; consumed on the next OnPaint to fit content to the viewport once geometry is known. */
	mutable bool bFramePending = false;

	/** True while a pan drag (with mouse capture) is active. */
	bool bIsPanning = false;

	/** Last local-space mouse position seen during a pan drag. */
	FVector2f LastMousePos = FVector2f::ZeroVector;

	/** Selected node index, or INDEX_NONE. */
	int32 SelectedIndex = INDEX_NONE;

	/** Neutral/white-fill rounded blob brush for ordinary branches (per-node color via MakeBox tint).
	 *  Default-initialized so SNew can default-construct the widget; Construct() overwrites with the
	 *  real fill/outline/size. (FSlateRoundedBoxBrush has no default ctor.) */
	FSlateRoundedBoxBrush NodeBrush{ FLinearColor::White, 8.0f, FVector2f(160.0f, 36.0f) };

	/** Accent-outline rounded blob brush for the current branch (overwritten in Construct()). */
	FSlateRoundedBoxBrush CurrentNodeBrush{ FLinearColor::White, 8.0f, FVector2f(160.0f, 36.0f) };
};
