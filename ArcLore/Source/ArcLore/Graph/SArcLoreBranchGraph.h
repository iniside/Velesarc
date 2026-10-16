// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/ArcLoreGraphLayout.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class STableViewBase;
class SHeaderRow;
struct FSlateBrush;

/**
 * A single Graph-column cell: a leaf widget that paints ONLY its own row's slice of
 * the commit DAG (the lane segments passing through this row plus the node dot, if any).
 *
 * Because each cell paints in its own AllottedGeometry there is no scroll math — the
 * drawing is intrinsically synced to the owning list row. The cell is self-contained:
 * it is handed a copy of its FArcLoreGraphRowCell (small) plus the global lane count and
 * row height so ComputeDesiredSize can reserve the full lane gutter width.
 */
class SArcLoreGraphRowCell : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SArcLoreGraphRowCell)
		: _NumLanes(0)
		, _LaneWidth(16.0f)
		, _DotRadius(4.0f)
		, _RowHeight(22.0f)
	{}
		/** This row's paint descriptor (copied by value — it is small). */
		SLATE_ARGUMENT(FArcLoreGraphRowCell, Cell)
		/** Total lane count across the whole layout (drives desired width). */
		SLATE_ARGUMENT(int32, NumLanes)
		/** Horizontal spacing between adjacent lanes, in local units. */
		SLATE_ARGUMENT(float, LaneWidth)
		/** Radius of the commit dot, in local units. */
		SLATE_ARGUMENT(float, DotRadius)
		/** Fixed row height (matches the list's row height). */
		SLATE_ARGUMENT(float, RowHeight)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	FArcLoreGraphRowCell Cell;
	int32 NumLanes = 0;
	float LaneWidth = 16.0f;
	float DotRadius = 4.0f;
	float RowHeight = 22.0f;
};

/**
 * The Branch Graph body: a virtualized SListView of commit rows. The leftmost column of
 * each row is an SArcLoreGraphRowCell that paints that row's lane segments + dot; the
 * remaining columns show message (with branch-ref pills), author, date and short hash.
 *
 * The widget owns the laid-out FArcLoreGraphLayout. SetLayout swaps it, rebuilds the row
 * item source and refreshes the list. The host panel (Step 4) binds OnCommitSelected and
 * OnContextMenuOpening; this widget only forwards selection/right-click.
 */
class SArcLoreBranchGraph : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnCommitSelected, TSharedPtr<FArcLoreCommitNode>);

	SLATE_BEGIN_ARGS(SArcLoreBranchGraph) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Swap the laid-out graph, rebuild the row item source, and refresh the list. */
	void SetLayout(FArcLoreGraphLayout InLayout);

	/** Fired when the list selection changes (the panel inspects the selected commit). */
	FOnCommitSelected OnCommitSelected;

	/** Forwarded to the list so the panel can build a row context menu. */
	FOnContextMenuOpening OnContextMenuOpening;

private:
	TSharedRef<ITableRow> HandleGenerateRow(
		TSharedPtr<FArcLoreCommitNode> InItem,
		const TSharedRef<STableViewBase>& OwnerTable);

	void HandleSelectionChanged(TSharedPtr<FArcLoreCommitNode> InItem, ESelectInfo::Type InSelectInfo);

	TSharedPtr<SWidget> HandleContextMenuOpening();

	TSharedRef<SHeaderRow> MakeGraphHeaderRow() const;

	/** The current laid-out graph (nodes + per-row cells + lane count). */
	FArcLoreGraphLayout Layout;

	/** List item source: one shared node ptr per row, sorted by Row. */
	TArray<TSharedPtr<FArcLoreCommitNode>> RowItems;

	TSharedPtr<SListView<TSharedPtr<FArcLoreCommitNode>>> ListView;

	/** Layout constants shared by the list rows and the graph cells. */
	float LaneWidth = 16.0f;
	float RowHeight = 22.0f;
	float DotRadius = 4.0f;
};
