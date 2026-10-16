// Copyright Lukasz Baran. All Rights Reserved.

#include "ResponseCurve/SArcResponseCurvePreview.h"

#include "Math/ArcResponseCurve.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Styling/CoreStyle.h"

namespace ArcResponseCurvePreviewPrivate
{
	constexpr int32 MinSamples = 16;
	constexpr int32 MaxSamples = 256;
	constexpr int32 GridDivisions = 4;
	constexpr float LabelledInsetLeftPx = 26.0f;
	constexpr float LabelledInsetBottomPx = 16.0f;
	constexpr float PlotInsetPx = 4.0f;
	constexpr float LabelHeightPx = 14.0f;

	const FLinearColor BackgroundColor(0.020f, 0.020f, 0.024f, 1.0f);
	const FLinearColor GridColor(0.120f, 0.125f, 0.145f, 1.0f);
	const FLinearColor AxisColor(0.320f, 0.330f, 0.380f, 1.0f);
	const FLinearColor LabelColor(0.620f, 0.640f, 0.700f, 1.0f);
	const FLinearColor CurveColor(0.450f, 0.800f, 1.000f, 1.0f);
	const FLinearColor MarkerColor(1.000f, 0.620f, 0.180f, 1.0f);
	constexpr float MarkerDotSizePx = 5.0f;

	void DrawLabel(FSlateWindowElementList& OutDrawElements,
		const int32 Layer,
		const FGeometry& AllottedGeometry,
		const FSlateFontInfo& Font,
		const FString& Text,
		const FVector2f& Position,
		const float Width)
	{
		FSlateDrawElement::MakeText(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(FVector2f(Width, LabelHeightPx), FSlateLayoutTransform(Position)),
			Text, Font, ESlateDrawEffect::None, LabelColor);
	}

	struct FPlotMapping
	{
		float Left = 0.0f;
		float Top = 0.0f;
		float Width = 1.0f;
		float Height = 1.0f;
		float YMin = 0.0f;
		float YRange = 1.0f;

		FVector2f ToScreen(const float X, const float Y) const
		{
			return FVector2f(Left + X * Width, Top + (1.0f - (Y - YMin) / YRange) * Height);
		}
	};

	FString FormatValue(const float Value)
	{
		return FString::SanitizeFloat(FMath::RoundToFloat(Value * 100.0f) / 100.0f, 0);
	}
}

void SArcResponseCurvePreview::Construct(const FArguments& InArgs)
{
	OnGetCurve = InArgs._OnGetCurve;
	DesiredSize = InArgs._DesiredSize;
	bShowLabels = InArgs._bShowLabels;
	Marker = InArgs._Marker;
	ForceVolatile(true);
}

FVector2D SArcResponseCurvePreview::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return DesiredSize;
}

int32 SArcResponseCurvePreview::OnPaint(const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	using namespace ArcResponseCurvePreviewPrivate;

	const int32 BackgroundLayer = LayerId;
	const int32 GridLayer = LayerId + 1;
	const int32 CurveLayer = LayerId + 2;
	const int32 TextLayer = LayerId + 3;

	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	const FSlateFontInfo Font = FCoreStyle::Get().GetFontStyle("SmallFont");
	const FPaintGeometry BaseGeometry = AllottedGeometry.ToPaintGeometry();

	FSlateDrawElement::MakeBox(OutDrawElements, BackgroundLayer, BaseGeometry, Brush,
		ESlateDrawEffect::None, BackgroundColor);

	const FVector2f LocalSize = FVector2f(AllottedGeometry.GetLocalSize());
	const float InsetLeft = bShowLabels ? LabelledInsetLeftPx : PlotInsetPx;
	const float InsetBottom = bShowLabels ? LabelledInsetBottomPx : PlotInsetPx;
	const float PlotLeft = InsetLeft;
	const float PlotTop = PlotInsetPx;
	const float PlotWidth = LocalSize.X - InsetLeft - PlotInsetPx;
	const float PlotHeight = LocalSize.Y - InsetBottom - PlotInsetPx;
	if (PlotWidth <= 4.0f || PlotHeight <= 4.0f)
	{
		return TextLayer + 1;
	}

	const FArcResponseCurve* Curve = OnGetCurve.IsBound() ? OnGetCurve.Execute() : nullptr;

	const int32 SampleCount = FMath::Clamp(FMath::CeilToInt(PlotWidth * 0.5f), MinSamples, MaxSamples);
	float YMin = 0.0f;
	float YMax = 1.0f;
	SampleValues.Reset();
	if (Curve != nullptr)
	{
		SampleValues.Reserve(SampleCount + 1);
		for (int32 Index = 0; Index <= SampleCount; ++Index)
		{
			const float X = static_cast<float>(Index) / static_cast<float>(SampleCount);
			const float Y = Curve->Evaluate(X);
			SampleValues.Add(Y);
			if (FMath::IsFinite(Y))
			{
				YMin = FMath::Min(YMin, Y);
				YMax = FMath::Max(YMax, Y);
			}
		}
	}

	FPlotMapping Plot;
	Plot.Left = PlotLeft;
	Plot.Top = PlotTop;
	Plot.Width = PlotWidth;
	Plot.Height = PlotHeight;
	Plot.YMin = YMin;
	Plot.YRange = FMath::Max(YMax - YMin, UE_KINDA_SMALL_NUMBER);

	for (int32 Division = 0; Division <= GridDivisions; ++Division)
	{
		const float Value = static_cast<float>(Division) / static_cast<float>(GridDivisions);
		const bool bBorder = Division == 0 || Division == GridDivisions;
		const FLinearColor& Color = bBorder ? AxisColor : GridColor;

		Line.Reset();
		Line.Add(Plot.ToScreen(Value, YMin));
		Line.Add(Plot.ToScreen(Value, YMax));
		FSlateDrawElement::MakeLines(OutDrawElements, GridLayer, BaseGeometry, Line,
			ESlateDrawEffect::None, Color, false, 1.0f);

		Line.Reset();
		Line.Add(Plot.ToScreen(0.0f, Value));
		Line.Add(Plot.ToScreen(1.0f, Value));
		FSlateDrawElement::MakeLines(OutDrawElements, GridLayer, BaseGeometry, Line,
			ESlateDrawEffect::None, Color, false, 1.0f);
	}

	if (bShowLabels)
	{
		DrawLabel(OutDrawElements, TextLayer, AllottedGeometry, Font, TEXT("0"),
			FVector2f(PlotLeft - 3.0f, PlotTop + PlotHeight + 1.0f), 20.0f);
		DrawLabel(OutDrawElements, TextLayer, AllottedGeometry, Font, TEXT("1"),
			FVector2f(PlotLeft + PlotWidth - 6.0f, PlotTop + PlotHeight + 1.0f), 20.0f);
		DrawLabel(OutDrawElements, TextLayer, AllottedGeometry, Font, TEXT("x"),
			FVector2f(PlotLeft + PlotWidth * 0.5f - 3.0f, PlotTop + PlotHeight + 1.0f), 20.0f);

		TArray<float, TInlineAllocator<4>> YLabels = { 0.0f, 1.0f };
		if (YMin < 0.0f)
		{
			YLabels.Add(YMin);
		}
		if (YMax > 1.0f)
		{
			YLabels.Add(YMax);
		}
		for (const float Value : YLabels)
		{
			const FVector2f Position = Plot.ToScreen(0.0f, Value);
			DrawLabel(OutDrawElements, TextLayer, AllottedGeometry, Font, FormatValue(Value),
				FVector2f(2.0f, Position.Y - LabelHeightPx * 0.5f), InsetLeft - 4.0f);
		}
	}

	if (SampleValues.Num() < 2)
	{
		return TextLayer + 1;
	}

	Line.Reset();
	for (int32 Index = 0; Index < SampleValues.Num(); ++Index)
	{
		const float Y = SampleValues[Index];
		if (!FMath::IsFinite(Y))
		{
			if (Line.Num() >= 2)
			{
				FSlateDrawElement::MakeLines(OutDrawElements, CurveLayer, BaseGeometry, Line,
					ESlateDrawEffect::None, CurveColor, true, bShowLabels ? 2.0f : 1.5f);
			}
			Line.Reset();
			continue;
		}

		const float X = static_cast<float>(Index) / static_cast<float>(SampleValues.Num() - 1);
		Line.Add(Plot.ToScreen(X, Y));
	}

	if (Line.Num() >= 2)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, CurveLayer, BaseGeometry, Line,
			ESlateDrawEffect::None, CurveColor, true, bShowLabels ? 2.0f : 1.5f);
	}

	const TOptional<float> MarkerValue = Marker.Get();
	if (MarkerValue.IsSet() && Curve != nullptr && FMath::IsFinite(MarkerValue.GetValue()))
	{
		const float MarkerX = FMath::Clamp(MarkerValue.GetValue(), 0.0f, 1.0f);
		Line.Reset();
		Line.Add(Plot.ToScreen(MarkerX, YMin));
		Line.Add(Plot.ToScreen(MarkerX, YMax));
		FSlateDrawElement::MakeLines(OutDrawElements, CurveLayer, BaseGeometry, Line,
			ESlateDrawEffect::None, MarkerColor, true, 1.0f);

		const float MarkerY = Curve->Evaluate(MarkerX);
		if (FMath::IsFinite(MarkerY))
		{
			const FVector2f DotCenter = Plot.ToScreen(MarkerX, MarkerY);
			const FVector2f DotSize(MarkerDotSizePx, MarkerDotSizePx);
			FSlateDrawElement::MakeBox(OutDrawElements, TextLayer,
				AllottedGeometry.ToPaintGeometry(DotSize, FSlateLayoutTransform(DotCenter - DotSize * 0.5f)),
				Brush, ESlateDrawEffect::None, MarkerColor);
		}
	}

	return TextLayer + 1;
}
