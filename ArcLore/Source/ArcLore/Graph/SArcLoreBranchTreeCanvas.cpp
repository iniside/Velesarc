// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/SArcLoreBranchTreeCanvas.h"

#include "Fonts/FontMeasure.h"
#include "Layout/Geometry.h"
#include "Layout/SlateRect.h"
#include "Math/Box2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace ArcLoreBranchTreeCanvasInternal
{
	/** Fixed blob size in canvas space. */
	static const FVector2f NodeSize(160.0f, 36.0f);
	/** Rounded-corner radius for the blob brush. */
	static constexpr float NodeCornerRadius = 8.0f;
	/** Inset for the in-blob label, in canvas space. */
	static constexpr float LabelInset = 8.0f;

	/**
	 * Black or white, whichever reads better on the given blob fill color.
	 * Uses perceptual luminance so the label stays legible on bright fills
	 * (e.g. the current-branch yellow) and on dark palette colors alike.
	 */
	FORCEINLINE FLinearColor ContrastingTextColor(const FLinearColor& Fill)
	{
		const float Luminance = 0.299f * Fill.R + 0.587f * Fill.G + 0.114f * Fill.B;
		return Luminance > 0.55f ? FLinearColor::Black : FLinearColor::White;
	}

	/** canvas -> widget : WidgetPt = (CanvasPt + PanOffset) * ZoomFactor. */
	FORCEINLINE FVector2f CanvasToWidget(const FVector2f& CanvasPt, const FVector2f& PanOffset, float ZoomFactor)
	{
		return (CanvasPt + PanOffset) * ZoomFactor;
	}

	/** widget -> canvas : CanvasPt = LocalPt / ZoomFactor - PanOffset. */
	FORCEINLINE FVector2f WidgetToCanvas(const FVector2f& LocalPt, const FVector2f& PanOffset, float ZoomFactor)
	{
		return LocalPt / ZoomFactor - PanOffset;
	}
}

void SArcLoreBranchTreeCanvas::Construct(const FArguments& InArgs)
{
	using namespace ArcLoreBranchTreeCanvasInternal;

	// Neutral/white fill — per-node color is applied via the MakeBox tint, not the brush fill.
	NodeBrush = FSlateRoundedBoxBrush(
		FLinearColor::White,
		NodeCornerRadius,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.5f),
		1.0f,
		FVector2f(NodeSize.X, NodeSize.Y));

	// Current branch: brighter, thicker outline for emphasis.
	CurrentNodeBrush = FSlateRoundedBoxBrush(
		FLinearColor::White,
		NodeCornerRadius,
		FLinearColor::White,
		3.0f,
		FVector2f(NodeSize.X, NodeSize.Y));

	SetCanTick(false);
}

void SArcLoreBranchTreeCanvas::SetLayout(FArcLoreBranchTreeLayout InLayout)
{
	Layout = MoveTemp(InLayout);

	IdToIndex.Reset();
	IdToIndex.Reserve(Layout.Nodes.Num());
	for (int32 Index = 0; Index < Layout.Nodes.Num(); ++Index)
	{
		IdToIndex.Add(Layout.Nodes[Index].BranchId, Index);
	}

	if (SelectedIndex != INDEX_NONE && !Layout.Nodes.IsValidIndex(SelectedIndex))
	{
		SelectedIndex = INDEX_NONE;
	}

	bFramePending = (Layout.Nodes.Num() > 0);

	Invalidate(EInvalidateWidgetReason::Paint);
}

void SArcLoreBranchTreeCanvas::SetSelectedByTipHash(const FString& InTipHash)
{
	for (int32 Index = 0; Index < Layout.Nodes.Num(); ++Index)
	{
		if (Layout.Nodes[Index].TipHash == InTipHash)
		{
			SelectedIndex = Index;
			Invalidate(EInvalidateWidgetReason::Paint);
			return;
		}
	}
}

int32 SArcLoreBranchTreeCanvas::HitTestNode(const FVector2f& CanvasPt) const
{
	using namespace ArcLoreBranchTreeCanvasInternal;

	// Topmost wins: iterate in reverse so later-drawn nodes are hit first.
	for (int32 Index = Layout.Nodes.Num() - 1; Index >= 0; --Index)
	{
		const FArcLoreBranchTreeNode& Node = Layout.Nodes[Index];
		const FVector2f NodePos(static_cast<float>(Node.Pos.X), static_cast<float>(Node.Pos.Y));
		const FBox2f NodeBox(NodePos, NodePos + NodeSize);
		if (NodeBox.IsInside(CanvasPt))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 SArcLoreBranchTreeCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace ArcLoreBranchTreeCanvasInternal;

	// Auto-frame: on the first paint after SetLayout, fit all blobs into the viewport.
	const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
	if (bFramePending && Layout.Nodes.Num() > 0 && LocalSize.X > 8.0f && LocalSize.Y > 8.0f)
	{
		// Content bbox in canvas space (Pos = blob top-left; blob is NodeSize).
		FVector2f BBoxMin = FVector2f(static_cast<float>(Layout.Nodes[0].Pos.X), static_cast<float>(Layout.Nodes[0].Pos.Y));
		FVector2f BBoxMax = BBoxMin + NodeSize;
		for (const FArcLoreBranchTreeNode& N : Layout.Nodes)
		{
			const FVector2f NPos(static_cast<float>(N.Pos.X), static_cast<float>(N.Pos.Y));
			BBoxMin = FVector2f(FMath::Min(BBoxMin.X, NPos.X), FMath::Min(BBoxMin.Y, NPos.Y));
			BBoxMax = FVector2f(FMath::Max(BBoxMax.X, NPos.X + NodeSize.X), FMath::Max(BBoxMax.Y, NPos.Y + NodeSize.Y));
		}
		const float Pad = 40.0f;
		const FVector2f Content = (BBoxMax - BBoxMin) + FVector2f(Pad * 2.0f, Pad * 2.0f);
		const float FitZoom = FMath::Clamp(FMath::Min(LocalSize.X / FMath::Max(Content.X, 1.0f), LocalSize.Y / FMath::Max(Content.Y, 1.0f)), 0.1f, 1.0f);
		ZoomFactor = FitZoom;
		const FVector2f ContentCenter = (BBoxMin + BBoxMax) * 0.5f;
		PanOffset = (LocalSize * 0.5f) / ZoomFactor - ContentCenter;
		bFramePending = false;
	}

	const int32 EdgeLayer = LayerId;
	const int32 BlobLayer = LayerId + 1;
	const int32 TextLayer = LayerId + 2;

	const FPaintGeometry BasePaintGeometry = AllottedGeometry.ToPaintGeometry();

	// --- Edges (parent -> child cubic beziers) ---
	for (int32 ChildIndex = 0; ChildIndex < Layout.Nodes.Num(); ++ChildIndex)
	{
		const FArcLoreBranchTreeNode& Child = Layout.Nodes[ChildIndex];
		const int32* ParentIndexPtr = IdToIndex.Find(Child.ParentBranchId);
		if (ParentIndexPtr == nullptr || !Layout.Nodes.IsValidIndex(*ParentIndexPtr))
		{
			continue;
		}

		const FArcLoreBranchTreeNode& Parent = Layout.Nodes[*ParentIndexPtr];

		// Parent right-center, child left-center, in canvas space.
		const FVector2f ParentCanvas(static_cast<float>(Parent.Pos.X) + NodeSize.X, static_cast<float>(Parent.Pos.Y) + NodeSize.Y * 0.5f);
		const FVector2f ChildCanvas(static_cast<float>(Child.Pos.X), static_cast<float>(Child.Pos.Y) + NodeSize.Y * 0.5f);

		const FVector2f P0 = CanvasToWidget(ParentCanvas, PanOffset, ZoomFactor);
		const FVector2f P3 = CanvasToWidget(ChildCanvas, PanOffset, ZoomFactor);

		const float Pull = (P3.X - P0.X) * 0.5f;
		const FVector2f P1(P0.X + Pull, P0.Y);
		const FVector2f P2(P3.X - Pull, P3.Y);

		const float Thickness = (Child.bIsCurrent ? 3.0f : 1.5f) * ZoomFactor;

		FSlateDrawElement::MakeCubicBezierSpline(
			OutDrawElements,
			EdgeLayer,
			BasePaintGeometry,
			P0, P1, P2, P3,
			Thickness,
			ESlateDrawEffect::None,
			Child.Color);
	}

	// --- Blobs + labels ---
	const FVector2f SizeScaled = NodeSize * ZoomFactor;
	const int32 FontSize = FMath::RoundToInt(FMath::Clamp(10.0f * ZoomFactor, 6.0f, 18.0f));
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FontSize);

	for (int32 Index = 0; Index < Layout.Nodes.Num(); ++Index)
	{
		const FArcLoreBranchTreeNode& Node = Layout.Nodes[Index];
		const FVector2f NodeCanvas(static_cast<float>(Node.Pos.X), static_cast<float>(Node.Pos.Y));
		const FVector2f WidgetPos = CanvasToWidget(NodeCanvas, PanOffset, ZoomFactor);

		// Cull: skip nodes whose widget-space box does not intersect the culling rect.
		const FSlateRect NodeWidgetRect(WidgetPos.X, WidgetPos.Y, WidgetPos.X + SizeScaled.X, WidgetPos.Y + SizeScaled.Y);
		if (!FSlateRect::DoRectanglesIntersect(NodeWidgetRect, MyCullingRect))
		{
			continue;
		}

		const bool bSelected = (Index == SelectedIndex);
		const FSlateRoundedBoxBrush& Brush = Node.bIsCurrent ? CurrentNodeBrush : NodeBrush;

		// Selection outline: a slightly larger box drawn behind, brighter tint.
		if (bSelected)
		{
			const float SelInset = 3.0f * ZoomFactor;
			const FVector2f SelPos = WidgetPos - FVector2f(SelInset, SelInset);
			const FVector2f SelSize = SizeScaled + FVector2f(SelInset, SelInset) * 2.0f;
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				BlobLayer,
				AllottedGeometry.ToPaintGeometry(SelSize, FSlateLayoutTransform(ZoomFactor, SelPos)),
				&CurrentNodeBrush,
				ESlateDrawEffect::None,
				FLinearColor(1.0f, 0.85f, 0.2f, 1.0f));
		}

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			BlobLayer + 1,
			AllottedGeometry.ToPaintGeometry(SizeScaled, FSlateLayoutTransform(ZoomFactor, WidgetPos)),
			&Brush,
			ESlateDrawEffect::None,
			Node.Color);

		// Label inside the blob (small inset).
		const FVector2f LabelCanvas = NodeCanvas + FVector2f(LabelInset, NodeSize.Y * 0.5f - static_cast<float>(FontSize) * 0.5f);
		const FVector2f LabelWidget = CanvasToWidget(LabelCanvas, PanOffset, ZoomFactor);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			TextLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(NodeSize.X - LabelInset * 2.0f, static_cast<float>(FontSize) + 2.0f) * ZoomFactor, FSlateLayoutTransform(ZoomFactor, LabelWidget)),
			Node.BranchName,
			LabelFont,
			ESlateDrawEffect::None,
			ContrastingTextColor(Node.Color));

		// Sync badge — right-aligned inside the pill, drawn only when a remote exists or the branch is in sync.
		// Rule: bInSync=true (includes no-remote) -> green ✓; !bInSync + bAheadBehindKnown -> amber arrows;
		//       !bInSync + !bAheadBehindKnown -> amber ⇅.
		{
			FString BadgeText;
			FLinearColor BadgeColor;

			if (Node.bInSync)
			{
				BadgeText = TEXT("✓");
				BadgeColor = FLinearColor(0.4f, 0.85f, 0.4f, 1.0f);
			}
			else if (Node.bAheadBehindKnown)
			{
				BadgeText.Empty();
				if (Node.Ahead > 0)
				{
					BadgeText += FString::Printf(TEXT("↑%d"), Node.Ahead);
				}
				if (Node.Behind > 0)
				{
					if (!BadgeText.IsEmpty())
					{
						BadgeText += TEXT(" ");
					}
					BadgeText += FString::Printf(TEXT("↓%d"), Node.Behind);
				}
				if (BadgeText.IsEmpty())
				{
					// Ahead=0, Behind=0 but !bInSync — shouldn't happen, fall back gracefully.
					BadgeText = TEXT("⇅");
				}
				BadgeColor = FLinearColor(0.95f, 0.75f, 0.2f, 1.0f);
			}
			else
			{
				// No remote or out-of-sync without count.
				if (!Node.bHasRemote)
				{
					// No remote: show nothing (bInSync covers the in-sync case above).
					BadgeText.Empty();
				}
				else
				{
					BadgeText = TEXT("⇅");
					BadgeColor = FLinearColor(0.95f, 0.75f, 0.2f, 1.0f);
				}
			}

			if (!BadgeText.IsEmpty())
			{
				const int32 BadgeFontSize = FMath::RoundToInt(FMath::Clamp(8.0f * ZoomFactor, 6.0f, 12.0f));
				const FSlateFontInfo BadgeFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BadgeFontSize);

				// Estimate badge width: ~7 canvas units per character is conservative enough for short strings.
				const float BadgeWidthEstimate = static_cast<float>(BadgeText.Len()) * 7.0f;
				const float BadgeX = NodeCanvas.X + NodeSize.X - LabelInset - BadgeWidthEstimate;
				const float BadgeY = NodeCanvas.Y + NodeSize.Y * 0.5f - static_cast<float>(BadgeFontSize) * 0.5f;
				const FVector2f BadgeCanvas(BadgeX, BadgeY);
				const FVector2f BadgeWidget = CanvasToWidget(BadgeCanvas, PanOffset, ZoomFactor);

				FSlateDrawElement::MakeText(
					OutDrawElements,
					TextLayer,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(BadgeWidthEstimate + LabelInset, static_cast<float>(BadgeFontSize) + 2.0f) * ZoomFactor,
						FSlateLayoutTransform(ZoomFactor, BadgeWidget)),
					BadgeText,
					BadgeFont,
					ESlateDrawEffect::None,
					BadgeColor);
			}
		}
	}

	return TextLayer;
}

FVector2D SArcLoreBranchTreeCanvas::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	// Fixed modest minimum — pan/zoom drive navigation, not the desired size.
	return FVector2D(400.0, 300.0);
}

FReply SArcLoreBranchTreeCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	using namespace ArcLoreBranchTreeCanvasInternal;

	const FVector2f LocalPt = FVector2f(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	const FVector2f CanvasPt = WidgetToCanvas(LocalPt, PanOffset, ZoomFactor);
	const int32 HitIndex = HitTestNode(CanvasPt);

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (HitIndex != INDEX_NONE)
		{
			SelectedIndex = HitIndex;
			Invalidate(EInvalidateWidgetReason::Paint);
			OnBranchSelected.ExecuteIfBound(Layout.Nodes[HitIndex]);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (HitIndex != INDEX_NONE)
		{
			OnBranchContextMenu.ExecuteIfBound(Layout.Nodes[HitIndex]);
			return FReply::Handled();
		}
		// RMB on empty -> start pan.
		bIsPanning = true;
		LastMousePos = LocalPt;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bIsPanning = true;
		LastMousePos = LocalPt;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}

FReply SArcLoreBranchTreeCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bIsPanning && HasMouseCapture())
	{
		const FVector2f Cur = FVector2f(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
		PanOffset += (Cur - LastMousePos) / ZoomFactor;
		LastMousePos = Cur;
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SArcLoreBranchTreeCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FKey Button = MouseEvent.GetEffectingButton();
	if (bIsPanning && (Button == EKeys::RightMouseButton || Button == EKeys::MiddleMouseButton))
	{
		bIsPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

void SArcLoreBranchTreeCanvas::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bIsPanning = false;
}

FReply SArcLoreBranchTreeCanvas::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float OldZoom = ZoomFactor;
	ZoomFactor = FMath::Clamp(ZoomFactor * FMath::Pow(1.15f, MouseEvent.GetWheelDelta()), 0.1f, 8.0f);

	// Zoom toward the cursor: keep the canvas point under the cursor fixed in widget space.
	const FVector2f L = FVector2f(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	PanOffset += L / ZoomFactor - L / OldZoom;

	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}
