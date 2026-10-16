// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/SArcLoreBranchGraph.h"

#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"
#include "Misc/DateTime.h"

#define LOCTEXT_NAMESPACE "SArcLoreBranchGraph"

// Unique, file-scoped namespace (CLAUDE.md: cpp free functions / helpers in a unique named namespace).
namespace ArcLoreBranchGraph_Internal
{
	// Column ids. The Graph column hosts the custom paint cell; the rest are plain text/pills.
	const FName ColumnGraph("Graph");
	const FName ColumnMessage("Message");
	const FName ColumnAuthor("Author");
	const FName ColumnDate("Date");
	const FName ColumnRevision("Revision");

	// Line thickness for lane edges. Native dashed lines are not available, so merge edges and
	// dangling stubs are distinguished by tint/opacity rather than dash pattern.
	const float EdgeThickness = 2.0f;

	// Maximum branch-ref pills rendered before collapsing the remainder into a "+N" indicator.
	const int32 MaxVisiblePills = 2;

	/** Resolve a filled, tintable round dot brush: prefer Epic's commit-graph circle, fall back to
	 *  the Starship filled-circle if the revision-control style isn't available. */
	const FSlateBrush* GetDotBrush()
	{
		const FSlateBrush* RcBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.BranchPoint_Circle");
		if (RcBrush != nullptr)
		{
			return RcBrush;
		}
		return FAppStyle::GetBrush("Icons.FilledCircle");
	}

	/** Center X (local space) of a given lane. */
	float LaneCenterX(int32 Lane, float LaneWidth)
	{
		return Lane * LaneWidth + LaneWidth * 0.5f;
	}

	/** A compact branch-ref pill matching the History tab's "Local" badge idiom. */
	TSharedRef<SWidget> MakeBranchPill(const FString& BranchName, bool bIsCurrentTip)
	{
		// Current branch tip uses the accent yellow; other refs a subdued accent so the
		// current branch reads first.
		const FSlateColor PillColor = bIsCurrentTip
			? FSlateColor(FStyleColors::AccentYellow)
			: FSlateColor(FStyleColors::AccentBlue);

		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(PillColor)
			.Padding(FMargin(4.0f, 1.0f))
			.ToolTipText(FText::FromString(BranchName))
			[
				SNew(STextBlock)
				.Text(FText::FromString(BranchName))
				.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
				.ColorAndOpacity(FStyleColors::Black)
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			];
	}

	/**
	 * One commit row. A single SMultiColumnTableRow subclass whose GenerateWidgetForColumn
	 * switches on the column id: the Graph column hosts the custom paint cell (fed this row's
	 * resolved FArcLoreGraphRowCell copy + the global lane count so it is self-contained); the
	 * rest are single-line, clipped text columns plus branch-ref pills on the message column.
	 */
	class SArcLoreCommitRow : public SMultiColumnTableRow<TSharedPtr<FArcLoreCommitNode>>
	{
	public:
		// 2-template-arg type aliased to dodge the Slate-macro comma gotcha in SLATE_ARGUMENT.
		typedef TSharedPtr<FArcLoreCommitNode> FNodePtr;

		SLATE_BEGIN_ARGS(SArcLoreCommitRow)
			: _NumLanes(0)
			, _LaneWidth(16.0f)
			, _RowHeight(22.0f)
			, _DotRadius(4.0f)
		{}
			SLATE_ARGUMENT(FNodePtr, Node)
			/** This row's resolved paint descriptor (copied by value into the graph cell). */
			SLATE_ARGUMENT(FArcLoreGraphRowCell, Cell)
			SLATE_ARGUMENT(int32, NumLanes)
			SLATE_ARGUMENT(float, LaneWidth)
			SLATE_ARGUMENT(float, RowHeight)
			SLATE_ARGUMENT(float, DotRadius)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
		{
			Node = InArgs._Node;
			Cell = InArgs._Cell;
			NumLanes = InArgs._NumLanes;
			LaneWidth = InArgs._LaneWidth;
			RowHeight = InArgs._RowHeight;
			DotRadius = InArgs._DotRadius;

			SMultiColumnTableRow<TSharedPtr<FArcLoreCommitNode>>::Construct(
				FSuperRowType::FArguments(), InOwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			if (!Node.IsValid())
			{
				return SNullWidget::NullWidget;
			}

			if (InColumnName == ColumnGraph)
			{
				return GenerateGraphColumn();
			}
			if (InColumnName == ColumnMessage)
			{
				return GenerateMessageColumn();
			}
			if (InColumnName == ColumnAuthor)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Node->Author))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis);
			}
			if (InColumnName == ColumnDate)
			{
				// Lore timestamps are milliseconds; divide by 1000 only for display.
				const FDateTime When = FDateTime::FromUnixTimestamp(Node->TimestampMs / 1000);
				return SNew(STextBlock)
					.Text(FText::AsDateTime(When))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis);
			}
			if (InColumnName == ColumnRevision)
			{
				const FString ShortHash = Node->Hash.Left(8);
				return SNew(STextBlock)
					.Text(FText::FromString(ShortHash))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.ToolTipText(FText::FromString(Node->Hash))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis);
			}

			return SNullWidget::NullWidget;
		}

	private:
		TSharedRef<SWidget> GenerateGraphColumn()
		{
			return SNew(SArcLoreGraphRowCell)
				.Cell(Cell)
				.NumLanes(NumLanes)
				.LaneWidth(LaneWidth)
				.RowHeight(RowHeight)
				.DotRadius(DotRadius);
		}

		TSharedRef<SWidget> GenerateMessageColumn()
		{
			// Current-branch tip messages render bold so the live branch reads first.
			const FSlateFontInfo MessageFont = Node->bIsCurrentTip
				? FAppStyle::Get().GetFontStyle("BoldFont")
				: FAppStyle::Get().GetFontStyle("NormalFont");

			const TSharedRef<SHorizontalBox> MessageRowBox = SNew(SHorizontalBox);

			// Branch-ref pills (clipped to the fixed-height strip), then a "+N" overflow text.
			const int32 NumRefs = Node->BranchRefs.Num();
			const int32 NumPills = FMath::Min(NumRefs, MaxVisiblePills);
			for (int32 PillIndex = 0; PillIndex < NumPills; ++PillIndex)
			{
				MessageRowBox->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						MakeBranchPill(Node->BranchRefs[PillIndex], Node->bIsCurrentTip)
					];
			}

			if (NumRefs > NumPills)
			{
				MessageRowBox->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("PillOverflow", "+{0}"), FText::AsNumber(NumRefs - NumPills)))
						.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					];
			}

			// Single-line, clipped message text.
			MessageRowBox->AddSlot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Node->Message))
					.Font(MessageFont)
					.ToolTipText(FText::FromString(Node->Message))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				];

			return MessageRowBox;
		}

		TSharedPtr<FArcLoreCommitNode> Node;
		FArcLoreGraphRowCell Cell;
		int32 NumLanes = 0;
		float LaneWidth = 16.0f;
		float RowHeight = 22.0f;
		float DotRadius = 4.0f;
	};
}

// ---------------------------------------------------------------------------------------------
// SArcLoreGraphRowCell
// ---------------------------------------------------------------------------------------------

void SArcLoreGraphRowCell::Construct(const FArguments& InArgs)
{
	Cell = InArgs._Cell;
	NumLanes = InArgs._NumLanes;
	LaneWidth = InArgs._LaneWidth;
	DotRadius = InArgs._DotRadius;
	RowHeight = InArgs._RowHeight;
}

int32 SArcLoreGraphRowCell::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	using namespace ArcLoreBranchGraph_Internal;

	const float RowH = AllottedGeometry.GetLocalSize().Y;
	const FPaintGeometry PaintGeom = AllottedGeometry.ToPaintGeometry();

	// --- Lane segments (edges) ---
	for (int32 SegIndex = 0; SegIndex < Cell.Segments.Num(); ++SegIndex)
	{
		const FArcLoreLaneSegment& Seg = Cell.Segments[SegIndex];

		const float TopX = LaneCenterX(Seg.InLane, LaneWidth);
		const float BotX = LaneCenterX(Seg.OutLane, LaneWidth);

		// Merge edges read differently via a slightly darkened tint (dashed lines aren't a
		// native Slate primitive — a tint difference is the chosen approximation).
		FLinearColor EdgeColor = Seg.bIsMerge ? (Seg.Color * 0.85f) : Seg.Color;

		if (Seg.bDanglingStub)
		{
			// Edge to an unfetched parent: draw only a short downward stub at reduced opacity.
			TArray<FVector2f> StubPoints;
			StubPoints.Add(FVector2f(TopX, 0.0f));
			StubPoints.Add(FVector2f(TopX, RowH * 0.5f));
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				PaintGeom,
				StubPoints,
				ESlateDrawEffect::None,
				EdgeColor.CopyWithNewOpacity(0.4f),
				true,
				EdgeThickness);
			continue;
		}

		if (Seg.InLane == Seg.OutLane)
		{
			// Straight vertical segment through this row.
			TArray<FVector2f> LinePoints;
			LinePoints.Add(FVector2f(TopX, 0.0f));
			LinePoints.Add(FVector2f(BotX, RowH));
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				PaintGeom,
				LinePoints,
				ESlateDrawEffect::None,
				EdgeColor,
				true,
				EdgeThickness);
		}
		else
		{
			// Lane change within this row: a vertical-tangent S-curve from top lane to bottom lane.
			FSlateDrawElement::MakeCubicBezierSpline(
				OutDrawElements,
				LayerId,
				PaintGeom,
				FVector2f(TopX, 0.0f),
				FVector2f(TopX, RowH * 0.5f),
				FVector2f(BotX, RowH * 0.5f),
				FVector2f(BotX, RowH),
				EdgeThickness,
				ESlateDrawEffect::None,
				EdgeColor);
		}
	}

	// --- Node dot (above the edges) ---
	if (Cell.bHasNode)
	{
		const float DotCenterX = LaneCenterX(Cell.NodeLane, LaneWidth);
		const FSlateBrush* DotBrush = GetDotBrush();
		if (DotBrush != nullptr)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(DotRadius * 2.0f, DotRadius * 2.0f),
					FSlateLayoutTransform(FVector2f(DotCenterX - DotRadius, RowH * 0.5f - DotRadius))),
				DotBrush,
				ESlateDrawEffect::None,
				Cell.NodeColor);
		}
	}

	return FMath::Max(LayerId, LayerId + 1);
}

FVector2D SArcLoreGraphRowCell::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(NumLanes * LaneWidth, RowHeight);
}

// ---------------------------------------------------------------------------------------------
// SArcLoreBranchGraph
// ---------------------------------------------------------------------------------------------

void SArcLoreBranchGraph::Construct(const FArguments& InArgs)
{
	ListView = SNew(SListView<TSharedPtr<FArcLoreCommitNode>>)
		.ListItemsSource(&RowItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SArcLoreBranchGraph::HandleGenerateRow)
		.OnSelectionChanged(this, &SArcLoreBranchGraph::HandleSelectionChanged)
		.OnContextMenuOpening(this, &SArcLoreBranchGraph::HandleContextMenuOpening)
		.HeaderRow(MakeGraphHeaderRow());

	ChildSlot
	[
		ListView.ToSharedRef()
	];
}

void SArcLoreBranchGraph::SetLayout(FArcLoreGraphLayout InLayout)
{
	Layout = MoveTemp(InLayout);

	// Build one shared node ptr per row (Nodes are already sorted by Row in BuildLayout).
	RowItems.Reset();
	RowItems.Reserve(Layout.Nodes.Num());
	for (int32 NodeIndex = 0; NodeIndex < Layout.Nodes.Num(); ++NodeIndex)
	{
		RowItems.Add(MakeShared<FArcLoreCommitNode>(Layout.Nodes[NodeIndex]));
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

TSharedRef<SHeaderRow> SArcLoreBranchGraph::MakeGraphHeaderRow() const
{
	using namespace ArcLoreBranchGraph_Internal;

	// The graph gutter is a fixed width = NumLanes * LaneWidth (at least one lane wide so the
	// column is visible even on an empty/linear graph).
	const float GraphWidth = FMath::Max(Layout.NumLanes, 1) * LaneWidth;

	return SNew(SHeaderRow)
		+ SHeaderRow::Column(ColumnGraph)
		.DefaultLabel(LOCTEXT("ColumnGraph", "Graph"))
		.FixedWidth(GraphWidth)

		+ SHeaderRow::Column(ColumnMessage)
		.DefaultLabel(LOCTEXT("ColumnMessage", "Message"))
		.FillWidth(0.45f)

		+ SHeaderRow::Column(ColumnAuthor)
		.DefaultLabel(LOCTEXT("ColumnAuthor", "Author"))
		.FillWidth(0.18f)

		+ SHeaderRow::Column(ColumnDate)
		.DefaultLabel(LOCTEXT("ColumnDate", "Date"))
		.FillWidth(0.22f)

		+ SHeaderRow::Column(ColumnRevision)
		.DefaultLabel(LOCTEXT("ColumnRevision", "Revision"))
		.FillWidth(0.15f);
}

TSharedRef<ITableRow> SArcLoreBranchGraph::HandleGenerateRow(
	TSharedPtr<FArcLoreCommitNode> InItem,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLoreBranchGraph_Internal;

	// Resolve this row's paint descriptor from the layout (indexed by Row). Pass a copy so the
	// graph cell is self-contained.
	FArcLoreGraphRowCell Cell;
	if (InItem.IsValid() && Layout.RowCells.IsValidIndex(InItem->Row))
	{
		Cell = Layout.RowCells[InItem->Row];
	}

	return SNew(SArcLoreCommitRow, OwnerTable)
		.Node(InItem)
		.Cell(Cell)
		.NumLanes(Layout.NumLanes)
		.LaneWidth(LaneWidth)
		.RowHeight(RowHeight)
		.DotRadius(DotRadius);
}

void SArcLoreBranchGraph::HandleSelectionChanged(TSharedPtr<FArcLoreCommitNode> InItem, ESelectInfo::Type InSelectInfo)
{
	OnCommitSelected.ExecuteIfBound(InItem);
}

TSharedPtr<SWidget> SArcLoreBranchGraph::HandleContextMenuOpening()
{
	if (OnContextMenuOpening.IsBound())
	{
		return OnContextMenuOpening.Execute();
	}
	return TSharedPtr<SWidget>();
}

#undef LOCTEXT_NAMESPACE
